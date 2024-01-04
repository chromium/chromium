// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/kcer/kcer_token_impl.h"

#include "base/hash/sha1.h"
#include "base/logging.h"
#include "chromeos/components/kcer/attributes.pb.h"
#include "chromeos/components/kcer/chaps/high_level_chaps_client.h"
#include "chromeos/components/kcer/chaps/session_chaps_client.h"
#include "chromeos/constants/pkcs11_definitions.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/openssl_util.h"
#include "net/cert/cert_database.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"
#include "third_party/cros_system_api/dbus/chaps/dbus-constants.h"

using AttributeId = kcer::HighLevelChapsClient::AttributeId;

namespace kcer::internal {
namespace {

void AddAttribute(chaps::AttributeList& attr_list,
                  chromeos::PKCS11_CK_ATTRIBUTE_TYPE type,
                  base::span<const uint8_t> data) {
  chaps::Attribute* new_attr = attr_list.add_attributes();
  new_attr->set_type(type);
  new_attr->set_value(std::string(data.begin(), data.end()));
  new_attr->set_length(data.size());
}

// `T` must be a simple type, i.e. no internal pointers, etc.
// `value` must outlive the returned span.
template <typename T>
base::span<const uint8_t> MakeSpan(T* value) {
  return base::as_bytes(base::span<T>(value, /*count=*/1u));
}

base::span<const uint8_t> GetAttributeValue(
    const chaps::AttributeList& attr_list,
    AttributeId attribute_id) {
  for (int i = 0; i < attr_list.attributes_size(); i++) {
    if (attr_list.attributes(i).type() != static_cast<uint32_t>(attribute_id)) {
      continue;
    }

    const chaps::Attribute& attr = attr_list.attributes(i);
    if (!attr.has_value()) {
      return {};
    }

    return base::as_bytes(base::make_span(attr.value()));
  }
  return {};
}

// The result should be the same as the one from NSS for backwards compatibility
// (at least until it's removed).
Pkcs11Id MakePkcs11Id(base::span<const uint8_t> public_key_data) {
  if (public_key_data.size() <= base::kSHA1Length) {
    return Pkcs11Id(
        std::vector<uint8_t>(public_key_data.begin(), public_key_data.end()));
  }

  base::SHA1Digest hash = base::SHA1HashSpan(public_key_data);
  return Pkcs11Id(std::vector<uint8_t>(hash.begin(), hash.end()));
}

Pkcs11Id MakePkcs11IdRsa(bssl::UniquePtr<RSA> rsa) {
  const BIGNUM* modulus = RSA_get0_n(rsa.get());
  if (!modulus) {
    LOG(ERROR) << "Could not parse RSA public key";
    return {};
  }

  std::vector<uint8_t> modulus_bytes(BN_num_bytes(modulus));
  // BN_bn2bin returns an absolute value of `modulus`, but according to RFC 8017
  // Section 3.1 the RSA modulus is a positive integer.
  BN_bn2bin(modulus, modulus_bytes.data());

  return MakePkcs11Id(modulus_bytes);
}

// Calculates PKCS#11 id for the provided public key SPKI.
Pkcs11Id GetPkcs11IdFromSpki(const PublicKeySpki& public_key_spki) {
  if (public_key_spki->empty()) {
    LOG(ERROR) << "Empty public key provided";
    return {};
  }

  const std::vector<uint8_t>& spki = public_key_spki.value();
  CBS cbs;
  CBS_init(&cbs, reinterpret_cast<const uint8_t*>(spki.data()), spki.size());
  bssl::UniquePtr<EVP_PKEY> evp_key(EVP_parse_public_key(&cbs));
  if (!evp_key || CBS_len(&cbs) != 0) {
    LOG(ERROR) << "Could not parse public key";
    return {};
  }

  bssl::UniquePtr<RSA> rsa(EVP_PKEY_get1_RSA(evp_key.get()));
  if (rsa) {
    return MakePkcs11IdRsa(std::move(rsa));
  }

  // TODO(244409232): Implement for EC keys when GenerateEcKey is implemented.
  return {};
}

// Returns true if the `key` already had PKCS#11 id or it was successfully set.
// Returns false if the `key` still doesn't have the id after the method
// finishes.
bool EnsurePkcs11IdIsSet(PrivateKeyHandle& key) {
  if (!key.GetPkcs11IdInternal()->empty()) {
    return true;
  }

  key.SetPkcs11IdInternal(GetPkcs11IdFromSpki(key.GetSpkiInternal()));
  return !key.GetPkcs11IdInternal()->empty();
}

PublicKeySpki MakeRsaSpki(const base::span<const uint8_t>& modulus,
                          const base::span<const uint8_t>& exponent) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  bssl::UniquePtr<BIGNUM> modulus_bignum(
      BN_bin2bn(modulus.data(), modulus.size(), nullptr));
  bssl::UniquePtr<BIGNUM> exponent_bignum(
      BN_bin2bn(exponent.data(), exponent.size(), nullptr));
  if (!modulus_bignum || !exponent_bignum) {
    return {};
  }

  bssl::UniquePtr<RSA> rsa(
      RSA_new_public_key(modulus_bignum.get(), exponent_bignum.get()));
  if (!rsa) {
    return {};
  }

  bssl::UniquePtr<EVP_PKEY> ssl_public_key(EVP_PKEY_new());
  if (!ssl_public_key || !EVP_PKEY_set1_RSA(ssl_public_key.get(), rsa.get())) {
    return {};
  }

  bssl::ScopedCBB cbb;
  uint8_t* der = nullptr;
  size_t der_len = 0;
  if (!CBB_init(cbb.get(), 0) ||
      !EVP_marshal_public_key(cbb.get(), ssl_public_key.get()) ||
      !CBB_finish(cbb.get(), &der, &der_len)) {
    return {};
  }
  bssl::UniquePtr<uint8_t> der_deleter(der);

  return PublicKeySpki(std::vector<uint8_t>(der, der + der_len));
}

void RunClosure(base::OnceClosure closure, uint32_t /*result_code*/) {
  std::move(closure).Run();
}
// A helper method for error handling. When some method fails and should return
// the `error` through the `callback`, but also should clean up something first,
// this helper allows to bind the error to the callback and create a new
// callback for the clean up code.
template <typename T>
base::OnceCallback<void(uint32_t)> Bind(
    base::OnceCallback<void(base::expected<T, Error>)> callback,
    Error error) {
  return base::BindOnce(&RunClosure, base::BindOnce(std::move(callback),
                                                    base::unexpected(error)));
}

}  // namespace

KcerTokenImpl::KcerTokenImpl(Token token, HighLevelChapsClient* chaps_client)
    : token_(token), pkcs_11_slot_id_(0), chaps_client_(chaps_client) {
  CHECK(chaps_client_);
}
KcerTokenImpl::~KcerTokenImpl() = default;

// Returns a weak pointer for the token. The pointer can be used to post tasks
// for the token.
base::WeakPtr<KcerToken> KcerTokenImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

// Initializes the token with the provided NSS slot. If `nss_slot` is nullptr,
// the initialization is considered failed and the token will return an error
// for all queued and future requests.
void KcerTokenImpl::InitializeWithoutNss(
    SessionChapsClient::SlotId pkcs11_slot_id) {
  pkcs_11_slot_id_ = pkcs11_slot_id;
  // This is supposed to be the first time the task queue is unblocked, no
  // other tasks should be already running.
  UnblockQueueProcessNextTask();
}

//==============================================================================

KcerTokenImpl::GenerateRsaKeyTask::GenerateRsaKeyTask(
    RsaModulusLength in_modulus_length_bits,
    bool in_hardware_backed,
    Kcer::GenerateKeyCallback in_callback)
    : modulus_length_bits(in_modulus_length_bits),
      hardware_backed(in_hardware_backed),
      callback(std::move(in_callback)) {}
KcerTokenImpl::GenerateRsaKeyTask::GenerateRsaKeyTask(
    GenerateRsaKeyTask&& other) = default;
KcerTokenImpl::GenerateRsaKeyTask::~GenerateRsaKeyTask() = default;

void KcerTokenImpl::GenerateRsaKey(RsaModulusLength modulus_length_bits,
                                   bool hardware_backed,
                                   Kcer::GenerateKeyCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(
        &KcerTokenImpl::GenerateRsaKey, weak_factory_.GetWeakPtr(),
        modulus_length_bits, hardware_backed, std::move(callback)));
  }

  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = BlockQueueGetUnblocker(std::move(callback));

  GenerateRsaKeyImpl(GenerateRsaKeyTask(modulus_length_bits, hardware_backed,
                                        std::move(unblocking_callback)));
}

// Generates a new key pair.
void KcerTokenImpl::GenerateRsaKeyImpl(GenerateRsaKeyTask task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  task.attemps_left--;
  if (task.attemps_left < 0) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kPkcs11SessionFailure));
  }

  chromeos::PKCS11_CK_BBOOL kTrue = chromeos::PKCS11_CK_TRUE;
  chromeos::PKCS11_CK_ULONG modulus_bits =
      static_cast<uint32_t>(task.modulus_length_bits);
  chromeos::PKCS11_CK_BYTE public_exponent[3] = {0x01, 0x00, 0x01};  // 65537

  chaps::AttributeList public_key_attrs;
  AddAttribute(public_key_attrs, chromeos::PKCS11_CKA_ENCRYPT,
               MakeSpan(&kTrue));
  AddAttribute(public_key_attrs, chromeos::PKCS11_CKA_VERIFY, MakeSpan(&kTrue));
  AddAttribute(public_key_attrs, chromeos::PKCS11_CKA_WRAP, MakeSpan(&kTrue));
  AddAttribute(public_key_attrs, chromeos::PKCS11_CKA_MODULUS_BITS,
               MakeSpan(&modulus_bits));
  AddAttribute(public_key_attrs, chromeos::PKCS11_CKA_PUBLIC_EXPONENT,
               MakeSpan(&public_exponent));

  chaps::AttributeList private_key_attrs;
  AddAttribute(private_key_attrs, chromeos::PKCS11_CKA_TOKEN, MakeSpan(&kTrue));
  AddAttribute(private_key_attrs, chromeos::PKCS11_CKA_PRIVATE,
               MakeSpan(&kTrue));
  AddAttribute(private_key_attrs, chromeos::PKCS11_CKA_SENSITIVE,
               MakeSpan(&kTrue));
  AddAttribute(private_key_attrs, chromeos::PKCS11_CKA_DECRYPT,
               MakeSpan(&kTrue));
  AddAttribute(private_key_attrs, chromeos::PKCS11_CKA_SIGN, MakeSpan(&kTrue));
  AddAttribute(private_key_attrs, chromeos::PKCS11_CKA_UNWRAP,
               MakeSpan(&kTrue));

  if (!task.hardware_backed) {
    AddAttribute(private_key_attrs, chaps::kForceSoftwareAttribute,
                 MakeSpan(&kTrue));
  }

  auto chaps_callback =
      base::BindOnce(&KcerTokenImpl::DidGenerateRsaKey,
                     weak_factory_.GetWeakPtr(), std::move(task));

  chaps_client_->GenerateKeyPair(
      pkcs_11_slot_id_, chromeos::PKCS11_CKM_RSA_PKCS_KEY_PAIR_GEN,
      /*mechanism_parameter=*/{}, std::move(public_key_attrs),
      std::move(private_key_attrs), std::move(chaps_callback));
}

// Fetches the public key attributes of the generated key.
void KcerTokenImpl::DidGenerateRsaKey(GenerateRsaKeyTask task,
                                      ObjectHandle public_key_id,
                                      ObjectHandle private_key_id,
                                      uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return GenerateRsaKeyImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToGenerateKey));
  }

  chaps_client_->GetAttributeValue(
      pkcs_11_slot_id_, public_key_id,
      {AttributeId::kModulus, AttributeId::kPublicExponent},
      base::BindOnce(&KcerTokenImpl::DidGetRsaPublicKey,
                     weak_factory_.GetWeakPtr(), std::move(task), public_key_id,
                     private_key_id));
}

// Computes PKCS#11 for the key and sets it.
void KcerTokenImpl::DidGetRsaPublicKey(
    GenerateRsaKeyTask task,
    ObjectHandle public_key_id,
    ObjectHandle private_key_id,
    chaps::AttributeList public_key_attributes,
    uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return GenerateRsaKeyImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return chaps_client_->DestroyObjectsWithRetries(
        pkcs_11_slot_id_, {public_key_id, private_key_id},
        Bind(std::move(task.callback), Error::kFailedToExportPublicKey));
  }

  base::span<const uint8_t> modulus =
      GetAttributeValue(public_key_attributes, AttributeId::kModulus);
  base::span<const uint8_t> public_exponent =
      GetAttributeValue(public_key_attributes, AttributeId::kPublicExponent);

  if (modulus.empty() || public_exponent.empty()) {
    return chaps_client_->DestroyObjectsWithRetries(
        pkcs_11_slot_id_, {public_key_id, private_key_id},
        Bind(std::move(task.callback), Error::kFailedToReadAttribute));
  }

  PublicKeySpki spki = MakeRsaSpki(modulus, public_exponent);
  if (spki->empty()) {
    return chaps_client_->DestroyObjectsWithRetries(
        pkcs_11_slot_id_, {public_key_id, private_key_id},
        Bind(std::move(task.callback), Error::kFailedToCreateSpki));
  }

  Pkcs11Id pkcs11_id = MakePkcs11Id(modulus);

  PublicKey kcer_public_key(token_, pkcs11_id, std::move(spki));

  chaps::AttributeList attr_list;
  AddAttribute(attr_list, chromeos::PKCS11_CKA_ID, pkcs11_id.value());

  auto chaps_callback =
      base::BindOnce(&KcerTokenImpl::DidAssignRsaKeyId,
                     weak_factory_.GetWeakPtr(), std::move(task), public_key_id,
                     private_key_id, std::move(kcer_public_key));
  chaps_client_->SetAttributeValue(pkcs_11_slot_id_,
                                   {public_key_id, private_key_id}, attr_list,
                                   std::move(chaps_callback));
}

void KcerTokenImpl::DidAssignRsaKeyId(GenerateRsaKeyTask task,
                                      ObjectHandle public_key_id,
                                      ObjectHandle private_key_id,
                                      PublicKey kcer_public_key,
                                      uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return GenerateRsaKeyImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return chaps_client_->DestroyObjectsWithRetries(
        pkcs_11_slot_id_, {public_key_id, private_key_id},
        Bind(std::move(task.callback), Error::kFailedToWriteAttribute));
  }

  return std::move(task.callback).Run(std::move(kcer_public_key));
}

//==============================================================================

void KcerTokenImpl::GenerateEcKey(EllipticCurve curve,
                                  bool hardware_backed,
                                  Kcer::GenerateKeyCallback callback) {
  // TODO(244409232): Implement.
}

//==============================================================================

void KcerTokenImpl::ImportKey(Pkcs8PrivateKeyInfoDer pkcs8_private_key_info_der,
                              Kcer::ImportKeyCallback callback) {
  // TODO(244409232): Implement.
}

//==============================================================================

void KcerTokenImpl::ImportCertFromBytes(CertDer cert_der,
                                        Kcer::StatusCallback callback) {
  // TODO(244409232): Implement.
}

//==============================================================================

void KcerTokenImpl::ImportPkcs12Cert(Pkcs12Blob pkcs12_blob,
                                     std::string password,
                                     bool hardware_backed,
                                     Kcer::StatusCallback callback) {
  // TODO(244409232): Implement.
}

//==============================================================================

void KcerTokenImpl::ExportPkcs12Cert(scoped_refptr<const Cert> cert,
                                     Kcer::ExportPkcs12Callback callback) {
  // TODO(244409232): Implement.
}

//==============================================================================

KcerTokenImpl::RemoveKeyAndCertsTask::RemoveKeyAndCertsTask(
    PrivateKeyHandle in_key,
    Kcer::StatusCallback in_callback)
    : key(std::move(in_key)), callback(std::move(in_callback)) {}
KcerTokenImpl::RemoveKeyAndCertsTask::RemoveKeyAndCertsTask(
    RemoveKeyAndCertsTask&& other) = default;
KcerTokenImpl::RemoveKeyAndCertsTask::~RemoveKeyAndCertsTask() = default;

void KcerTokenImpl::RemoveKeyAndCerts(PrivateKeyHandle key,
                                      Kcer::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(
        &KcerTokenImpl::RemoveKeyAndCerts, weak_factory_.GetWeakPtr(),
        std::move(key), std::move(callback)));
  }

  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = BlockQueueGetUnblocker(std::move(callback));

  if (!EnsurePkcs11IdIsSet(key)) {
    return std::move(unblocking_callback)
        .Run(base::unexpected(Error::kFailedToGetPkcs11Id));
  }

  RemoveKeyAndCertsImpl(
      RemoveKeyAndCertsTask(std::move(key), std::move(unblocking_callback)));
}

// Finds all objects related to the `task.key` by PKCS#11 id.
void KcerTokenImpl::RemoveKeyAndCertsImpl(RemoveKeyAndCertsTask task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  task.attemps_left--;
  if (task.attemps_left < 0) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kPkcs11SessionFailure));
  }

  chaps::AttributeList attributes;
  AddAttribute(attributes, chromeos::PKCS11_CKA_ID,
               task.key.GetPkcs11IdInternal().value());

  chaps_client_->FindObjects(
      pkcs_11_slot_id_, std::move(attributes),
      base::BindOnce(&KcerTokenImpl::RemoveKeyAndCertsWithObjectHandles,
                     weak_factory_.GetWeakPtr(), std::move(task)));
}

// Destroys all found objects.
void KcerTokenImpl::RemoveKeyAndCertsWithObjectHandles(
    RemoveKeyAndCertsTask task,
    std::vector<ObjectHandle> handles,
    uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return RemoveKeyAndCertsImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToSearchForObjects));
  }

  chaps_client_->DestroyObjectsWithRetries(
      pkcs_11_slot_id_, std::move(handles),
      base::BindOnce(&KcerTokenImpl::DidRemoveKeyAndCerts,
                     weak_factory_.GetWeakPtr(), std::move(task)));
}

// Checks the result and notifies that some certs were changed.
void KcerTokenImpl::DidRemoveKeyAndCerts(RemoveKeyAndCertsTask task,
                                         uint32_t result_code) {
  base::expected<void, Error> result;
  if (SessionChapsClient::IsSessionError(result_code)) {
    return RemoveKeyAndCertsImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    result = base::unexpected(Error::kFailedToRemoveObjects);
  }
  // Even if `DestroyObjectsWithRetries` fails, it might have removed at
  // least some objects, so notify about possible changes.
  NotifyCertsChanged(
      base::BindOnce(std::move(task.callback), std::move(result)));
}

//==============================================================================

void KcerTokenImpl::RemoveCert(scoped_refptr<const Cert> cert,
                               Kcer::StatusCallback callback) {
  // TODO(244409232): Implement.
}

//==============================================================================

void KcerTokenImpl::ListKeys(TokenListKeysCallback callback) {
  // TODO(244409232): Implement.
}

//==============================================================================

void KcerTokenImpl::ListCerts(TokenListCertsCallback callback) {
  // TODO(244409232): Implement.
}

//==============================================================================

KcerTokenImpl::DoesPrivateKeyExistTask::DoesPrivateKeyExistTask(
    PrivateKeyHandle in_key,
    Kcer::DoesKeyExistCallback in_callback)
    : key(std::move(in_key)), callback(std::move(in_callback)) {}
KcerTokenImpl::DoesPrivateKeyExistTask::DoesPrivateKeyExistTask(
    DoesPrivateKeyExistTask&& other) = default;
KcerTokenImpl::DoesPrivateKeyExistTask::~DoesPrivateKeyExistTask() = default;

void KcerTokenImpl::DoesPrivateKeyExist(PrivateKeyHandle key,
                                        Kcer::DoesKeyExistCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(
        &KcerTokenImpl::DoesPrivateKeyExist, weak_factory_.GetWeakPtr(),
        std::move(key), std::move(callback)));
  }

  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = BlockQueueGetUnblocker(std::move(callback));

  if (!EnsurePkcs11IdIsSet(key)) {
    return std::move(unblocking_callback)
        .Run(base::unexpected(Error::kFailedToGetPkcs11Id));
  }

  DoesPrivateKeyExistImpl(
      DoesPrivateKeyExistTask(std::move(key), std::move(unblocking_callback)));
}

// Searches for the Chaps handle for `task.key`.
void KcerTokenImpl::DoesPrivateKeyExistImpl(DoesPrivateKeyExistTask task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  task.attemps_left--;
  if (task.attemps_left < 0) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kPkcs11SessionFailure));
  }

  chromeos::PKCS11_CK_OBJECT_CLASS priv_key_class =
      chromeos::PKCS11_CKO_PRIVATE_KEY;
  chaps::AttributeList private_key_attrs;
  AddAttribute(private_key_attrs, chromeos::PKCS11_CKA_CLASS,
               MakeSpan(&priv_key_class));
  AddAttribute(private_key_attrs, chromeos::PKCS11_CKA_ID,
               task.key.GetPkcs11IdInternal().value());

  chaps_client_->FindObjects(
      pkcs_11_slot_id_, std::move(private_key_attrs),
      base::BindOnce(&KcerTokenImpl::DidDoesPrivateKeyExist,
                     weak_factory_.GetWeakPtr(), std::move(task)));
}

void KcerTokenImpl::DidDoesPrivateKeyExist(
    DoesPrivateKeyExistTask task,
    std::vector<ObjectHandle> object_list,
    uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return DoesPrivateKeyExistImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToSearchForObjects));
  }

  return std::move(task.callback).Run(!object_list.empty());
}

//==============================================================================

void KcerTokenImpl::Sign(PrivateKeyHandle key,
                         SigningScheme signing_scheme,
                         DataToSign data,
                         Kcer::SignCallback callback) {
  // TODO(244409232): Implement.
}

//==============================================================================

void KcerTokenImpl::SignRsaPkcs1Raw(PrivateKeyHandle key,
                                    DigestWithPrefix digest_with_prefix,
                                    Kcer::SignCallback callback) {
  // TODO(244409232): Implement.
}

//==============================================================================

void KcerTokenImpl::GetTokenInfo(Kcer::GetTokenInfoCallback callback) {
  // TODO(244409232): Implement.
}

//==============================================================================

void KcerTokenImpl::GetKeyInfo(PrivateKeyHandle key,
                               Kcer::GetKeyInfoCallback callback) {
  // TODO(244409232): Implement.
}

//==============================================================================

void KcerTokenImpl::SetKeyNickname(PrivateKeyHandle key,
                                   std::string nickname,
                                   Kcer::StatusCallback callback) {
  // TODO(244409232): Implement.
}

//==============================================================================

void KcerTokenImpl::SetKeyPermissions(PrivateKeyHandle key,
                                      chaps::KeyPermissions key_permissions,
                                      Kcer::StatusCallback callback) {
  // TODO(244409232): Implement.
}

//==============================================================================

void KcerTokenImpl::SetCertProvisioningProfileId(
    PrivateKeyHandle key,
    std::string profile_id,
    Kcer::StatusCallback callback) {
  // TODO(244409232): Implement.
}

//==============================================================================

void KcerTokenImpl::NotifyCertsChanged(base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  net::CertDatabase::GetInstance()->NotifyObserversClientCertStoreChanged();
  // The Notify... above will post a task to invalidate the cache. Calling the
  // original callback for a request will automatically trigger updating cache
  // and executing the next request. Post a task with the original callback
  // (instead of calling it synchronously), so the cache update and the next
  // request happen after the notification.
  content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(callback));
}

template <typename... Args>
void RunUnblockerAndCallback(base::ScopedClosureRunner unblocker,
                             base::OnceCallback<void(Args...)> callback,
                             Args... args) {
  unblocker.RunAndReset();
  std::move(callback).Run(args...);
}

template <typename... Args>
base::OnceCallback<void(Args...)> KcerTokenImpl::BlockQueueGetUnblocker(
    base::OnceCallback<void(Args...)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  CHECK(!is_blocked_);
  is_blocked_ = true;

  // `unblocker` is executed either manually or on destruction.
  base::ScopedClosureRunner unblocker(base::BindOnce(
      &KcerTokenImpl::UnblockQueueProcessNextTask, weak_factory_.GetWeakPtr()));
  return base::BindOnce(&RunUnblockerAndCallback<Args...>, std::move(unblocker),
                        std::move(callback));
}

void KcerTokenImpl::UnblockQueueProcessNextTask() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  is_blocked_ = false;

  if (task_queue_.empty()) {
    return;
  }

  base::OnceClosure next_task = std::move(task_queue_.front());
  task_queue_.pop_front();
  std::move(next_task).Run();
}

}  // namespace kcer::internal
