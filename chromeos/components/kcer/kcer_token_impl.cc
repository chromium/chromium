// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/kcer/kcer_token_impl.h"

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "chromeos/components/kcer/attributes.pb.h"
#include "chromeos/components/kcer/chaps/high_level_chaps_client.h"
#include "chromeos/components/kcer/chaps/session_chaps_client.h"
#include "chromeos/constants/pkcs11_definitions.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/openssl_util.h"
#include "net/cert/cert_database.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
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

// Chaps wraps the EC point in a DER-encoded ASN.1 OctetString, which is
// required by PKCS#11 standard, but it needs to be removed before using it for
// boringssl.
bssl::UniquePtr<ASN1_OCTET_STRING> UnwrapEcPoint(
    base::span<const uint8_t> ec_point) {
  if (ec_point.empty()) {
    return {};
  }

  const uint8_t* data = ec_point.data();
  bssl::UniquePtr<ASN1_OCTET_STRING> result(
      d2i_ASN1_OCTET_STRING(nullptr, &data, ec_point.size()));
  return result;
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

// Backwards compatible with how NSS generated CKA_ID for RSA keys.
Pkcs11Id MakePkcs11IdFromRsaKey(bssl::UniquePtr<RSA> rsa_key) {
  const BIGNUM* modulus = RSA_get0_n(rsa_key.get());
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

// Backwards compatible with how NSS generated CKA_ID for EC keys.
Pkcs11Id MakePkcs11IdFromEcKey(bssl::UniquePtr<EC_KEY> ec_key) {
  const EC_POINT* point = EC_KEY_get0_public_key(ec_key.get());
  const EC_GROUP* group = EC_KEY_get0_group(ec_key.get());

  if (!point || !group) {
    LOG(ERROR) << "Could not parse EC public key";
    return {};
  }

  // Serialize the public key as an uncompressed point in X9.62 form.
  bssl::ScopedCBB cbb;
  uint8_t* point_bytes = nullptr;
  size_t point_bytes_len = 0;
  if (!CBB_init(cbb.get(), 0) ||
      !EC_POINT_point2cbb(
          cbb.get(), group, point,
          point_conversion_form_t::POINT_CONVERSION_UNCOMPRESSED,
          /*ctx=*/nullptr) ||
      !CBB_finish(cbb.get(), &point_bytes, &point_bytes_len)) {
    return {};
  }
  bssl::UniquePtr<uint8_t> point_bytes_deleter(point_bytes);

  return MakePkcs11Id(base::make_span(point_bytes, point_bytes_len));
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

  if (EVP_PKEY_base_id(evp_key.get()) == EVP_PKEY_RSA) {
    bssl::UniquePtr<RSA> rsa_key(EVP_PKEY_get1_RSA(evp_key.get()));
    if (!rsa_key) {
      return {};
    }
    return MakePkcs11IdFromRsaKey(std::move(rsa_key));
  }

  if (EVP_PKEY_base_id(evp_key.get()) == EVP_PKEY_EC) {
    bssl::UniquePtr<EC_KEY> ec_key(EVP_PKEY_get1_EC_KEY(evp_key.get()));
    if (!ec_key) {
      return {};
    }
    return MakePkcs11IdFromEcKey(std::move(ec_key));
  }

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

PublicKeySpki MakeEcSpki(const base::span<const uint8_t>& ec_point) {
  bssl::UniquePtr<EC_KEY> ec(EC_KEY_new());
  if (!ec) {
    return {};
  }

  if (!EC_KEY_set_group(ec.get(), EC_group_p256())) {
    return {};
  }

  EC_KEY* ec_ptr = ec.get();
  const uint8_t* data_2 = ec_point.data();
  size_t data_2_len = ec_point.size();
  if (!o2i_ECPublicKey(&ec_ptr, &data_2, data_2_len)) {
    return {};
  }

  bssl::UniquePtr<EVP_PKEY> ssl_public_key(EVP_PKEY_new());
  if (!ssl_public_key ||
      !EVP_PKEY_set1_EC_KEY(ssl_public_key.get(), ec.get())) {
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

uint64_t SigningSchemeToPkcs11Mechanism(SigningScheme scheme) {
  switch (scheme) {
    case SigningScheme::kRsaPkcs1Sha1:
    case SigningScheme::kRsaPkcs1Sha256:
    case SigningScheme::kRsaPkcs1Sha384:
    case SigningScheme::kRsaPkcs1Sha512:
      return chromeos::PKCS11_CKM_RSA_PKCS;
    case SigningScheme::kEcdsaSecp256r1Sha256:
    case SigningScheme::kEcdsaSecp384r1Sha384:
    case SigningScheme::kEcdsaSecp521r1Sha512:
      return chromeos::PKCS11_CKM_ECDSA;
    case SigningScheme::kRsaPssRsaeSha256:
    case SigningScheme::kRsaPssRsaeSha384:
    case SigningScheme::kRsaPssRsaeSha512:
      return chromeos::PKCS11_CKM_RSA_PKCS_PSS;
  }
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

// Creates a digest for `data_to_sign` with the correct prefix (if needed) for
// `kcer_signing_scheme`.
base::expected<DigestWithPrefix, Error> DigestOnWorkerThread(
    SigningScheme kcer_signing_scheme,
    DataToSign data_to_sign) {
  // SigningScheme is defined in a way where this cast is meaningful.
  uint16_t ssl_algorithm = static_cast<uint16_t>(kcer_signing_scheme);

  const EVP_MD* digest_method =
      SSL_get_signature_algorithm_digest(ssl_algorithm);
  uint8_t digest_buffer[EVP_MAX_MD_SIZE];
  uint8_t* digest = digest_buffer;
  unsigned int digest_len = 0;
  if (!digest_method ||
      !EVP_Digest(data_to_sign->data(), data_to_sign->size(), digest,
                  &digest_len, digest_method, nullptr)) {
    return base::unexpected(Error::kFailedToSignFailedToDigest);
  }

  bssl::UniquePtr<uint8_t> free_digest_info;
  if ((SSL_get_signature_algorithm_key_type(ssl_algorithm) == EVP_PKEY_RSA) &&
      !SSL_is_signature_algorithm_rsa_pss(ssl_algorithm)) {
    // PKCS#11 Sign expects the caller to prepend the DigestInfo for PKCS #1.
    int hash_nid =
        EVP_MD_type(SSL_get_signature_algorithm_digest(ssl_algorithm));
    int is_alloced = 0;
    size_t digest_with_prefix_len = 0;
    if (!RSA_add_pkcs1_prefix(&digest, &digest_with_prefix_len, &is_alloced,
                              hash_nid, digest, digest_len)) {
      return base::unexpected(Error::kFailedToSignFailedToAddPrefix);
    }
    digest_len = digest_with_prefix_len;
    if (is_alloced) {
      free_digest_info.reset(digest);
    }
  }

  return DigestWithPrefix(std::vector<uint8_t>(digest, digest + digest_len));
}

// The EC signature returned by Chaps is a concatenation of two numbers r and s
// (see PKCS#11 v2.40: 2.3.1 EC Signatures). Kcer needs to return it as a DER
// encoding of the following ASN.1 notations:
// Ecdsa-Sig-Value ::= SEQUENCE {
//     r       INTEGER,
//     s       INTEGER
// }
// (according to the RFC 8422, Section 5.4).
// This function reencodes the signature.
base::expected<std::vector<uint8_t>, Error> ReencodeEcSignature(
    base::span<const uint8_t> signature) {
  if (signature.size() % 2 != 0) {
    return base::unexpected(Error::kFailedToSignBadSignatureLength);
  }
  size_t order_size_bytes = signature.size() / 2;
  base::span<const uint8_t> r_bytes = signature.first(order_size_bytes);
  base::span<const uint8_t> s_bytes = signature.subspan(order_size_bytes);

  // Convert the RAW ECDSA signature to a DER-encoded ECDSA-Sig-Value.
  bssl::UniquePtr<ECDSA_SIG> sig(ECDSA_SIG_new());
  if (!sig || !BN_bin2bn(r_bytes.data(), r_bytes.size(), sig->r) ||
      !BN_bin2bn(s_bytes.data(), s_bytes.size(), sig->s)) {
    return base::unexpected(Error::kFailedToDerEncode);
  }

  std::vector<uint8_t> result_signature;

  {
    const int len = i2d_ECDSA_SIG(sig.get(), nullptr);
    if (len <= 0) {
      return base::unexpected(Error::kFailedToSignBadSignatureLength);
    }
    result_signature.resize(len);
  }

  {
    uint8_t* ptr = result_signature.data();
    const int len = i2d_ECDSA_SIG(sig.get(), &ptr);
    if (len <= 0) {
      return base::unexpected(Error::kFailedToDerEncode);
    }
  }

  return result_signature;
}

std::vector<uint8_t> GetPssSignParams(SigningScheme kcer_signing_scheme) {
  chromeos::PKCS11_CK_RSA_PKCS_PSS_PARAMS pss_params;

  uint16_t ssl_algorithm = static_cast<uint16_t>(kcer_signing_scheme);
  CHECK(SSL_is_signature_algorithm_rsa_pss(ssl_algorithm));

  const EVP_MD* digest_method =
      SSL_get_signature_algorithm_digest(ssl_algorithm);

  switch (EVP_MD_type(digest_method)) {
    case NID_sha256:
      pss_params.hashAlg = CKM_SHA256;
      pss_params.mgf = CKG_MGF1_SHA256;
      break;
    case NID_sha384:
      pss_params.hashAlg = CKM_SHA384;
      pss_params.mgf = CKG_MGF1_SHA384;
      break;
    case NID_sha512:
      pss_params.hashAlg = CKM_SHA512;
      pss_params.mgf = CKG_MGF1_SHA512;
      break;
    default:
      return {};
  }

  // Use the hash length for the salt length.
  pss_params.sLen = EVP_MD_size(digest_method);

  const uint8_t* params_ptr = reinterpret_cast<const uint8_t*>(&pss_params);
  return std::vector<uint8_t>(params_ptr, params_ptr + sizeof(pss_params));
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
  if (pkcs11_id->empty()) {
    return chaps_client_->DestroyObjectsWithRetries(
        pkcs_11_slot_id_, {public_key_id, private_key_id},
        Bind(std::move(task.callback), Error::kFailedToGetPkcs11Id));
  }

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

KcerTokenImpl::GenerateEcKeyTask::GenerateEcKeyTask(
    EllipticCurve in_curve,
    bool in_hardware_backed,
    Kcer::GenerateKeyCallback in_callback)
    : curve(in_curve),
      hardware_backed(in_hardware_backed),
      callback(std::move(in_callback)) {}
KcerTokenImpl::GenerateEcKeyTask::GenerateEcKeyTask(GenerateEcKeyTask&& other) =
    default;
KcerTokenImpl::GenerateEcKeyTask::~GenerateEcKeyTask() = default;

void KcerTokenImpl::GenerateEcKey(EllipticCurve curve,
                                  bool hardware_backed,
                                  Kcer::GenerateKeyCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(
        &KcerTokenImpl::GenerateEcKey, weak_factory_.GetWeakPtr(), curve,
        hardware_backed, std::move(callback)));
  }

  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = BlockQueueGetUnblocker(std::move(callback));

  GenerateEcKeyImpl(GenerateEcKeyTask(curve, hardware_backed,
                                      std::move(unblocking_callback)));
}

// Generates an EC key pair.
void KcerTokenImpl::GenerateEcKeyImpl(GenerateEcKeyTask task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  task.attemps_left--;
  if (task.attemps_left < 0) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kPkcs11SessionFailure));
  }

  if (task.curve != EllipticCurve::kP256) {
    return std::move(task.callback).Run(base::unexpected(Error::kBadKeyParams));
  }

  bssl::ScopedCBB cbb;
  uint8_t* ec_params_der = nullptr;
  size_t ec_params_der_len = 0;
  if (!CBB_init(cbb.get(), 0) ||
      !EC_KEY_marshal_curve_name(cbb.get(), EC_group_p256()) ||
      !CBB_finish(cbb.get(), &ec_params_der, &ec_params_der_len)) {
    return std::move(task.callback).Run(base::unexpected(Error::kBadKeyParams));
  }
  bssl::UniquePtr<uint8_t> der_deleter(ec_params_der);

  chromeos::PKCS11_CK_BBOOL kTrue = chromeos::PKCS11_CK_TRUE;

  chaps::AttributeList public_key_attrs;
  AddAttribute(public_key_attrs, chromeos::PKCS11_CKA_ENCRYPT,
               MakeSpan(&kTrue));
  AddAttribute(public_key_attrs, chromeos::PKCS11_CKA_VERIFY, MakeSpan(&kTrue));
  AddAttribute(public_key_attrs, chromeos::PKCS11_CKA_WRAP, MakeSpan(&kTrue));
  AddAttribute(public_key_attrs, chromeos::PKCS11_CKA_EC_PARAMS,
               base::make_span(ec_params_der, ec_params_der_len));

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
      base::BindOnce(&KcerTokenImpl::DidGenerateEcKey,
                     weak_factory_.GetWeakPtr(), std::move(task));
  chaps_client_->GenerateKeyPair(
      pkcs_11_slot_id_, chromeos::PKCS11_CKM_EC_KEY_PAIR_GEN,
      /*mechanism_parameter=*/{}, std::move(public_key_attrs),
      std::move(private_key_attrs), std::move(chaps_callback));
}

// Fetches the public key attributes of the generated key.
void KcerTokenImpl::DidGenerateEcKey(GenerateEcKeyTask task,
                                     ObjectHandle public_key_id,
                                     ObjectHandle private_key_id,
                                     uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return GenerateEcKeyImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToGenerateKey));
  }

  chaps_client_->GetAttributeValue(
      pkcs_11_slot_id_, public_key_id, {AttributeId::kEcPoint},
      base::BindOnce(&KcerTokenImpl::DidGetEcPublicKey,
                     weak_factory_.GetWeakPtr(), std::move(task), public_key_id,
                     private_key_id));
}

// Computes PKCS#11 for the key and sets it.
void KcerTokenImpl::DidGetEcPublicKey(
    GenerateEcKeyTask task,
    ObjectHandle public_key_id,
    ObjectHandle private_key_id,
    chaps::AttributeList public_key_attributes,
    uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return GenerateEcKeyImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return chaps_client_->DestroyObjectsWithRetries(
        pkcs_11_slot_id_, {public_key_id, private_key_id},
        Bind(std::move(task.callback), Error::kFailedToExportPublicKey));
  }

  base::span<const uint8_t> wrapped_ec_point =
      GetAttributeValue(public_key_attributes, AttributeId::kEcPoint);
  bssl::UniquePtr<ASN1_OCTET_STRING> ec_point_oct =
      UnwrapEcPoint(wrapped_ec_point);
  if (!ec_point_oct) {
    return chaps_client_->DestroyObjectsWithRetries(
        pkcs_11_slot_id_, {public_key_id, private_key_id},
        Bind(std::move(task.callback), Error::kFailedToReadAttribute));
  }
  const uint8_t* ec_point_data = ASN1_STRING_data(ec_point_oct.get());
  size_t ec_point_data_len = ASN1_STRING_length(ec_point_oct.get());
  base::span<const uint8_t> ec_point =
      base::make_span(ec_point_data, ec_point_data_len);

  PublicKeySpki spki = MakeEcSpki(ec_point);
  if (spki->empty()) {
    return chaps_client_->DestroyObjectsWithRetries(
        pkcs_11_slot_id_, {public_key_id, private_key_id},
        Bind(std::move(task.callback), Error::kFailedToCreateSpki));
  }

  Pkcs11Id pkcs11_id = MakePkcs11Id(ec_point);
  if (pkcs11_id->empty()) {
    return chaps_client_->DestroyObjectsWithRetries(
        pkcs_11_slot_id_, {public_key_id, private_key_id},
        Bind(std::move(task.callback), Error::kFailedToGetPkcs11Id));
  }

  PublicKey kcer_public_key(token_, pkcs11_id, std::move(spki));

  chaps::AttributeList attr_list;
  AddAttribute(attr_list, chromeos::PKCS11_CKA_ID, pkcs11_id.value());

  auto chaps_callback =
      base::BindOnce(&KcerTokenImpl::DidAssignEcKeyId,
                     weak_factory_.GetWeakPtr(), std::move(task), public_key_id,
                     private_key_id, std::move(kcer_public_key));
  chaps_client_->SetAttributeValue(pkcs_11_slot_id_,
                                   {public_key_id, private_key_id}, attr_list,
                                   std::move(chaps_callback));
}

void KcerTokenImpl::DidAssignEcKeyId(GenerateEcKeyTask task,
                                     ObjectHandle public_key_id,
                                     ObjectHandle private_key_id,
                                     PublicKey kcer_public_key,
                                     uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return GenerateEcKeyImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return chaps_client_->DestroyObjectsWithRetries(
        pkcs_11_slot_id_, {public_key_id, private_key_id},
        Bind(std::move(task.callback), Error::kFailedToWriteAttribute));
  }
  return std::move(task.callback).Run(std::move(kcer_public_key));
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

KcerTokenImpl::ListKeysTask::ListKeysTask(TokenListKeysCallback in_callback)
    : callback(std::move(in_callback)) {}
KcerTokenImpl::ListKeysTask::ListKeysTask(ListKeysTask&& other) = default;
KcerTokenImpl::ListKeysTask::~ListKeysTask() = default;

void KcerTokenImpl::ListKeys(TokenListKeysCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(&KcerTokenImpl::ListKeys,
                                                weak_factory_.GetWeakPtr(),
                                                std::move(callback)));
  }

  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = BlockQueueGetUnblocker(std::move(callback));

  ListKeysImpl(ListKeysTask(std::move(unblocking_callback)));
}

// Starts by finding RSA key objects.
void KcerTokenImpl::ListKeysImpl(ListKeysTask task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  task.attemps_left--;
  if (task.attemps_left < 0) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kPkcs11SessionFailure));
  }

  // For RSA keys the required attributes are stored in the private key objects.
  chromeos::PKCS11_CK_OBJECT_CLASS obj_class = chromeos::PKCS11_CKO_PRIVATE_KEY;
  chromeos::PKCS11_CK_KEY_TYPE key_type = chromeos::PKCS11_CKK_RSA;
  chaps::AttributeList attributes;
  AddAttribute(attributes, chromeos::PKCS11_CKA_CLASS, MakeSpan(&obj_class));
  AddAttribute(attributes, chromeos::PKCS11_CKA_KEY_TYPE, MakeSpan(&key_type));

  chaps_client_->FindObjects(
      pkcs_11_slot_id_, std::move(attributes),
      base::BindOnce(&KcerTokenImpl::ListKeysWithRsaHandles,
                     weak_factory_.GetWeakPtr(), std::move(task)));
}

// Starts iterating over the RSA keys.
void KcerTokenImpl::ListKeysWithRsaHandles(ListKeysTask task,
                                           std::vector<ObjectHandle> handles,
                                           uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return ListKeysImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToSearchForObjects));
  }

  ListKeysGetOneRsaKey(std::move(task), std::move(handles),
                       std::vector<PublicKey>());
}

// This is called repeatedly until `handles` is empty.
void KcerTokenImpl::ListKeysGetOneRsaKey(ListKeysTask task,
                                         std::vector<ObjectHandle> handles,
                                         std::vector<PublicKey> result_keys) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (handles.empty()) {
    // All RSA keys are handled, now search for EC keys.
    return ListKeysFindEcKeys(std::move(task), std::move(result_keys));
  }

  ObjectHandle current_handle = handles.back();
  handles.pop_back();

  chaps_client_->GetAttributeValue(
      pkcs_11_slot_id_, current_handle,
      {AttributeId::kPkcs11Id, AttributeId::kModulus,
       AttributeId::kPublicExponent},
      base::BindOnce(&KcerTokenImpl::ListKeysDidGetOneRsaKey,
                     weak_factory_.GetWeakPtr(), std::move(task),
                     std::move(handles), std::move(result_keys)));
}

// Receives attributes for a single RSA key and creates kcer::PublicKey from
// them.
void KcerTokenImpl::ListKeysDidGetOneRsaKey(ListKeysTask task,
                                            std::vector<ObjectHandle> handles,
                                            std::vector<PublicKey> result_keys,
                                            chaps::AttributeList attributes,
                                            uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return ListKeysImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    // Try to get as many keys as possible even if some of them fail.
    return ListKeysGetOneRsaKey(std::move(task), std::move(handles),
                                std::move(result_keys));
  }

  base::span<const uint8_t> pkcs11_id =
      GetAttributeValue(attributes, AttributeId::kPkcs11Id);
  base::span<const uint8_t> modulus =
      GetAttributeValue(attributes, AttributeId::kModulus);
  base::span<const uint8_t> public_exponent =
      GetAttributeValue(attributes, AttributeId::kPublicExponent);
  if (pkcs11_id.empty() || modulus.empty() || public_exponent.empty()) {
    LOG(WARNING) << "Invalid RSA key was fetched from Chaps, skipping it.";
    return ListKeysGetOneRsaKey(std::move(task), std::move(handles),
                                std::move(result_keys));
  }

  PublicKeySpki spki = MakeRsaSpki(modulus, public_exponent);
  if (spki->empty()) {
    LOG(WARNING) << "Invalid RSA key was fetched from Chaps, skipping it.";
    return ListKeysGetOneRsaKey(std::move(task), std::move(handles),
                                std::move(result_keys));
  }

  std::vector<uint8_t> id(pkcs11_id.begin(), pkcs11_id.end());
  result_keys.emplace_back(token_, Pkcs11Id(std::move(id)), std::move(spki));
  return ListKeysGetOneRsaKey(std::move(task), std::move(handles),
                              std::move(result_keys));
}

// Finds EC key objects.
void KcerTokenImpl::ListKeysFindEcKeys(ListKeysTask task,
                                       std::vector<PublicKey> result_keys) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // For EC keys the required attributes are stored in the public key objects.
  chromeos::PKCS11_CK_OBJECT_CLASS obj_class = chromeos::PKCS11_CKO_PUBLIC_KEY;
  chromeos::PKCS11_CK_KEY_TYPE key_type = chromeos::PKCS11_CKK_EC;
  chaps::AttributeList attributes;
  AddAttribute(attributes, chromeos::PKCS11_CKA_CLASS, MakeSpan(&obj_class));
  AddAttribute(attributes, chromeos::PKCS11_CKA_KEY_TYPE, MakeSpan(&key_type));

  chaps_client_->FindObjects(
      pkcs_11_slot_id_, std::move(attributes),
      base::BindOnce(&KcerTokenImpl::ListKeysWithEcHandles,
                     weak_factory_.GetWeakPtr(), std::move(task),
                     std::move(result_keys)));
}

// Starts iterating over the EC keys.
void KcerTokenImpl::ListKeysWithEcHandles(ListKeysTask task,
                                          std::vector<PublicKey> result_keys,
                                          std::vector<ObjectHandle> handles,
                                          uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return ListKeysImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToSearchForObjects));
  }

  ListKeysGetOneEcKey(std::move(task), std::move(handles),
                      std::move(result_keys));
}

// This is called repeatedly until `handles` is empty.
void KcerTokenImpl::ListKeysGetOneEcKey(ListKeysTask task,
                                        std::vector<ObjectHandle> handles,
                                        std::vector<PublicKey> result_keys) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (handles.empty()) {
    // All RSA and EC keys are handled, return the final result.
    return std::move(task.callback).Run(std::move(result_keys));
  }

  ObjectHandle current_handle = handles.back();
  handles.pop_back();

  chaps_client_->GetAttributeValue(
      pkcs_11_slot_id_, current_handle,
      {AttributeId::kPkcs11Id, AttributeId::kEcPoint},
      base::BindOnce(&KcerTokenImpl::ListKeysDidGetOneEcKey,
                     weak_factory_.GetWeakPtr(), std::move(task),
                     std::move(handles), std::move(result_keys)));
}

// Receives attributes for a single EC key and creates kcer::PublicKey from
// them.
void KcerTokenImpl::ListKeysDidGetOneEcKey(ListKeysTask task,
                                           std::vector<ObjectHandle> handles,
                                           std::vector<PublicKey> result_keys,
                                           chaps::AttributeList attributes,
                                           uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return ListKeysImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    // Try to get as many keys as possible even if some of them fail.
    return ListKeysGetOneEcKey(std::move(task), std::move(handles),
                               std::move(result_keys));
  }

  base::span<const uint8_t> pkcs11_id =
      GetAttributeValue(attributes, AttributeId::kPkcs11Id);
  base::span<const uint8_t> wrapped_ec_point =
      GetAttributeValue(attributes, AttributeId::kEcPoint);
  if (pkcs11_id.empty() || wrapped_ec_point.empty()) {
    LOG(WARNING) << "Invalid EC key was fetched from Chaps, skipping it.";
    return ListKeysGetOneEcKey(std::move(task), std::move(handles),
                               std::move(result_keys));
  }

  bssl::UniquePtr<ASN1_OCTET_STRING> ec_point_oct =
      UnwrapEcPoint(wrapped_ec_point);
  if (!ec_point_oct) {
    LOG(WARNING) << "Invalid EC key was fetched from Chaps, skipping it.";
    return ListKeysGetOneEcKey(std::move(task), std::move(handles),
                               std::move(result_keys));
  }
  const uint8_t* ec_point_data = ASN1_STRING_data(ec_point_oct.get());
  size_t ec_point_data_len = ASN1_STRING_length(ec_point_oct.get());
  base::span<const uint8_t> ec_point =
      base::make_span(ec_point_data, ec_point_data_len);

  PublicKeySpki spki = MakeEcSpki(ec_point);
  if (spki->empty()) {
    LOG(WARNING) << "Invalid EC key was fetched from Chaps, skipping it.";
    return ListKeysGetOneEcKey(std::move(task), std::move(handles),
                               std::move(result_keys));
  }

  std::vector<uint8_t> id(pkcs11_id.begin(), pkcs11_id.end());
  PublicKey public_key(token_, Pkcs11Id(std::move(id)), std::move(spki));

  chromeos::PKCS11_CK_OBJECT_CLASS obj_class = chromeos::PKCS11_CKO_PRIVATE_KEY;
  chromeos::PKCS11_CK_KEY_TYPE key_type = chromeos::PKCS11_CKK_EC;
  chaps::AttributeList private_key_attributes;
  AddAttribute(private_key_attributes, chromeos::PKCS11_CKA_CLASS,
               MakeSpan(&obj_class));
  AddAttribute(private_key_attributes, chromeos::PKCS11_CKA_KEY_TYPE,
               MakeSpan(&key_type));
  AddAttribute(private_key_attributes, chromeos::PKCS11_CKA_ID,
               public_key.GetPkcs11Id().value());

  // Check that the private key for public key exists in Chaps. RSA keys don't
  // need this check because key attributes can be read from the RSA private key
  // objects.
  chaps_client_->FindObjects(
      pkcs_11_slot_id_, std::move(private_key_attributes),
      base::BindOnce(&KcerTokenImpl::ListKeysDidFindEcPrivateKey,
                     weak_factory_.GetWeakPtr(), std::move(task),
                     std::move(handles), std::move(result_keys),
                     std::move(public_key)));
}

void KcerTokenImpl::ListKeysDidFindEcPrivateKey(
    ListKeysTask task,
    std::vector<ObjectHandle> handles,
    std::vector<PublicKey> result_keys,
    PublicKey current_public_key,
    std::vector<ObjectHandle> private_key_handles,
    uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!private_key_handles.empty()) {
    result_keys.push_back(std::move(current_public_key));
  }

  return ListKeysGetOneEcKey(std::move(task), std::move(handles),
                             std::move(result_keys));
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

KcerTokenImpl::SignTask::SignTask(PrivateKeyHandle in_key,
                                  SigningScheme in_signing_scheme,
                                  DataToSign in_data,
                                  Kcer::SignCallback in_callback)
    : key(std::move(in_key)),
      signing_scheme(in_signing_scheme),
      data(std::move(in_data)),
      callback(std::move(in_callback)) {}
KcerTokenImpl::SignTask::SignTask(SignTask&& other) = default;
KcerTokenImpl::SignTask::~SignTask() = default;

void KcerTokenImpl::Sign(PrivateKeyHandle key,
                         SigningScheme signing_scheme,
                         DataToSign data,
                         Kcer::SignCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(
        &KcerTokenImpl::Sign, weak_factory_.GetWeakPtr(), std::move(key),
        signing_scheme, std::move(data), std::move(callback)));
  }

  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = BlockQueueGetUnblocker(std::move(callback));

  if (!EnsurePkcs11IdIsSet(key)) {
    return std::move(unblocking_callback)
        .Run(base::unexpected(Error::kFailedToGetPkcs11Id));
  }

  SignImpl(SignTask(std::move(key), signing_scheme, std::move(data),
                    std::move(unblocking_callback)));
}

// Finds the key.
void KcerTokenImpl::SignImpl(SignTask task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  task.attemps_left--;
  if (task.attemps_left < 0) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kPkcs11SessionFailure));
  }

  Pkcs11Id key_id = task.key.GetPkcs11IdInternal();
  FindPrivateKey(std::move(key_id),
                 base::BindOnce(&KcerTokenImpl::SignWithKeyHandle,
                                weak_factory_.GetWeakPtr(), std::move(task)));
}

// Digests the data.
void KcerTokenImpl::SignWithKeyHandle(SignTask task,
                                      std::vector<ObjectHandle> key_handles,
                                      uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return SignImpl(std::move(task));
  }
  if ((result_code != chromeos::PKCS11_CKR_OK) || key_handles.empty()) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToSearchForObjects));
  }
  DCHECK_EQ(key_handles.size(), 1u);

  DataToSign data = task.data;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&DigestOnWorkerThread, task.signing_scheme,
                     std::move(data)),
      base::BindOnce(&KcerTokenImpl::SignWithKeyHandleAndDigest,
                     weak_factory_.GetWeakPtr(), std::move(task),
                     key_handles.front()));
}

// Signs the data.
void KcerTokenImpl::SignWithKeyHandleAndDigest(
    SignTask task,
    ObjectHandle key_handle,
    base::expected<DigestWithPrefix, Error> digest) {
  if (!digest.has_value()) {
    return std::move(task.callback).Run(base::unexpected(digest.error()));
  }

  uint64_t mechanism = SigningSchemeToPkcs11Mechanism(task.signing_scheme);
  std::vector<uint8_t> mechanism_params;

  if (mechanism == chromeos::PKCS11_CKM_RSA_PKCS_PSS) {
    mechanism_params = GetPssSignParams(task.signing_scheme);
  }

  auto chaps_callback = base::BindOnce(
      &KcerTokenImpl::DidSign, weak_factory_.GetWeakPtr(), std::move(task));

  chaps_client_->Sign(pkcs_11_slot_id_, mechanism, mechanism_params, key_handle,
                      std::move(digest).value().value(),
                      std::move(chaps_callback));
}

// Re-encodes the signature if needed.
void KcerTokenImpl::DidSign(SignTask task,
                            std::vector<uint8_t> signature,
                            uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return SignImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return std::move(task.callback).Run(base::unexpected(Error::kFailedToSign));
  }

  // ECDSA signatures have to be reencoded.
  uint64_t mechanism = SigningSchemeToPkcs11Mechanism(task.signing_scheme);
  if (mechanism == chromeos::PKCS11_CKM_ECDSA) {
    base::expected<std::vector<uint8_t>, Error> reencoded_signature =
        ReencodeEcSignature(std::move(signature));
    if (!reencoded_signature.has_value()) {
      return std::move(task.callback)
          .Run(base::unexpected(reencoded_signature.error()));
    }
    signature = std::move(reencoded_signature).value();
  }

  return std::move(task.callback).Run(Signature(signature));
}

//==============================================================================

KcerTokenImpl::SignRsaPkcs1RawTask::SignRsaPkcs1RawTask(
    PrivateKeyHandle in_key,
    DigestWithPrefix in_digest_with_prefix,
    Kcer::SignCallback in_callback)
    : key(std::move(in_key)),
      digest_with_prefix(std::move(in_digest_with_prefix)),
      callback(std::move(in_callback)) {}
KcerTokenImpl::SignRsaPkcs1RawTask::SignRsaPkcs1RawTask(
    SignRsaPkcs1RawTask&& other) = default;
KcerTokenImpl::SignRsaPkcs1RawTask::~SignRsaPkcs1RawTask() = default;

void KcerTokenImpl::SignRsaPkcs1Raw(PrivateKeyHandle key,
                                    DigestWithPrefix digest_with_prefix,
                                    Kcer::SignCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(
        &KcerTokenImpl::SignRsaPkcs1Raw, weak_factory_.GetWeakPtr(),
        std::move(key), std::move(digest_with_prefix), std::move(callback)));
  }

  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = BlockQueueGetUnblocker(std::move(callback));

  if (!EnsurePkcs11IdIsSet(key)) {
    return std::move(unblocking_callback)
        .Run(base::unexpected(Error::kFailedToGetPkcs11Id));
  }

  SignRsaPkcs1RawImpl(SignRsaPkcs1RawTask(std::move(key),
                                          std::move(digest_with_prefix),
                                          std::move(unblocking_callback)));
}

// Finds the key.
void KcerTokenImpl::SignRsaPkcs1RawImpl(SignRsaPkcs1RawTask task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  task.attemps_left--;
  if (task.attemps_left < 0) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kPkcs11SessionFailure));
  }

  Pkcs11Id key_id = task.key.GetPkcs11IdInternal();
  FindPrivateKey(std::move(key_id),
                 base::BindOnce(&KcerTokenImpl::SignRsaPkcs1RawWithKeyHandle,
                                weak_factory_.GetWeakPtr(), std::move(task)));
}

// Sings the data.
void KcerTokenImpl::SignRsaPkcs1RawWithKeyHandle(
    SignRsaPkcs1RawTask task,
    std::vector<ObjectHandle> key_handles,
    uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return SignRsaPkcs1RawImpl(std::move(task));
  }
  if ((result_code != chromeos::PKCS11_CKR_OK) || key_handles.empty()) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToSearchForObjects));
  }
  DCHECK_EQ(key_handles.size(), 1u);
  ObjectHandle key_handle = key_handles.front();

  uint64_t mechanism =
      SigningSchemeToPkcs11Mechanism(SigningScheme::kRsaPkcs1Sha256);

  std::vector<uint8_t> digest = task.digest_with_prefix.value();
  auto chaps_callback =
      base::BindOnce(&KcerTokenImpl::DidSignRsaPkcs1Raw,
                     weak_factory_.GetWeakPtr(), std::move(task));

  chaps_client_->Sign(pkcs_11_slot_id_, mechanism,
                      /*mechanism_parameter=*/std::vector<uint8_t>(),
                      key_handle, std::move(digest), std::move(chaps_callback));
}

void KcerTokenImpl::DidSignRsaPkcs1Raw(SignRsaPkcs1RawTask task,
                                       std::vector<uint8_t> signature,
                                       uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return SignRsaPkcs1RawImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return std::move(task.callback).Run(base::unexpected(Error::kFailedToSign));
  }

  return std::move(task.callback).Run(Signature(signature));
}

//==============================================================================

void KcerTokenImpl::GetTokenInfo(Kcer::GetTokenInfoCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Do not block the task queue, this method doesn't communicate with Chaps.

  TokenInfo result;
  result.pkcs11_id = pkcs_11_slot_id_.value();
  result.module_name = "Chaps";

  switch (token_) {
    case Token::kUser:
      result.token_name = "User Token";
      break;
    case Token::kDevice:
      result.token_name = "Device Token";
      break;
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
}

//==============================================================================

void KcerTokenImpl::GetKeyInfo(PrivateKeyHandle key,
                               Kcer::GetKeyInfoCallback callback) {
  // TODO(244409232): Implement.
}

//==============================================================================

void KcerTokenImpl::GetKeyPermissions(
    PrivateKeyHandle key,
    Kcer::GetKeyPermissionsCallback callback) {
  // TODO(244409232): Implement.
}

//==============================================================================

void KcerTokenImpl::GetCertProvisioningProfileId(
    PrivateKeyHandle key,
    Kcer::GetCertProvisioningProfileIdCallback callback) {
  // TODO(244409232): Implement.
}

//==============================================================================

KcerTokenImpl::SetKeyAttributeTask::SetKeyAttributeTask(
    PrivateKeyHandle in_key,
    HighLevelChapsClient::AttributeId in_attribute_id,
    std::vector<uint8_t> in_attribute_value,
    Kcer::StatusCallback in_callback)
    : key(std::move(in_key)),
      attribute_id(in_attribute_id),
      attribute_value(std::move(in_attribute_value)),
      callback(std::move(in_callback)) {}
KcerTokenImpl::SetKeyAttributeTask::SetKeyAttributeTask(
    SetKeyAttributeTask&& other) = default;
KcerTokenImpl::SetKeyAttributeTask::~SetKeyAttributeTask() = default;

void KcerTokenImpl::SetKeyAttribute(
    PrivateKeyHandle key,
    HighLevelChapsClient::AttributeId attribute_id,
    std::vector<uint8_t> attribute_value,
    Kcer::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!EnsurePkcs11IdIsSet(key)) {
    return std::move(callback).Run(
        base::unexpected(Error::kFailedToGetPkcs11Id));
  }

  SetKeyAttributeImpl(SetKeyAttributeTask(std::move(key), attribute_id,
                                          std::move(attribute_value),
                                          std::move(callback)));
}

// Finds the private key that will store the attribute.
void KcerTokenImpl::SetKeyAttributeImpl(SetKeyAttributeTask task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  task.attemps_left--;
  if (task.attemps_left < 0) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kPkcs11SessionFailure));
  }

  chromeos::PKCS11_CK_OBJECT_CLASS obj_class = chromeos::PKCS11_CKO_PRIVATE_KEY;
  chaps::AttributeList attributes;
  AddAttribute(attributes, chromeos::PKCS11_CKA_CLASS, MakeSpan(&obj_class));
  AddAttribute(attributes, chromeos::PKCS11_CKA_ID,
               task.key.GetPkcs11IdInternal().value());

  chaps_client_->FindObjects(
      pkcs_11_slot_id_, std::move(attributes),
      base::BindOnce(&KcerTokenImpl::SetKeyAttributeWithHandle,
                     weak_factory_.GetWeakPtr(), std::move(task)));
}

// Set attribute on the key.
void KcerTokenImpl::SetKeyAttributeWithHandle(
    SetKeyAttributeTask task,
    std::vector<ObjectHandle> private_key_handles,
    uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return SetKeyAttributeImpl(std::move(task));
  }
  if ((result_code != chromeos::PKCS11_CKR_OK) || private_key_handles.empty()) {
    return std::move(task.callback).Run(base::unexpected(Error::kKeyNotFound));
  }
  if (private_key_handles.size() != 1) {
    // This shouldn't happen.
    return std::move(task.callback)
        .Run(base::unexpected(Error::kUnexpectedFindResult));
  }

  chaps::AttributeList attributes;
  AddAttribute(attributes, static_cast<uint32_t>(task.attribute_id),
               task.attribute_value);

  chaps_client_->SetAttributeValue(
      pkcs_11_slot_id_, private_key_handles.front(), attributes,
      base::BindOnce(&KcerTokenImpl::SetKeyAttributeDidSetAttribute,
                     weak_factory_.GetWeakPtr(), std::move(task)));
}

void KcerTokenImpl::SetKeyAttributeDidSetAttribute(SetKeyAttributeTask task,
                                                   uint32_t result_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (SessionChapsClient::IsSessionError(result_code)) {
    return SetKeyAttributeImpl(std::move(task));
  }
  if (result_code != chromeos::PKCS11_CKR_OK) {
    return std::move(task.callback)
        .Run(base::unexpected(Error::kFailedToWriteAttribute));
  }
  return std::move(task.callback).Run({});
}

//==============================================================================

void KcerTokenImpl::SetKeyNickname(PrivateKeyHandle key,
                                   std::string nickname,
                                   Kcer::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(
        &KcerTokenImpl::SetKeyNickname, weak_factory_.GetWeakPtr(),
        std::move(key), std::move(nickname), std::move(callback)));
  }

  // Block task queue, attach unblocking
  // task to the callback.
  auto unblocking_callback = BlockQueueGetUnblocker(std::move(callback));

  return SetKeyAttribute(std::move(key),
                         HighLevelChapsClient::AttributeId::kLabel,
                         std::vector<uint8_t>(nickname.begin(), nickname.end()),
                         std::move(unblocking_callback));
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
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  is_blocked_ = false;

  if (task_queue_.empty()) {
    return;
  }

  base::OnceClosure next_task = std::move(task_queue_.front());
  task_queue_.pop_front();
  std::move(next_task).Run();
}

void KcerTokenImpl::FindPrivateKey(
    Pkcs11Id id,
    base::OnceCallback<void(std::vector<ObjectHandle>, uint32_t)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const chromeos::PKCS11_CK_OBJECT_CLASS kPrivKeyClass =
      chromeos::PKCS11_CKO_PRIVATE_KEY;
  chaps::AttributeList private_key_attrs;
  AddAttribute(private_key_attrs, chromeos::PKCS11_CKA_CLASS,
               MakeSpan(&kPrivKeyClass));
  AddAttribute(private_key_attrs, chromeos::PKCS11_CKA_ID, id.value());

  chaps_client_->FindObjects(pkcs_11_slot_id_, std::move(private_key_attrs),
                             std::move(callback));
}

}  // namespace kcer::internal
