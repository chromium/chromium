// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/enclave_manager.h"

#include <utility>
#include <variant>

#include "base/base64.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/functional/overloaded.h"
#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/strong_alias.h"
#include "build/build_config.h"
#include "chrome/browser/webauthn/proto/enclave_local_state.pb.h"
#include "chrome/common/chrome_version.h"
#include "components/cbor/diagnostic_writer.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "components/device_event_log/device_event_log.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/trusted_vault/frontend_trusted_vault_connection.h"
#include "components/trusted_vault/proto/recovery_key_store.pb.h"
#include "components/trusted_vault/recovery_key_store_connection.h"
#include "components/trusted_vault/recovery_key_store_connection_impl.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/trusted_vault_access_token_fetcher_frontend.h"
#include "components/trusted_vault/trusted_vault_access_token_fetcher_impl.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "components/unexportable_keys/ref_counted_unexportable_signing_key.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "crypto/aead.h"
#include "crypto/hkdf.h"
#include "crypto/random.h"
#include "crypto/sha2.h"
#include "crypto/unexportable_key.h"
#include "crypto/user_verifying_key.h"
#include "device/fido/enclave/constants.h"
#include "device/fido/enclave/enclave_websocket_client.h"
#include "device/fido/enclave/transact.h"
#include "device/fido/enclave/types.h"
#include "device/fido/features.h"
#include "device/fido/network_context_factory.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/url_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/rand.h"

#if BUILDFLAG(IS_MAC)
#include "crypto/scoped_lacontext.h"
#include "device/fido/mac/util.h"
#endif  // BUILDFLAG(IS_MAC)

namespace enclave = device::enclave;
using webauthn_pb::EnclaveLocalState;

// Holds the arguments to `StoreKeys` so that they can be processed when the
// state machine is ready for them.
struct EnclaveManager::StoreKeysArgs {
  std::string gaia_id;
  std::vector<std::vector<uint8_t>> keys;
  int last_key_version;
};

struct EnclaveManager::PendingAction {
  EnclaveManager::Callback callback;
  bool want_registration = false;
  std::unique_ptr<StoreKeysArgs> store_keys_args;
  bool setup_account = false;
  std::string pin;                  // the PIN to add to an account.
  std::string updated_pin;          // a new PIN, to replace the current PIN.
  std::optional<std::string> rapt;  // ReAuthentication Proof Token.
  std::unique_ptr<EnclaveLocalState::WrappedPIN> wrapped_pin;
  std::optional<std::string> pin_public_key;
};

namespace {

#if BUILDFLAG(IS_MAC)
constexpr char kUserVerifyingKeyKeychainAccessGroup[] =
    MAC_TEAM_IDENTIFIER_STRING "." MAC_BUNDLE_IDENTIFIER_STRING ".webauthn-uvk";
#endif  // BUILDFLAG(IS_MAC)

// The maximum number of bytes that will be downloaded from the above two URLs.
constexpr size_t kMaxFetchBodyBytes = 128 * 1024;

const net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("recovery_key_store_fetch", R"(
        semantics {
          sender: "Google Password Manager"
          description:
            "If a user enrolls a Google Password Manager PIN, it is hashed and "
            "sent to the Recovery Key Store so that they can recover their "
            "credentials with it in the future. This key store involves "
            "dedicated hardware to limit the number of guesses permitted. The "
            "PIN hash is encrypted directly to this hardware and these network "
            "fetches cover downloading the neccessary public key and uploading "
            "the encrypted package to the key store."
          trigger:
            "A user enrolls a PIN in Google Password Manager."
          user_data {
            type: ACCESS_TOKEN
          }
          data: "An encrypted PIN."
          internal {
            contacts {
              email: "chrome-webauthn@google.com"
            }
          }
          destination: GOOGLE_OWNED_SERVICE
          last_reviewed: "2024-02-08"
        }
        policy {
          cookies_allowed: NO
          setting: "Users can disable this feature by opening settings "
            "and signing out of the Google account in their profile, or by "
            "disabling password sync on the profile. Password sync can be "
            "disabled from the Sync and Google Services screen."
          chrome_policy {
            SyncDisabled {
              SyncDisabled: true
            }
            SyncTypesListDisabled {
              SyncTypesListDisabled: {
                entries: "passwords"
              }
            }
          }
        })");

// This prefix is the protobuf encoding for a 32-byte value with tag 1024.
// This means that, with the hash appended, the serialised state file is still a
// valid protobuf, which is handly for debugging.
static const uint8_t kHashPrefix[] = {0x82, 0x40, 32};

// Since protobuf maps `bytes` to `std::string` (rather than
// `std::vector<uint8_t>`), functions for jumping between these representations
// are needed.

base::span<const uint8_t> ToSpan(const std::string& s) {
  const uint8_t* data = reinterpret_cast<const uint8_t*>(s.data());
  return base::span<const uint8_t>(data, s.size());
}

template <size_t N>
base::span<const uint8_t, N> ToSizedSpan(const std::string& s) {
  CHECK_EQ(s.size(), N);
  const uint8_t* data = reinterpret_cast<const uint8_t*>(s.data());
  return base::span<const uint8_t, N>(data, N);
}

std::vector<uint8_t> ToVector(const std::string& s) {
  const auto span = ToSpan(s);
  return std::vector<uint8_t>(span.begin(), span.end());
}

std::string VecToString(base::span<const uint8_t> v) {
  const char* data = reinterpret_cast<const char*>(v.data());
  return std::string(data, data + v.size());
}

bool IsValidSubjectPublicKeyInfo(base::span<const uint8_t> spki) {
  CBS cbs;
  CBS_init(&cbs, spki.data(), spki.size());
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_parse_public_key(&cbs));
  return static_cast<bool>(pkey);
}

bool IsValidUncompressedP256X962(base::span<const uint8_t> x962) {
  if (x962.empty() || x962[0] != 4) {
    return false;
  }
  const EC_GROUP* group = EC_group_p256();
  bssl::UniquePtr<EC_POINT> point(EC_POINT_new(group));
  return 1 == EC_POINT_oct2point(group, point.get(), x962.data(), x962.size(),
                                 /*ctx=*/nullptr);
}

std::optional<int> CheckPINInvariants(
    const EnclaveLocalState::WrappedPIN& wrapped_pin) {
  // The nonce is 12 bytes, and the tag is 16 bytes, so this establishes
  // a lower-bound of one byte of plaintext.
  if (wrapped_pin.wrapped_pin().size() < 12 + 1 + 16) {
    return __LINE__;
  }
  if (wrapped_pin.claim_key().size() != 32) {
    return __LINE__;
  }
  if (wrapped_pin.generation() < 0) {
    return __LINE__;
  }
  if (wrapped_pin.form() == wrapped_pin.FORM_UNSPECIFIED) {
    return __LINE__;
  }
  if (wrapped_pin.hash() == wrapped_pin.HASH_UNSPECIFIED) {
    return __LINE__;
  }
  if (wrapped_pin.hash_difficulty() <= 0) {
    return __LINE__;
  }
  if (wrapped_pin.hash_salt().empty()) {
    return __LINE__;
  }

  return std::nullopt;
}

// CheckInvariants checks all the invariants of `user`, returning either a
// line-number for the failing check, or else `nullopt` to indicate success.
std::optional<int> CheckInvariants(const EnclaveLocalState::User& user) {
  if (user.wrapped_hardware_private_key().empty() !=
      user.hardware_public_key().empty()) {
    return __LINE__;
  }
  if (!user.hardware_public_key().empty() &&
      !IsValidSubjectPublicKeyInfo(ToSpan(user.hardware_public_key()))) {
    return __LINE__;
  }
  if (user.wrapped_hardware_private_key().empty() != user.device_id().empty()) {
    return __LINE__;
  }

  if (user.wrapped_uv_private_key().empty() != user.uv_public_key().empty()) {
    return __LINE__;
  }
  if (!user.uv_public_key().empty() &&
      !IsValidSubjectPublicKeyInfo(ToSpan(user.uv_public_key()))) {
    return __LINE__;
  }

  if (user.registered() && user.wrapped_hardware_private_key().empty()) {
    return __LINE__;
  }
  if (user.registered() != !user.wrapped_member_private_key().empty()) {
    return __LINE__;
  }
  if (user.wrapped_member_private_key().empty() !=
      user.member_public_key().empty()) {
    return __LINE__;
  }
  if (!user.member_public_key().empty() &&
      !IsValidUncompressedP256X962(ToSpan(user.member_public_key()))) {
    return __LINE__;
  }

  if (user.joined() && !user.registered()) {
    return __LINE__;
  }
  if (!user.wrapped_security_domain_secrets().empty() != user.joined()) {
    return __LINE__;
  }

  if (user.has_wrapped_pin() != user.has_pin_public_key()) {
    return __LINE__;
  }
  if (user.has_wrapped_pin()) {
    return CheckPINInvariants(user.wrapped_pin());
  }

  return std::nullopt;
}

// Build an enclave request that registers a new device and requests a new
// wrapped asymmetric key which will be used to join the security domain.
cbor::Value BuildRegistrationMessage(
    const std::string& device_id,
    const crypto::UnexportableSigningKey& hardware_key,
    scoped_refptr<crypto::RefCountedUserVerifyingSigningKey> uv_key) {
  cbor::Value::MapValue pub_keys;
  pub_keys.emplace(enclave::kHardwareKey,
                   hardware_key.GetSubjectPublicKeyInfo());
  if (uv_key) {
    pub_keys.emplace(enclave::kUserVerificationKey,
                     uv_key->key().GetPublicKey());
  }

  cbor::Value::MapValue request1;
  request1.emplace(enclave::kRequestCommandKey, enclave::kRegisterCommandName);
  request1.emplace(enclave::kRegisterDeviceIdKey,
                   std::vector<uint8_t>(device_id.begin(), device_id.end()));
  request1.emplace(enclave::kRegisterPubKeysKey, std::move(pub_keys));

  cbor::Value::MapValue request2;
  request2.emplace(enclave::kRequestCommandKey,
                   enclave::kGenKeyPairCommandName);
  request2.emplace(enclave::kWrappingPurpose,
                   enclave::kKeyPurposeSecurityDomainMemberKey);

  cbor::Value::ArrayValue requests;
  requests.emplace_back(std::move(request1));
  requests.emplace_back(std::move(request2));

  return cbor::Value(std::move(requests));
}

EnclaveLocalState::User* StateForUser(EnclaveLocalState* local_state,
                                      const CoreAccountInfo& account) {
  auto it = local_state->mutable_users()->find(account.gaia);
  if (it == local_state->mutable_users()->end()) {
    return nullptr;
  }
  return &(it->second);
}

EnclaveLocalState::User* CreateStateForUser(EnclaveLocalState* local_state,
                                            const CoreAccountInfo& account) {
  auto pair = local_state->mutable_users()->insert(
      {account.gaia, EnclaveLocalState::User()});
  CHECK(pair.second);
  return &(pair.first->second);
}

// Returns true if `response` contains exactly `num_responses` results, and none
// of them is an error. This is used for checking whether an enclave response is
// successful or not.
bool IsAllOk(const cbor::Value& response, const size_t num_responses) {
  if (!response.is_array()) {
    return false;
  }
  const cbor::Value::ArrayValue& responses = response.GetArray();
  if (responses.size() != num_responses) {
    return false;
  }
  for (size_t i = 0; i < num_responses; i++) {
    const cbor::Value& inner_response = responses[i];
    if (!inner_response.is_map()) {
      return false;
    }
    const cbor::Value::MapValue& inner_response_map = inner_response.GetMap();
    if (inner_response_map.find(cbor::Value(enclave::kResponseSuccessKey)) ==
        inner_response_map.end()) {
      return false;
    }
  }
  return true;
}

// Update `user` with the wrapped security domain member key in `response`.
// This is used when registering with the enclave, which provides a wrapped
// asymmetric key that becomes the security domain member key for this device.
bool SetSecurityDomainMemberKey(EnclaveLocalState::User* user,
                                const cbor::Value& wrap_response) {
  if (!wrap_response.is_map()) {
    return false;
  }
  const cbor::Value::MapValue& map = wrap_response.GetMap();
  const auto pub_it =
      map.find(cbor::Value(enclave::kWrappingResponsePublicKey));
  const auto priv_it =
      map.find(cbor::Value(enclave::kWrappingResponseWrappedPrivateKey));
  if (pub_it == map.end() || priv_it == map.end() ||
      !pub_it->second.is_bytestring() || !priv_it->second.is_bytestring()) {
    return false;
  }

  user->set_wrapped_member_private_key(
      VecToString(priv_it->second.GetBytestring()));
  user->set_member_public_key(VecToString(pub_it->second.GetBytestring()));
  return true;
}

// Build an enclave request to wrap the given security domain secrets.
cbor::Value::ArrayValue BuildSecretWrappingEnclaveRequest(
    const base::flat_map<int32_t, std::vector<uint8_t>>
        new_security_domain_secrets) {
  cbor::Value::ArrayValue requests;
  for (const auto& it : new_security_domain_secrets) {
    cbor::Value::MapValue request;
    request.emplace(enclave::kRequestCommandKey, enclave::kWrapKeyCommandName);
    request.emplace(enclave::kWrappingPurpose,
                    enclave::kKeyPurposeSecurityDomainSecret);
    request.emplace(enclave::kWrappingKeyToWrap, it.second);
    requests.emplace_back(std::move(request));
  }

  return requests;
}

// Build an enclave request to encrypt a PIN to the recovery key store.
cbor::Value::ArrayValue BuildRecoveryKeyStorePINWrappingEnclaveRequest(
    base::span<const uint8_t> hashed_pin,
    std::string cert_xml,
    std::string sig_xml) {
  cbor::Value::MapValue request;
  request.emplace(enclave::kRequestCommandKey,
                  enclave::kRecoveryKeyStoreWrapCommandName);
  request.emplace(enclave::kRecoveryKeyStorePinHash, hashed_pin);
  request.emplace(enclave::kRecoveryKeyStoreCertXml, ToVector(cert_xml));
  request.emplace(enclave::kRecoveryKeyStoreSigXml, ToVector(sig_xml));

  cbor::Value::ArrayValue requests;
  requests.emplace_back(std::move(request));
  return requests;
}

// Build an enclave request to wrap a PIN with the security domain secret.
cbor::Value::ArrayValue BuildPINWrappingEnclaveRequest(
    base::span<const uint8_t> hashed_pin,
    int64_t generation,
    base::span<const uint8_t, 32> claim_key,
    base::span<const uint8_t> wrapped_secret) {
  cbor::Value::MapValue request;
  request.emplace(enclave::kRequestCommandKey,
                  enclave::kPasskeysWrapPinCommandName);
  request.emplace(enclave::kPinHash, hashed_pin);
  request.emplace(enclave::kGeneration, generation);
  request.emplace(enclave::kClaimKey, claim_key);
  request.emplace(enclave::kRequestWrappedSecretKey, wrapped_secret);

  cbor::Value::ArrayValue requests;
  requests.emplace_back(std::move(request));
  return requests;
}

// Build an enclave request to unwrap a security domain secret and encrypt it to
// a fresh recovery key store entry.
cbor::Value::ArrayValue BuildRecoveryKeyStorePINChangeEnclaveRequest(
    base::span<const uint8_t> hashed_pin,
    std::string cert_xml,
    std::string sig_xml,
    base::span<const uint8_t> wrapped_secret) {
  cbor::Value::MapValue request;
  request.emplace(enclave::kRequestCommandKey,
                  enclave::kRecoveryKeyStoreWrapAsMemberCommandName);
  request.emplace(enclave::kRecoveryKeyStorePinHash, hashed_pin);
  request.emplace(enclave::kRecoveryKeyStoreCertXml, ToVector(cert_xml));
  request.emplace(enclave::kRecoveryKeyStoreSigXml, ToVector(sig_xml));
  request.emplace(enclave::kRequestWrappedSecretKey, wrapped_secret);

  cbor::Value::ArrayValue requests;
  requests.emplace_back(std::move(request));
  return requests;
}

cbor::Value ConcatEnclaveRequests(cbor::Value::ArrayValue head,
                                  cbor::Value::ArrayValue tail) {
  for (auto& request : tail) {
    head.emplace_back(std::move(request));
  }
  return cbor::Value(std::move(head));
}

// Update `user` with the wrapped secrets in `response`. The
// `new_security_domain_secrets` argument is used to determine the version
// numbers of the wrapped secrets and this value must be the same as was passed
// to `BuildSecretWrappingEnclaveRequest` to generate the enclave request.
bool StoreWrappedSecrets(EnclaveLocalState::User* user,
                         const base::flat_map<int32_t, std::vector<uint8_t>>
                             new_security_domain_secrets,
                         base::span<const cbor::Value> responses) {
  CHECK_EQ(new_security_domain_secrets.size(), responses.size());

  size_t i = 0;
  for (const auto& it : new_security_domain_secrets) {
    const cbor::Value& wrapped_value =
        responses[i++]
            .GetMap()
            .find(cbor::Value(enclave::kResponseSuccessKey))
            ->second;
    if (!wrapped_value.is_bytestring()) {
      return false;
    }
    const std::vector<uint8_t>& wrapped = wrapped_value.GetBytestring();
    if (wrapped.empty()) {
      return false;
    }
    user->mutable_wrapped_security_domain_secrets()->insert(
        {it.first, VecToString(wrapped)});
  }

  return true;
}

const char* TrustedVaultRegistrationStatusToString(
    trusted_vault::TrustedVaultRegistrationStatus status) {
  switch (status) {
    case trusted_vault::TrustedVaultRegistrationStatus::kSuccess:
      return "Success";
    case trusted_vault::TrustedVaultRegistrationStatus::kAlreadyRegistered:
      return "AlreadyRegistered";
    case trusted_vault::TrustedVaultRegistrationStatus::kLocalDataObsolete:
      return "LocalDataObsolete";
    case trusted_vault::TrustedVaultRegistrationStatus::
        kTransientAccessTokenFetchError:
      return "TransientAccessTokenFetchError";
    case trusted_vault::TrustedVaultRegistrationStatus::
        kPersistentAccessTokenFetchError:
      return "PersistentAccessTokenFetchError";
    case trusted_vault::TrustedVaultRegistrationStatus::
        kPrimaryAccountChangeAccessTokenFetchError:
      return "PrimaryAccountChangeAccessTokenFetchError";
    case trusted_vault::TrustedVaultRegistrationStatus::kNetworkError:
      return "NetworkError";
    case trusted_vault::TrustedVaultRegistrationStatus::kOtherError:
      return "OtherError";
  }
}

// The list of algorithms that are acceptable as device identity keys.
constexpr crypto::SignatureVerifier::SignatureAlgorithm kSigningAlgorithms[] = {
    // This is in preference order and the enclave must support all the
    // algorithms listed here.
    crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256,
    crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256,
};

// Parse the contents of the decrypted state file. In the event of an error, an
// empty state is returned. This causes a corrupt state file to reset the
// enclave state for the current profile. Users will have to re-register with
// the enclave.
std::unique_ptr<EnclaveLocalState> ParseStateFile(
    const std::string& contents_str) {
  auto ret = std::make_unique<EnclaveLocalState>();

  const base::span<const uint8_t> contents = ToSpan(contents_str);
  if (contents.size() < crypto::kSHA256Length + sizeof(kHashPrefix)) {
    FIDO_LOG(ERROR) << "Enclave state too small to be valid";
    return ret;
  }

  const base::span<const uint8_t> digest = contents.last(crypto::kSHA256Length);
  const base::span<const uint8_t> payload = contents.first(
      contents.size() - crypto::kSHA256Length - sizeof(kHashPrefix));
  const std::array<uint8_t, crypto::kSHA256Length> calculated =
      crypto::SHA256Hash(payload);
  if (memcmp(calculated.data(), digest.data(), crypto::kSHA256Length) != 0) {
    FIDO_LOG(ERROR) << "Checksum mismatch. Discarding state.";
    return ret;
  }

  if (!ret->ParseFromArray(payload.data(), payload.size())) {
    FIDO_LOG(ERROR) << "Parse failure loading enclave state";
    // Just in case the failed parse left partial state, reset it.
    ret = std::make_unique<EnclaveLocalState>();
  }

  return ret;
}

base::flat_set<std::string> GetGaiaIDs(
    const std::vector<gaia::ListedAccount>& listed_accounts) {
  base::flat_set<std::string> result;
  for (const gaia::ListedAccount& listed_account : listed_accounts) {
    result.insert(listed_account.gaia_id);
  }
  return result;
}

base::flat_set<std::string> GetGaiaIDs(
    const google::protobuf::Map<std::string, EnclaveLocalState::User>& users) {
  base::flat_set<std::string> result;
  for (const auto& it : users) {
    result.insert(it.first);
  }
  return result;
}

std::string UserVerifyingLabelToString(crypto::UserVerifyingKeyLabel label) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  return label;
#else
  return std::string();
#endif
}

std::optional<crypto::UserVerifyingKeyLabel> UserVerifyingKeyLabelFromString(
    std::string saved_label) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  return saved_label;
#else
  return std::nullopt;
#endif
}

// Fetch the contents of the given URL.
std::unique_ptr<network::SimpleURLLoader> FetchURL(
    network::mojom::URLLoaderFactory* url_loader_factory,
    std::string_view url,
    base::OnceCallback<void(std::optional<std::string>)> callback) {
  auto network_request = std::make_unique<network::ResourceRequest>();
  GURL gurl(url);
  CHECK(gurl.is_valid());
  network_request->url = std::move(gurl);

  auto loader = network::SimpleURLLoader::Create(std::move(network_request),
                                                 kTrafficAnnotation);
  loader->SetTimeoutDuration(base::Seconds(10));
  loader->SetURLLoaderFactoryOptions(
      network::mojom::kURLLoadOptionBlockAllCookies);
  loader->DownloadToString(url_loader_factory, std::move(callback),
                           kMaxFetchBodyBytes);
  return loader;
}

// Takes a CBOR array of bytestrings and returns those bytestrings assembled
// into an ASN.1 SEQUENCE.
std::optional<std::string> CBORListOfBytestringToASN1Sequence(
    const cbor::Value& array) {
  if (!array.is_array()) {
    return std::nullopt;
  }

  const std::vector<cbor::Value>& bytestrings = array.GetArray();
  base::CheckedNumeric<size_t> total_bytes_checked = 0;
  for (const auto& bytestring : bytestrings) {
    if (!bytestring.is_bytestring()) {
      return std::nullopt;
    }
    total_bytes_checked += bytestring.GetBytestring().size();
  }

  // 16 bytes is more than sufficient for the ASN.1 header that needs to be
  // prepended. (If it were not then `CBB_finish` would fail, below, so this is
  // not a memory-safety-load-bearing assumption.)
  total_bytes_checked += 16;

  if (!total_bytes_checked.IsValid()) {
    return std::nullopt;
  }
  const size_t total_bytes = total_bytes_checked.ValueOrDie();

  std::string cert_path;
  cert_path.resize(total_bytes);
  bssl::ScopedCBB cbb;
  CBB_init_fixed(cbb.get(), reinterpret_cast<uint8_t*>(&cert_path[0]),
                 cert_path.size());
  CBB inner;
  CBB_add_asn1(cbb.get(), &inner, CBS_ASN1_SEQUENCE);
  for (const auto& bytestring : bytestrings) {
    const std::vector<uint8_t>& bytes = bytestring.GetBytestring();
    if (!CBB_add_bytes(&inner, bytes.data(), bytes.size())) {
      return std::nullopt;
    }
  }
  size_t final_len;
  if (!CBB_finish(cbb.get(), nullptr, &final_len)) {
    return std::nullopt;
  }
  cert_path.resize(final_len);
  return cert_path;
}

// Convert the response to an enclave "recovery_key_store/wrap" command, into a
// protobuf that can be sent to the recovery key store service.
std::optional<std::unique_ptr<trusted_vault_pb::Vault>>
RecoveryKeyStoreWrapResponseToProto(
    base::span<const uint8_t> scrypt_salt,
    int scrypt_n,
    bool is_six_digits,
    const cbor::Value& recovery_key_store_wrap_response) {
  if (!recovery_key_store_wrap_response.is_map()) {
    return std::nullopt;
  }
  const cbor::Value::MapValue& response =
      recovery_key_store_wrap_response.GetMap();
  cbor::Value::MapValue::const_iterator it;

#define GET_BYTESTRING(name)                                 \
  it = response.find(cbor::Value(#name));                    \
  if (it == response.end() || !it->second.is_bytestring()) { \
    return std::nullopt;                                     \
  }                                                          \
  const std::vector<uint8_t>& name = it->second.GetBytestring();

  GET_BYTESTRING(cohort_public_key);
  GET_BYTESTRING(encrypted_recovery_key);
  GET_BYTESTRING(vault_handle);
  GET_BYTESTRING(counter_id);
  GET_BYTESTRING(app_public_key);
  GET_BYTESTRING(wrapped_app_private_key);
  GET_BYTESTRING(wrapped_wrapping_key);

#undef GET_BYTESTRING

  it = response.find(cbor::Value("max_attempts"));
  if (it == response.end() || !it->second.is_unsigned()) {
    return std::nullopt;
  }
  const int64_t max_attempts = it->second.GetUnsigned();
  if (max_attempts > std::numeric_limits<int32_t>::max()) {
    return std::nullopt;
  }

  // "certs_in_path" contains an array of bytestrings. Each is an X.509
  // certificate in the verified path from leaf to root, omitting the root
  // itself. The protobuf wants this in an ASN.1 SEQUENCE.
  it = response.find(cbor::Value("certs_in_path"));
  if (it == response.end()) {
    return std::nullopt;
  }
  std::optional<std::string> cert_path =
      CBORListOfBytestringToASN1Sequence(it->second);
  if (!cert_path) {
    return std::nullopt;
  }

  auto vault = std::make_unique<trusted_vault_pb::Vault>();
  auto* params = vault->mutable_vault_parameters();
  params->set_backend_public_key(VecToString(cohort_public_key));
  params->set_counter_id(VecToString(counter_id));
  params->set_max_attempts(base::checked_cast<int32_t>(max_attempts));
  params->set_vault_handle(VecToString(vault_handle));

  vault->set_recovery_key(VecToString(encrypted_recovery_key));

  auto* app_key = vault->add_application_keys();
  // This key name mirrors what Android sets.
  app_key->set_key_name("security_domain_member_key_encrypted_locally");
  auto* asymmetric_key_pair = app_key->mutable_asymmetric_key_pair();
  asymmetric_key_pair->set_public_key(VecToString(app_public_key));
  asymmetric_key_pair->set_wrapped_private_key(
      VecToString(wrapped_app_private_key));
  asymmetric_key_pair->set_wrapping_key(VecToString(wrapped_wrapping_key));

  trusted_vault_pb::VaultMetadata metadata;
  metadata.set_lskf_type(is_six_digits
                             ? trusted_vault_pb::VaultMetadata::PIN
                             : trusted_vault_pb::VaultMetadata::PASSWORD);
  metadata.set_hash_type(trusted_vault_pb::VaultMetadata::SCRYPT);
  metadata.set_hash_salt(VecToString(scrypt_salt));
  metadata.set_hash_difficulty(scrypt_n);
  metadata.set_cert_path(std::move(*cert_path));

  std::string metadata_bytes;
  if (!metadata.SerializeToString(&metadata_bytes)) {
    return std::nullopt;
  }
  vault->set_vault_metadata(std::move(metadata_bytes));

  return vault;
}

base::flat_map<int32_t, std::vector<uint8_t>> GetNewSecretsToStore(
    const EnclaveLocalState::User& user,
    const EnclaveManager::StoreKeysArgs& args) {
  const auto& existing = user.wrapped_security_domain_secrets();
  base::flat_map<int32_t, std::vector<uint8_t>> new_secrets;
  for (int32_t i = args.last_key_version - args.keys.size() + 1;
       i <= args.last_key_version; i++) {
    if (existing.find(i) == existing.end()) {
      new_secrets.emplace(i, args.keys[args.last_key_version - i]);
    }
  }

  return new_secrets;
}

std::unique_ptr<crypto::UnexportableKeyProvider> GetUnexportableKeyProvider() {
  if (base::FeatureList::IsEnabled(
          device::kWebAuthnUseInsecureSoftwareUnexportableKeys)) {
    return crypto::GetSoftwareUnsecureUnexportableKeyProvider();
  }
  crypto::UnexportableKeyProvider::Config config;
#if BUILDFLAG(IS_MAC)
  config.keychain_access_group = kUserVerifyingKeyKeychainAccessGroup;
#endif  // BUILDFLAG(IS_MAC)
  return crypto::GetUnexportableKeyProvider(std::move(config));
}

crypto::UserVerifyingKeyProvider::Config MakeUserVerifyingKeyConfig(
    EnclaveManager::UVKeyOptions options) {
  crypto::UserVerifyingKeyProvider::Config config;
#if BUILDFLAG(IS_MAC)
  config.keychain_access_group = kUserVerifyingKeyKeychainAccessGroup;
  config.lacontext = std::move(options.lacontext);
#endif  // BUILDFLAG(IS_MAC)
  return config;
}

struct HashedPIN {
  ~HashedPIN() { memset(hashed, 0, sizeof(hashed)); }

  // Copies the values of this structure into a `WrappedPIN` protobuf with a
  // random claim key. The inner `wrapped_pin` member is not set and needs to be
  // filled in by the caller once that value is available.
  std::unique_ptr<EnclaveLocalState::WrappedPIN> ToWrappedPIN(
      int64_t generation) const {
    uint8_t claim_key[32];
    crypto::RandBytes(claim_key);

    auto ret = std::make_unique<EnclaveLocalState::WrappedPIN>();
    ret->set_claim_key(VecToString(claim_key));
    ret->set_generation(generation);
    ret->set_form(this->is_six_digits
                      ? EnclaveLocalState::WrappedPIN::FORM_SIX_DIGITS
                      : EnclaveLocalState::WrappedPIN::FORM_ARBITRARY);
    ret->set_hash(EnclaveLocalState::WrappedPIN::HASH_SCRYPT);
    ret->set_hash_difficulty(this->n);
    ret->set_hash_salt(VecToString(this->salt));

    return ret;
  }

  int n = 0;  // The scrypt `N` parameter.
  bool is_six_digits = false;
  uint8_t salt[16];
  uint8_t hashed[32];
};

std::unique_ptr<HashedPIN> HashPINSlowly(std::string_view pin) {
  auto hashed = std::make_unique<HashedPIN>();
  RAND_bytes(hashed->salt, sizeof(hashed->salt));
  // This is the primary work factor in scrypt. This value matches
  // the original recommended parameters. Those are a little out
  // of date in 2024, but Android is using 4096. Since this work
  // factor falls on the server when MagicArch is used, I've stuck
  // with this norm.
  hashed->n = 16384;
  hashed->is_six_digits =
      pin.size() == 6 && base::ranges::all_of(pin, [](char c) -> bool {
        return c >= '0' && c <= '9';
      });
  CHECK(EVP_PBE_scrypt(pin.data(), pin.size(), hashed->salt,
                       sizeof(hashed->salt), hashed->n, 8, 1,
                       /*max_mem=*/0, hashed->hashed, sizeof(hashed->hashed)));
  return hashed;
}

std::pair<int32_t, std::vector<uint8_t>> GetCurrentWrappedSecretForUser(
    const EnclaveLocalState::User* user) {
  CHECK(!user->wrapped_security_domain_secrets().empty());

  std::optional<int32_t> max_version;
  const std::string* max_wrapped_secret = nullptr;
  for (const auto& it : user->wrapped_security_domain_secrets()) {
    if (!max_version.has_value() || *max_version < it.first) {
      max_version = it.first;
      max_wrapped_secret = &it.second;
    }
  }
  return std::make_pair(*max_version, ToVector(*max_wrapped_secret));
}

std::vector<uint8_t> EncryptWrappedPIN(
    base::span<const uint8_t> security_domain_secret,
    base::span<const uint8_t> cbor_bytes) {
  // This is "KeychainApplicationKey:chrome:GPM PIN data wrapping key".
  static constexpr uint8_t kKeyPurposePinDataKey[] = {
      0x4b, 0x65, 0x79, 0x63, 0x68, 0x61, 0x69, 0x6e, 0x41, 0x70, 0x70,
      0x6c, 0x69, 0x63, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x4b, 0x65, 0x79,
      0x3a, 0x63, 0x68, 0x72, 0x6f, 0x6d, 0x65, 0x3a, 0x47, 0x50, 0x4d,
      0x20, 0x50, 0x49, 0x4e, 0x20, 0x64, 0x61, 0x74, 0x61, 0x20, 0x77,
      0x72, 0x61, 0x70, 0x70, 0x69, 0x6e, 0x67, 0x20, 0x6b, 0x65, 0x79};
  const std::vector<uint8_t> derived_key = crypto::HkdfSha256(
      security_domain_secret, /*salt=*/base::span<const uint8_t>(),
      kKeyPurposePinDataKey, 32);
  crypto::Aead aead(crypto::Aead::AeadAlgorithm::AES_256_GCM);
  aead.Init(derived_key);
  uint8_t nonce[12];
  crypto::RandBytes(nonce);
  std::vector<uint8_t> wrapped_pin = aead.Seal(
      cbor_bytes, nonce, /*additional_data=*/base::span<const uint8_t>());
  wrapped_pin.insert(wrapped_pin.begin(), std::begin(nonce), std::end(nonce));
  return wrapped_pin;
}

}  // namespace

// StateMachine performs a sequence of actions, as specified by the public
// `set_` functions, when `Start` is called. It always operates within the
// context of a specific Google account and will be destroyed by the
// EnclaveManager if the currently signed-in user changes. It works on a copy of
// the EnclaveLocalState and writes updated versions to the EnclaveManager
// once they are ready. A StateMachine is owned by the EnclaveManager and at
// most one exists at any given time.
class EnclaveManager::StateMachine {
 public:
  explicit StateMachine(EnclaveManager* manager,
                        webauthn_pb::EnclaveLocalState local_state,
                        std::unique_ptr<CoreAccountInfo> primary_account_info,
                        std::unique_ptr<PendingAction> action)
      : manager_(manager),
        local_state_(std::move(local_state)),
        user_(StateForUser(&local_state_, *primary_account_info)),
        primary_account_info_(std::move(primary_account_info)),
        action_(std::move(action)) {
    Process(None());
  }

  ~StateMachine() {
    if (action_->callback) {
      std::move(action_->callback).Run(false);
    }
  }

 private:
  // This class is a state machine that uses the following states. It moves from
  // state to state in response to `Event` values.
  enum class State {
    kStop,
    kNextAction,
    kGeneratingKeys,
    kWaitingForEnclaveTokenForRegistration,
    kRegisteringWithEnclave,
    kWaitingForEnclaveTokenForWrapping,
    kWrappingSecrets,
    kJoiningDomain,
    kHashingPIN,
    kDownloadingRecoveryKeyStoreKeys,
    kWaitingForEnclaveTokenForPINWrapping,
    kWrappingPIN,
    kWaitingForRecoveryKeyStore,
    kJoiningPINToDomain,
    kJoiningUpdatedPINToDomain,
    kChangingPIN,
  };

  enum class FetchedFile {
    kCertFile,
    kSigFile,
  };

  using None = base::StrongAlias<class None, absl::monostate>;
  using Failure =
      base::StrongAlias<class KeyGenerationFailure, absl::monostate>;
  using FileContents = base::StrongAlias<class FileContents, std::string>;
  using KeyReady = base::StrongAlias<
      class KeyGenerated,
      std::pair<std::unique_ptr<crypto::UserVerifyingSigningKey>,
                std::unique_ptr<crypto::UnexportableSigningKey>>>;
  using EnclaveResponse = base::StrongAlias<class EnclaveResponse, cbor::Value>;
  using JoinStatus =
      base::StrongAlias<class JoinStatus,
                        std::pair<trusted_vault::TrustedVaultRegistrationStatus,
                                  /*key_version=*/int>>;
  using AccessToken = base::StrongAlias<class AccessToken, std::string>;
  using FileFetched =
      base::StrongAlias<class FileFetched,
                        std::pair<FetchedFile, std::optional<std::string>>>;
  using PINHashed =
      base::StrongAlias<class PINHashed, std::unique_ptr<HashedPIN>>;
  using Response = base::StrongAlias<class Response, std::string>;
  using Event = absl::variant<None,
                              Failure,
                              FileContents,
                              KeyReady,
                              EnclaveResponse,
                              AccessToken,
                              JoinStatus,
                              FileFetched,
                              PINHashed,
                              Response,
                              trusted_vault::UpdateRecoveryKeyStoreStatus>;

  void Process(Event event) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    CHECK(!processing_) << ToString(state_);
    processing_ = true;

    const State initial_state = state_;
    const std::string event_str = ToString(event);

    switch (state_) {
      case State::kStop:
        // This should never be observed here as this special case is handled
        // below.
        NOTREACHED();
        break;

      case State::kNextAction:
        CHECK(absl::holds_alternative<None>(event)) << ToString(event);
        DoNextAction();
        break;

      case State::kGeneratingKeys:
        DoGeneratingKeys(std::move(event));
        break;

      case State::kWaitingForEnclaveTokenForRegistration:
        DoWaitingForEnclaveTokenForRegistration(std::move(event));
        break;

      case State::kRegisteringWithEnclave:
        DoRegisteringWithEnclave(std::move(event));
        break;

      case State::kWaitingForEnclaveTokenForWrapping:
        DoWaitingForEnclaveTokenForWrapping(std::move(event));
        break;

      case State::kWrappingSecrets:
        DoWrappingSecrets(std::move(event));
        break;

      case State::kJoiningDomain:
        DoJoiningDomain(std::move(event));
        break;

      case State::kHashingPIN:
        DoHashingPIN(std::move(event));
        break;

      case State::kDownloadingRecoveryKeyStoreKeys:
        DoDownloadingRecoveryKeyStoreKeys(std::move(event));
        break;

      case State::kWaitingForEnclaveTokenForPINWrapping:
        DoWaitingForEnclaveTokenForPINWrapping(std::move(event));
        break;

      case State::kWrappingPIN:
        DoWrappingPIN(std::move(event));
        break;

      case State::kWaitingForRecoveryKeyStore:
        DoWaitingForRecoveryKeyStore(std::move(event));
        break;

      case State::kJoiningPINToDomain:
        DoJoiningPINToDomain(std::move(event));
        break;

      case State::kChangingPIN:
        DoChangingPIN(std::move(event));
        break;

      case State::kJoiningUpdatedPINToDomain:
        DoJoiningUpdatedPINToDomain(std::move(event));
        break;
    }

    FIDO_LOG(EVENT) << ToString(initial_state) << " -" << event_str << "-> "
                    << ToString(state_);

    if (state_ == State::kStop) {
      std::move(action_->callback).Run(success_);
      manager_->Stopped();
      // `this` has been deleted now.
      return;
    }

    // The only internal state transition (i.e. where one state moves to another
    // without waiting for an external event) allowed is to `kNextAction`.
    if (state_ != State::kNextAction) {
      processing_ = false;
      return;
    }

    const State prior_state = state_;
    DoNextAction();
    FIDO_LOG(EVENT) << ToString(prior_state) << " --> " << ToString(state_);

    if (state_ == State::kStop) {
      std::move(action_->callback).Run(success_);
      manager_->Stopped();
      // `this` has been deleted now.
      return;
    }

    processing_ = false;
  }

  static std::string ToString(State state) {
    switch (state) {
      case State::kStop:
        return "Stop";
      case State::kNextAction:
        return "NextAction";
      case State::kGeneratingKeys:
        return "GeneratingKeys";
      case State::kWaitingForEnclaveTokenForRegistration:
        return "WaitingForEnclaveTokenForRegistration";
      case State::kRegisteringWithEnclave:
        return "RegisteringWithEnclave";
      case State::kWaitingForEnclaveTokenForWrapping:
        return "WaitingForEnclaveTokenForWrapping";
      case State::kWrappingSecrets:
        return "WrappingSecrets";
      case State::kJoiningDomain:
        return "JoiningDomain";
      case State::kHashingPIN:
        return "HashingPIN";
      case State::kDownloadingRecoveryKeyStoreKeys:
        return "DownloadingRecoveryKeyStoreKeys";
      case State::kWaitingForEnclaveTokenForPINWrapping:
        return "WaitingForEnclaveTokenForPINWrapping";
      case State::kWrappingPIN:
        return "WrappingPIN";
      case State::kWaitingForRecoveryKeyStore:
        return "WaitingForRecoveryKeyStore";
      case State::kJoiningPINToDomain:
        return "JoiningPINToDomain";
      case State::kChangingPIN:
        return "ChangingPIN";
      case State::kJoiningUpdatedPINToDomain:
        return "JoiningUpdatedPINToDomain";
    }
  }

  static const char* ToString(
      trusted_vault::UpdateRecoveryKeyStoreStatus status) {
    switch (status) {
      case trusted_vault::UpdateRecoveryKeyStoreStatus::kSuccess:
        return "Success";
      case trusted_vault::UpdateRecoveryKeyStoreStatus::
          kTransientAccessTokenFetchError:
        return "TransientError";
      case trusted_vault::UpdateRecoveryKeyStoreStatus::
          kPersistentAccessTokenFetchError:
        return "AccessTokenError";
      case trusted_vault::UpdateRecoveryKeyStoreStatus::
          kPrimaryAccountChangeAccessTokenFetchError:
        return "AccountChangedError";
      case trusted_vault::UpdateRecoveryKeyStoreStatus::kNetworkError:
        return "NetworkError";
      case trusted_vault::UpdateRecoveryKeyStoreStatus::kOtherError:
        return "OtherError";
    }
  }

  static std::string ToString(const Event& event) {
    return absl::visit(
        base::Overloaded{
            [](const None&) { return std::string(); },
            [](const Failure&) { return std::string("Failure"); },
            [](const FileContents&) { return std::string("FileContents"); },
            [](const KeyReady&) { return std::string("KeyReady"); },
            [](const EnclaveResponse&) {
              return std::string("EnclaveResponse");
            },
            [](const AccessToken&) { return std::string("AccessToken"); },
            [](const JoinStatus& status) {
              return base::StrCat(
                  {"JoinStatus(",
                   TrustedVaultRegistrationStatusToString(status.value().first),
                   ", ", base::NumberToString(status.value().second), ")"});
            },
            [](const FileFetched& fetched) {
              const FetchedFile fetched_file = fetched.value().first;
              const std::optional<std::string>& contents =
                  fetched.value().second;
              return base::StrCat(
                  {"FileFetched(", ToString(fetched_file), ", ",
                   (contents ? base::StringPrintf("%zu bytes", contents->size())
                             : "error"),
                   ")"});
            },
            [](const PINHashed&) { return std::string("PINHashed"); },
            [](const Response& response) {
              const std::string& response_str = response.value();
              return base::StringPrintf("Response(%zu bytes)",
                                        response_str.size());
            },
            [](const trusted_vault::UpdateRecoveryKeyStoreStatus& status) {
              return base::StrCat(
                  {"UpdateRecoveryKeyStoreStatus(", ToString(status), ")"});
            },
        },
        event);
  }

  static std::string ToString(FetchedFile fetched_file) {
    switch (fetched_file) {
      case FetchedFile::kCertFile:
        return "cert.xml";
      case FetchedFile::kSigFile:
        return "cert.sig.xml";
    }
  }

  void DoNextAction() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if ((action_->want_registration || action_->store_keys_args ||
         !action_->pin.empty()) &&
        !user_->registered()) {
      action_->want_registration = false;
      StartEnclaveRegistration();
      return;
    }

    if (user_->registered() && !action_->pin.empty()) {
      if (action_->setup_account) {
        CHECK(!action_->store_keys_args);
        action_->setup_account = false;

        // Create `store_keys_args_for_joining_` as if we had received the keys
        // for the security domain from an external source.
        store_keys_args_for_joining_ = std::make_unique<StoreKeysArgs>();
        store_keys_args_for_joining_->gaia_id = primary_account_info_->gaia;
        uint8_t security_domain_secret[32];
        crypto::RandBytes(security_domain_secret);
        store_keys_args_for_joining_->keys.emplace_back(
            std::begin(security_domain_secret),
            std::end(security_domain_secret));
        // Zero is a special value that indicates that the epoch is unknown.
        store_keys_args_for_joining_->last_key_version = 0;
      } else {
        CHECK(action_->store_keys_args);
        store_keys_args_for_joining_ = std::move(action_->store_keys_args);
      }

      state_ = State::kHashingPIN;
      HashPIN(std::move(action_->pin));
      return;
    }

    if (user_->registered() && action_->store_keys_args) {
      CHECK_EQ(primary_account_info_->gaia, action_->store_keys_args->gaia_id);
      auto store_keys_args = std::move(action_->store_keys_args);
      action_->store_keys_args.reset();

      new_security_domain_secrets_ =
          GetNewSecretsToStore(*user_, *store_keys_args);
      store_keys_args_for_joining_ = std::move(store_keys_args);
      if (!new_security_domain_secrets_.empty()) {
        state_ = State::kWaitingForEnclaveTokenForWrapping;
        GetAccessTokenInternal(GaiaConstants::kPasskeysEnclaveOAuth2Scope);
      } else if (!user_->joined() && !user_->member_public_key().empty()) {
        JoinSecurityDomain();
      }
      return;
    }

    if (!action_->updated_pin.empty()) {
      if (!user_->registered()) {
        state_ = State::kStop;
        return;
      }

      is_pin_update_ = true;
      rapt_ = std::move(action_->rapt);
      state_ = State::kHashingPIN;
      HashPIN(std::move(action_->updated_pin));
      return;
    }

    success_ = true;
    state_ = State::kStop;
  }

  void FetchComplete(FetchedFile file, std::optional<std::string> contents) {
    Process(FileFetched(std::make_pair(file, std::move(contents))));
  }

  void StartEnclaveRegistration() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    state_ = State::kGeneratingKeys;

    manager_->user_verifying_key_.reset();

    crypto::AreUserVerifyingKeysSupported(
        MakeUserVerifyingKeyConfig(/*options=*/{}),
        base::BindOnce(
            [](base::WeakPtr<StateMachine> state_machine,
               bool is_uv_key_supported) {
              if (!state_machine) {
                return;
              }
              auto key_provider = crypto::GetUserVerifyingKeyProvider(
                  MakeUserVerifyingKeyConfig(/*options=*/{}));
              if (!is_uv_key_supported || !key_provider) {
                // UV keys are not available, so skip to generating a hardware
                // key.
                state_machine->GenerateHardwareKey(nullptr);
                return;
              }
              if (state_machine->user_->wrapped_uv_private_key().empty()) {
                // Create a new UV key.
                key_provider->GenerateUserVerifyingSigningKey(
                    kSigningAlgorithms,
                    base::BindOnce(&StateMachine::GenerateHardwareKey,
                                   state_machine));
                return;
              }
              // Use the existing UV key.
              key_provider->GetUserVerifyingSigningKey(
                  state_machine->user_->wrapped_uv_private_key(),
                  base::BindOnce(&StateMachine::GenerateHardwareKey,
                                 state_machine));
            },
            weak_ptr_factory_.GetWeakPtr()));
  }

  void GenerateHardwareKey(
      std::unique_ptr<crypto::UserVerifyingSigningKey> uv_key) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(state_ == State::kGeneratingKeys);
    std::optional<std::vector<uint8_t>> existing_key_id;
    if (!user_->wrapped_hardware_private_key().empty()) {
      existing_key_id = ToVector(user_->wrapped_hardware_private_key());
    }
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
        base::BindOnce(
            [](std::optional<std::vector<uint8_t>> key_id,
               std::unique_ptr<crypto::UserVerifyingSigningKey> uv_key)
                -> Event {
              std::unique_ptr<crypto::UnexportableKeyProvider> provider =
                  GetUnexportableKeyProvider();
              if (!provider) {
                return Failure();
              }
              if (key_id) {
                std::unique_ptr<crypto::UnexportableSigningKey> key =
                    provider->FromWrappedSigningKeySlowly(*key_id);
                if (key) {
                  return KeyReady(
                      std::make_pair(std::move(uv_key), std::move(key)));
                }
              }
              std::unique_ptr<crypto::UnexportableSigningKey> key =
                  provider->GenerateSigningKeySlowly(kSigningAlgorithms);
              if (!key) {
                return Failure();
              }
              return KeyReady(
                  std::make_pair(std::move(uv_key), std::move(key)));
            },
            std::move(existing_key_id), std::move(uv_key)),
        base::BindOnce(&StateMachine::Process, weak_ptr_factory_.GetWeakPtr()));
  }

  void DoGeneratingKeys(Event event) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (absl::holds_alternative<Failure>(event)) {
      state_ = State::kStop;
      return;
    }
    CHECK(absl::holds_alternative<KeyReady>(event)) << ToString(event);

    bool state_dirty = false;

    auto uv_key = std::move(absl::get_if<KeyReady>(&event)->value().first);
    manager_->user_verifying_key_ =
        (uv_key == nullptr)
            ? nullptr
            : base::MakeRefCounted<crypto::RefCountedUserVerifyingSigningKey>(
                  std::move(uv_key));

    manager_->hardware_key_ = base::MakeRefCounted<
        unexportable_keys::RefCountedUnexportableSigningKey>(
        std::move(absl::get_if<KeyReady>(&event)->value().second),
        unexportable_keys::UnexportableKeyId());

    EnclaveLocalState::User* const user_state = user_;
    if (manager_->user_verifying_key_) {
      const std::vector<uint8_t> uv_public_key =
          manager_->user_verifying_key_->key().GetPublicKey();
      const std::string uv_public_key_str = VecToString(uv_public_key);
      if (user_state->uv_public_key() != uv_public_key_str) {
        user_state->set_uv_public_key(uv_public_key_str);
        user_state->set_wrapped_uv_private_key(UserVerifyingLabelToString(
            manager_->user_verifying_key_->key().GetKeyLabel()));
        state_dirty = true;
      }
    }

    const std::vector<uint8_t> spki =
        manager_->hardware_key_->key().GetSubjectPublicKeyInfo();
    const std::string spki_str = VecToString(spki);
    if (user_state->hardware_public_key() != spki_str) {
      std::array<uint8_t, crypto::kSHA256Length> device_id =
          crypto::SHA256Hash(spki);
      user_state->set_hardware_public_key(spki_str);
      user_state->set_wrapped_hardware_private_key(
          VecToString(manager_->hardware_key_->key().GetWrappedKey()));
      user_state->set_device_id(VecToString(device_id));
      state_dirty = true;
    }

    if (state_dirty) {
      manager_->WriteState(&local_state_);
    }

    state_ = State::kWaitingForEnclaveTokenForRegistration;
    GetAccessTokenInternal(GaiaConstants::kPasskeysEnclaveOAuth2Scope);
  }

  void DoWaitingForEnclaveTokenForRegistration(Event event) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    access_token_fetcher_.reset();
    if (absl::holds_alternative<Failure>(event)) {
      FIDO_LOG(ERROR) << "Failed to get access token for enclave";
      state_ = State::kStop;
      return;
    }
    CHECK(absl::holds_alternative<AccessToken>(event)) << ToString(event);

    state_ = State::kRegisteringWithEnclave;
    std::string token = std::move(absl::get_if<AccessToken>(&event)->value());
    enclave::Transact(manager_->network_context_factory_,
                      enclave::GetEnclaveIdentity(), std::move(token),
                      /*reauthentication_token=*/std::nullopt,
                      BuildRegistrationMessage(user_->device_id(),
                                               manager_->hardware_key_->key(),
                                               manager_->user_verifying_key_),
                      enclave::SigningCallback(),
                      base::BindOnce(&StateMachine::OnEnclaveResponse,
                                     weak_ptr_factory_.GetWeakPtr()));
  }

  void DoRegisteringWithEnclave(Event event) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (absl::holds_alternative<Failure>(event)) {
      state_ = State::kStop;
      return;
    }

    cbor::Value response =
        std::move(absl::get_if<EnclaveResponse>(&event)->value());
    if (!IsAllOk(response, 2)) {
      FIDO_LOG(ERROR) << "Registration resulted in error response: "
                      << cbor::DiagnosticWriter::Write(response);
      state_ = State::kStop;
      return;
    }

    if (!SetSecurityDomainMemberKey(
            user_, response.GetArray()[1]
                       .GetMap()
                       .find(cbor::Value(enclave::kResponseSuccessKey))
                       ->second)) {
      FIDO_LOG(ERROR) << "Wrapped member key was invalid: "
                      << cbor::DiagnosticWriter::Write(response);
      state_ = State::kStop;
      return;
    }

    user_->set_registered(true);
    manager_->WriteState(&local_state_);
    state_ = State::kNextAction;
  }

  void DoWaitingForEnclaveTokenForWrapping(Event event) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    access_token_fetcher_.reset();
    if (absl::holds_alternative<Failure>(event)) {
      FIDO_LOG(ERROR) << "Failed to get access token for enclave";
      state_ = State::kStop;
      return;
    }

    state_ = State::kWrappingSecrets;
    std::string token = std::move(absl::get_if<AccessToken>(&event)->value());
    enclave::Transact(
        manager_->network_context_factory_, enclave::GetEnclaveIdentity(),
        std::move(token),
        /*reauthentication_token=*/std::nullopt,
        cbor::Value(
            BuildSecretWrappingEnclaveRequest(new_security_domain_secrets_)),
        manager_->HardwareKeySigningCallback(),
        base::BindOnce(
            [](base::WeakPtr<StateMachine> machine,
               std::optional<cbor::Value> response) {
              if (!machine) {
                return;
              }
              if (!response) {
                machine->Process(Failure());
              } else {
                machine->Process(EnclaveResponse(std::move(*response)));
              }
            },
            weak_ptr_factory_.GetWeakPtr()));
  }

  void DoWrappingSecrets(Event event) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    const auto new_security_domain_secrets =
        std::move(new_security_domain_secrets_);
    new_security_domain_secrets_.clear();

    if (absl::holds_alternative<Failure>(event)) {
      FIDO_LOG(ERROR) << "Failed to wrap security domain secrets";
      state_ = State::kStop;
      return;
    }

    cbor::Value response =
        std::move(absl::get_if<EnclaveResponse>(&event)->value());
    if (!IsAllOk(response, new_security_domain_secrets.size())) {
      FIDO_LOG(ERROR) << "Wrapping resulted in error response: "
                      << cbor::DiagnosticWriter::Write(response);
      state_ = State::kStop;
      return;
    }

    if (!StoreWrappedSecrets(user_, new_security_domain_secrets,
                             response.GetArray())) {
      FIDO_LOG(ERROR) << "Failed to store wrapped secrets";
      state_ = State::kStop;
      return;
    }

    if (action_->wrapped_pin) {
      *user_->mutable_wrapped_pin() = std::move(*action_->wrapped_pin);
      user_->set_pin_public_key(std::move(*action_->pin_public_key));
      action_->wrapped_pin.reset();
    }

    if (!user_->joined()) {
      JoinSecurityDomain();
    } else {
      manager_->WriteState(&local_state_);
      state_ = State::kNextAction;
    }
  }

  void DoJoiningDomain(Event event) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    join_request_.reset();

    manager_->SetSecret(store_keys_args_for_joining_->last_key_version,
                        *store_keys_args_for_joining_->keys.rbegin());
    store_keys_args_for_joining_.reset();

    CHECK(absl::holds_alternative<JoinStatus>(event));
    const trusted_vault::TrustedVaultRegistrationStatus status =
        absl::get_if<JoinStatus>(&event)->value().first;

    switch (status) {
      case trusted_vault::TrustedVaultRegistrationStatus::kSuccess:
      case trusted_vault::TrustedVaultRegistrationStatus::kAlreadyRegistered:
        user_->set_joined(true);
        break;
      default:
        user_->mutable_wrapped_security_domain_secrets()->clear();
        break;
    }

    manager_->WriteState(&local_state_);
    state_ = State::kNextAction;
  }

  void DoHashingPIN(Event event) {
    // The new PIN has been hashed. Next we fetch the public keys of the
    // recovery key store.
    CHECK(absl::holds_alternative<PINHashed>(event));
    hashed_pin_ = std::move(absl::get_if<PINHashed>(&event)->value());

    int64_t generation = 0;
    if (is_pin_update_) {
      generation = user_->wrapped_pin().generation() + 1;
    }
    wrapped_pin_proto_ = hashed_pin_->ToWrappedPIN(generation);

    cert_xml_loader_ = FetchURL(
        manager_->url_loader_factory_.get(),
        device::enclave::kRecoveryKeyStoreCertFileURL,
        base::BindOnce(&StateMachine::FetchComplete,
                       weak_ptr_factory_.GetWeakPtr(), FetchedFile::kCertFile));
    sig_xml_loader_ = FetchURL(
        manager_->url_loader_factory_.get(),
        device::enclave::kRecoveryKeyStoreSigFileURL,
        base::BindOnce(&StateMachine::FetchComplete,
                       weak_ptr_factory_.GetWeakPtr(), FetchedFile::kSigFile));
    state_ = State::kDownloadingRecoveryKeyStoreKeys;
  }

  void DoDownloadingRecoveryKeyStoreKeys(Event event) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    CHECK(absl::holds_alternative<FileFetched>(event)) << ToString(event);
    auto& file_fetched = absl::get_if<FileFetched>(&event)->value();
    const FetchedFile fetched_file = file_fetched.first;
    std::optional<std::string>& contents = file_fetched.second;

    switch (fetched_file) {
      case FetchedFile::kCertFile:
        cert_xml_loader_.reset();
        cert_xml_ = std::move(contents);
        break;

      case FetchedFile::kSigFile:
        sig_xml_loader_.reset();
        sig_xml_ = std::move(contents);
        break;
    }

    if (cert_xml_loader_ || sig_xml_loader_) {
      // One of the fetches is still running.
      return;
    }

    if (!cert_xml_ || !sig_xml_) {
      // One (or both) fetches failed.
      state_ = State::kStop;
      return;
    }

    state_ = State::kWaitingForEnclaveTokenForPINWrapping;
    GetAccessTokenInternal(GaiaConstants::kPasskeysEnclaveOAuth2Scope);
  }

  void DoWaitingForEnclaveTokenForPINWrapping(Event event) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    access_token_fetcher_.reset();
    if (absl::holds_alternative<Failure>(event)) {
      FIDO_LOG(ERROR) << "Failed to get access token for enclave";
      state_ = State::kStop;
      return;
    }
    CHECK(absl::holds_alternative<AccessToken>(event)) << ToString(event);
    std::string token = std::move(absl::get_if<AccessToken>(&event)->value());

    if (is_pin_update_) {
      // The commands to send to the enclave are different if changing a PIN.
      SendPINChangeRequest(std::move(token));
      return;
    }

    // We have everything needed to make the enclave request to wrap the hashed
    // PIN for transmission to the recovery key store.
    state_ = State::kWrappingPIN;
    enclave::Transact(
        manager_->network_context_factory_, enclave::GetEnclaveIdentity(),
        std::move(token),
        /*reauthentication_token=*/std::nullopt,
        ConcatEnclaveRequests(
            BuildRecoveryKeyStorePINWrappingEnclaveRequest(
                hashed_pin_->hashed, std::move(*cert_xml_),
                std::move(*sig_xml_)),
            BuildSecretWrappingEnclaveRequest(
                GetNewSecretsToStore(*user_, *store_keys_args_for_joining_))),
        manager_->HardwareKeySigningCallback(),
        base::BindOnce(&StateMachine::OnEnclaveResponse,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void SendPINChangeRequest(std::string token) {
    const bool has_rapt = rapt_.has_value();

    state_ = State::kChangingPIN;
    std::vector<uint8_t> wrapped_secret =
        GetCurrentWrappedSecretForUser(user_).second;
    enclave::Transact(
        manager_->network_context_factory_, enclave::GetEnclaveIdentity(),
        std::move(token), std::move(rapt_),
        // The enclave needs to do two things:
        //   1) Encrypt the PIN hash with the security domain secret,
        //      effectively "blessing" it as a valid PIN.
        //   2) Encrypt the security domain secret to the recovery key store
        //      under the new PIN, so that the security domain can be recovered
        //      with that PIN in the future.
        ConcatEnclaveRequests(
            BuildPINWrappingEnclaveRequest(
                hashed_pin_->hashed, wrapped_pin_proto_->generation(),
                ToSizedSpan<32>(wrapped_pin_proto_->claim_key()),
                wrapped_secret),
            BuildRecoveryKeyStorePINChangeEnclaveRequest(
                hashed_pin_->hashed, std::move(*cert_xml_),
                std::move(*sig_xml_), wrapped_secret)),
        // These operations require more authentication than just the hardware
        // key. Thus either we have a reauthentication proof token to send to
        // the enclave, or else we need to use the UV key.
        //
        // (The default `UVKeyOptions` will cause the system UV prompt to appear
        // on macOS. This isn't ideal, but changing the GPM PIN is an
        // infrequent operation.)
        has_rapt ? manager_->HardwareKeySigningCallback()
                 : manager_->UserVerifyingKeySigningCallback(UVKeyOptions()),
        base::BindOnce(&StateMachine::OnEnclaveResponse,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void DoWrappingPIN(Event event) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (absl::holds_alternative<Failure>(event)) {
      state_ = State::kStop;
      return;
    }

    cbor::Value response =
        std::move(absl::get_if<EnclaveResponse>(&event)->value());
    if (!IsAllOk(response, 2)) {
      FIDO_LOG(ERROR) << "PIN wrapping resulted in error response: "
                      << cbor::DiagnosticWriter::Write(response);
      state_ = State::kStop;
      return;
    }

    const cbor::Value& recovery_key_store_wrap_response =
        response.GetArray()[0]
            .GetMap()
            .find(cbor::Value(enclave::kResponseSuccessKey))
            ->second;

    std::optional<std::unique_ptr<trusted_vault_pb::Vault>> vault =
        RecoveryKeyStoreWrapResponseToProto(hashed_pin_->salt, hashed_pin_->n,
                                            hashed_pin_->is_six_digits,
                                            recovery_key_store_wrap_response);
    if (!vault) {
      FIDO_LOG(ERROR)
          << "Failed to translate response into an UpdateVaultProto";
      state_ = State::kStop;
      return;
    }
    vault_ = std::move(*vault);

    wrapping_response_ = std::move(response);

    state_ = State::kWaitingForRecoveryKeyStore;
    recovery_key_store_request_ =
        manager_->recovery_key_store_conn_->UpdateRecoveryKeyStore(
            *primary_account_info_, *vault_,
            base::BindOnce(
                [](base::WeakPtr<StateMachine> machine,
                   trusted_vault::UpdateRecoveryKeyStoreStatus status) {
                  if (!machine) {
                    return;
                  }
                  machine->Process(status);
                },
                weak_ptr_factory_.GetWeakPtr()));
  }

  void DoWaitingForRecoveryKeyStore(Event event) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    recovery_key_store_request_.reset();
    CHECK(absl::holds_alternative<trusted_vault::UpdateRecoveryKeyStoreStatus>(
        event))
        << ToString(event);

    const auto* status =
        absl::get_if<trusted_vault::UpdateRecoveryKeyStoreStatus>(&event);
    if (*status != trusted_vault::UpdateRecoveryKeyStoreStatus::kSuccess) {
      FIDO_LOG(ERROR) << "Failed to upload to recovery key store";
      state_ = State::kStop;
      return;
    }

    if (!is_pin_update_) {
      CHECK(wrapped_pin_proto_->wrapped_pin().empty());
      wrapped_pin_proto_->set_wrapped_pin(BuildWrappedPIN(
          *hashed_pin_, /*generation=*/0,
          ToSizedSpan<32>(wrapped_pin_proto_->claim_key()), vault_.get(),
          store_keys_args_for_joining_->keys.back()));
    }
    const std::string& vault_public_key =
        vault_->application_keys()[0].asymmetric_key_pair().public_key();
    const auto secure_box_pub_key =
        trusted_vault::SecureBoxPublicKey::CreateByImport(
            ToSpan(vault_public_key));

    std::string wrapped_pin_proto_serialized =
        wrapped_pin_proto_->SerializeAsString();
    *user_->mutable_wrapped_pin() = std::move(*wrapped_pin_proto_);
    const std::string previous_pin_public_key = user_->pin_public_key();
    // If changing the PIN, there must be a previous PIN member public key.
    // If setting a first PIN, there must not be one.
    CHECK(!previous_pin_public_key.empty() == is_pin_update_);
    user_->set_pin_public_key(vault_public_key);

    state_ = is_pin_update_ ? State::kJoiningUpdatedPINToDomain
                            : State::kJoiningPINToDomain;
    std::optional<trusted_vault::MemberKeysSource> member_keys_source =
        std::move(member_keys_source_);
    // If changing a PIN then `member_keys_source` will have been populated by
    // the enclave. Otherwise a new PIN is being set and
    // `store_keys_args_for_joining_` will contain the security domain secret,
    // which is sufficient for calculating the member keys.
    CHECK_EQ(member_keys_source.has_value(), is_pin_update_);
    if (!member_keys_source) {
      member_keys_source = trusted_vault::GetTrustedVaultKeysWithVersions(
          store_keys_args_for_joining_->keys,
          store_keys_args_for_joining_->last_key_version);
    }
    join_request_ = manager_->trusted_vault_conn_->RegisterAuthenticationFactor(
        *primary_account_info_, std::move(*member_keys_source),
        *secure_box_pub_key,
        trusted_vault::GpmPinMetadata(previous_pin_public_key,
                                      std::move(wrapped_pin_proto_serialized)),
        base::BindOnce(&StateMachine::OnJoinedSecurityDomain,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void DoJoiningPINToDomain(Event event) {
    CHECK(absl::holds_alternative<JoinStatus>(event)) << ToString(event);

    wrapped_pin_proto_.reset();

    const auto& join_status = absl::get_if<JoinStatus>(&event)->value();
    const trusted_vault::TrustedVaultRegistrationStatus status =
        join_status.first;
    const int key_version = join_status.second;

    if (status != trusted_vault::TrustedVaultRegistrationStatus::kSuccess) {
      state_ = State::kStop;
      return;
    }

    store_keys_args_for_joining_->last_key_version = key_version;

    if (!StoreWrappedSecrets(
            user_, GetNewSecretsToStore(*user_, *store_keys_args_for_joining_),
            base::span<const cbor::Value>(&wrapping_response_->GetArray()[1],
                                          1ul))) {
      FIDO_LOG(ERROR) << "Secret wrapping resulted in malformed resposne: "
                      << cbor::DiagnosticWriter::Write(*wrapping_response_);
      state_ = State::kStop;
      return;
    }

    JoinSecurityDomain();
  }

  void DoJoiningUpdatedPINToDomain(Event event) {
    CHECK(absl::holds_alternative<JoinStatus>(event)) << ToString(event);

    wrapped_pin_proto_.reset();

    const auto& join_status = absl::get_if<JoinStatus>(&event)->value();
    const trusted_vault::TrustedVaultRegistrationStatus status =
        join_status.first;

    success_ =
        status == trusted_vault::TrustedVaultRegistrationStatus::kSuccess;
    if (success_) {
      manager_->WriteState(&local_state_);
    }
    state_ = State::kStop;
  }

  void DoChangingPIN(Event event) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    state_ = State::kStop;
    if (absl::holds_alternative<Failure>(event)) {
      return;
    }

    cbor::Value response =
        std::move(absl::get_if<EnclaveResponse>(&event)->value());
    if (!IsAllOk(response, 2)) {
      FIDO_LOG(ERROR) << "PIN change resulted in error response: "
                      << cbor::DiagnosticWriter::Write(response);
      return;
    }

    const cbor::Value& wrapped_pin_value =
        response.GetArray()[0]
            .GetMap()
            .find(cbor::Value(enclave::kResponseSuccessKey))
            ->second;
    if (!wrapped_pin_value.is_bytestring()) {
      FIDO_LOG(ERROR) << "Wrapped PIN was not a bytestring";
      return;
    }
    wrapped_pin_proto_->set_wrapped_pin(
        VecToString(wrapped_pin_value.GetBytestring()));

    const cbor::Value& wrap_as_member_response_value =
        response.GetArray()[1]
            .GetMap()
            .find(cbor::Value(enclave::kResponseSuccessKey))
            ->second;
    if (!wrap_as_member_response_value.is_map()) {
      FIDO_LOG(ERROR) << "wrap_as_member response was not a map";
      return;
    }
    const cbor::Value::MapValue& wrap_as_member_response =
        wrap_as_member_response_value.GetMap();

    auto it = wrap_as_member_response.find(cbor::Value("wrapped"));
    if (it == wrap_as_member_response.end()) {
      FIDO_LOG(ERROR) << "wrap_as_member response missing 'wrapped'";
      return;
    }
    std::optional<std::unique_ptr<trusted_vault_pb::Vault>> vault =
        RecoveryKeyStoreWrapResponseToProto(hashed_pin_->salt, hashed_pin_->n,
                                            hashed_pin_->is_six_digits,
                                            it->second);
    if (!vault) {
      FIDO_LOG(ERROR)
          << "Failed to translate response into an UpdateVaultProto";
      return;
    }
    vault_ = std::move(*vault);

    it = wrap_as_member_response.find(cbor::Value("wrapped_sds"));
    if (it == wrap_as_member_response.end() || !it->second.is_bytestring()) {
      FIDO_LOG(ERROR) << "wrap_as_member has invalid 'wrapped_sds'";
      return;
    }
    const std::vector<uint8_t>& wrapped_sds = it->second.GetBytestring();

    it = wrap_as_member_response.find(cbor::Value("member_proof"));
    if (it == wrap_as_member_response.end() || !it->second.is_bytestring()) {
      FIDO_LOG(ERROR) << "wrap_as_member has invalid 'member_proof'";
      return;
    }
    const std::vector<uint8_t>& member_proof = it->second.GetBytestring();

    int32_t key_version = GetCurrentWrappedSecretForUser(user_).first;
    member_keys_source_ = trusted_vault::PrecomputedMemberKeys(
        key_version, wrapped_sds, member_proof);

    state_ = State::kWaitingForRecoveryKeyStore;
    recovery_key_store_request_ =
        manager_->recovery_key_store_conn_->UpdateRecoveryKeyStore(
            *primary_account_info_, *vault_,
            base::BindOnce(
                [](base::WeakPtr<StateMachine> machine,
                   trusted_vault::UpdateRecoveryKeyStoreStatus status) {
                  if (!machine) {
                    return;
                  }
                  machine->Process(status);
                },
                weak_ptr_factory_.GetWeakPtr()));
  }

  void JoinSecurityDomain() {
    state_ = State::kJoiningDomain;
    const auto secure_box_pub_key =
        trusted_vault::SecureBoxPublicKey::CreateByImport(
            ToSpan(user_->member_public_key()));
    join_request_ = manager_->trusted_vault_conn_->RegisterAuthenticationFactor(
        *primary_account_info_,
        trusted_vault::GetTrustedVaultKeysWithVersions(
            store_keys_args_for_joining_->keys,
            store_keys_args_for_joining_->last_key_version),
        *secure_box_pub_key, trusted_vault::PhysicalDevice(),
        base::BindOnce(&StateMachine::OnJoinedSecurityDomain,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void GetAccessTokenInternal(const char* scope) {
    access_token_fetcher_ =
        std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
            "passkeys_enclave", manager_->identity_manager_,
            signin::ScopeSet{scope},
            base::BindOnce(
                [](base::WeakPtr<StateMachine> machine,
                   GoogleServiceAuthError error,
                   signin::AccessTokenInfo access_token_info) {
                  if (!machine) {
                    return;
                  }
                  if (error.state() == GoogleServiceAuthError::NONE) {
                    machine->Process(AccessToken(access_token_info.token));
                  } else {
                    machine->Process(Failure());
                  }
                },
                weak_ptr_factory_.GetWeakPtr()),
            signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,
            signin::ConsentLevel::kSignin);
  }

  void OnEnclaveResponse(std::optional<cbor::Value> response) {
    if (!response) {
      Process(Failure());
    } else {
      Process(EnclaveResponse(std::move(*response)));
    }
  }

  void OnJoinedSecurityDomain(
      trusted_vault::TrustedVaultRegistrationStatus status,
      int key_version) {
    Process(JoinStatus(std::make_pair(status, key_version)));
  }

  void HashPIN(std::string pin) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::USER_BLOCKING, base::MayBlock()},
        base::BindOnce(&HashPINSlowly, std::move(pin)),
        base::BindOnce(
            [](base::WeakPtr<StateMachine> machine,
               std::unique_ptr<HashedPIN> hashed) {
              if (!machine) {
                return;
              }
              machine->Process(PINHashed(std::move(hashed)));
            },
            weak_ptr_factory_.GetWeakPtr()));
  }

  // Constructed a wrapped version of the hashed PIN that will be part of the
  // virtual member metadata. This inner CBOR structure contains everything that
  // the enclave would need when processing a PIN and is authenticated (and
  // encrypted) by the security domain secret.
  static std::string BuildWrappedPIN(
      const HashedPIN& hashed_pin,
      int64_t generation,
      base::span<const uint8_t, 32> claim_key,
      const trusted_vault_pb::Vault* vault,
      base::span<const uint8_t> security_domain_secret) {
    cbor::Value::MapValue map;
    map.emplace(1, base::span<const uint8_t>(hashed_pin.hashed));
    map.emplace(2, generation);
    map.emplace(3, claim_key);
    map.emplace(4, ToSpan(vault->vault_parameters().counter_id()));
    map.emplace(5, ToSpan(vault->vault_parameters().vault_handle()));
    const std::vector<uint8_t> cbor_bytes =
        cbor::Writer::Write(cbor::Value(std::move(map))).value();
    return VecToString(EncryptWrappedPIN(security_domain_secret, cbor_bytes));
  }

  const raw_ptr<EnclaveManager> manager_;
  // local_state_ contains a copy of the EnclaveManager's state from when this
  // StateMachine was created.
  EnclaveLocalState local_state_;
  // user_ points within `local_state_` to the state for the user specified in
  // `primary_account_info_`.
  const raw_ptr<EnclaveLocalState::User> user_;
  const std::unique_ptr<CoreAccountInfo> primary_account_info_;

  bool success_ = false;
  State state_ = State::kNextAction;
  bool processing_ = false;

  const std::unique_ptr<EnclaveManager::PendingAction> action_;

  std::unique_ptr<StoreKeysArgs> store_keys_args_for_joining_;
  base::flat_map<int32_t, std::vector<uint8_t>> new_security_domain_secrets_;
  std::unique_ptr<trusted_vault::TrustedVaultConnection::Request> join_request_;
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;
  std::unique_ptr<network::SimpleURLLoader> cert_xml_loader_;
  std::unique_ptr<network::SimpleURLLoader> sig_xml_loader_;
  std::unique_ptr<network::SimpleURLLoader> upload_loader_;
  std::optional<std::string> cert_xml_;
  std::optional<std::string> sig_xml_;
  std::unique_ptr<HashedPIN> hashed_pin_;
  std::unique_ptr<trusted_vault_pb::Vault> vault_;
  std::unique_ptr<trusted_vault::RecoveryKeyStoreConnection::Request>
      recovery_key_store_request_;
  std::optional<cbor::Value> wrapping_response_;
  // True if a PIN is being hashed in order to change it, rather than to set
  // a new PIN on an account.
  bool is_pin_update_ = false;
  // If changing a PIN, this holds a ReAuthentication Proof Token (RAPT), if
  // the user is authenticating the request via doing a GAIA reauth.
  std::optional<std::string> rapt_;
  // If present, these keys will be used for adding the PIN to the domain.
  std::optional<trusted_vault::MemberKeysSource> member_keys_source_;
  // When uploading a PIN, this contains the pending `WrappedPIN`.
  std::unique_ptr<EnclaveLocalState::WrappedPIN> wrapped_pin_proto_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<StateMachine> weak_ptr_factory_{this};
};

EnclaveManager::UVKeyOptions::UVKeyOptions() = default;
EnclaveManager::UVKeyOptions::~UVKeyOptions() = default;
EnclaveManager::UVKeyOptions::UVKeyOptions(UVKeyOptions&&) = default;
EnclaveManager::UVKeyOptions& EnclaveManager::UVKeyOptions::operator=(
    EnclaveManager::UVKeyOptions&& other) = default;

EnclaveManager::EnclaveManager(
    const base::FilePath& base_dir,
    signin::IdentityManager* identity_manager,
    device::NetworkContextFactory network_context_factory,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : file_path_(base_dir.Append(FILE_PATH_LITERAL("passkey_enclave_state"))),
      identity_manager_(identity_manager),
      network_context_factory_(network_context_factory),
      url_loader_factory_(url_loader_factory),
      trusted_vault_conn_(trusted_vault::NewFrontendTrustedVaultConnection(
          trusted_vault::SecurityDomainId::kPasskeys,
          identity_manager,
          url_loader_factory_)),
      trusted_vault_access_token_fetcher_frontend_(
          std::make_unique<
              trusted_vault::TrustedVaultAccessTokenFetcherFrontend>(
              identity_manager_)),
      recovery_key_store_conn_(std::make_unique<
                               trusted_vault::RecoveryKeyStoreConnectionImpl>(
          url_loader_factory_->Clone(),
          std::make_unique<trusted_vault::TrustedVaultAccessTokenFetcherImpl>(
              trusted_vault_access_token_fetcher_frontend_->GetWeakPtr()))),
      identity_observer_(
          std::make_unique<IdentityObserver>(identity_manager_, this)) {}

EnclaveManager::~EnclaveManager() = default;

bool EnclaveManager::is_idle() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !loading_ && !state_machine_;
}

bool EnclaveManager::is_loaded() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return static_cast<bool>(local_state_);
}

bool EnclaveManager::is_registered() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return user_ && user_->registered();
}

bool EnclaveManager::has_pending_keys() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pending_keys_ != nullptr;
}

bool EnclaveManager::is_ready() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_registered() && !user_->wrapped_security_domain_secrets().empty();
}

unsigned EnclaveManager::store_keys_count() const {
  return store_keys_count_;
}

void EnclaveManager::Load(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (is_loaded()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(closure));
    return;
  }
  load_callbacks_.emplace_back(std::move(closure));
  Act();
}

void EnclaveManager::RegisterIfNeeded(EnclaveManager::Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (user_ && user_->registered()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
    return;
  }

  auto action = std::make_unique<PendingAction>();
  action->callback = std::move(callback);
  action->want_registration = true;
  pending_actions_.emplace_back(std::move(action));
  Act();
}

void EnclaveManager::SetupWithPIN(std::string pin,
                                  EnclaveManager::Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto action = std::make_unique<PendingAction>();
  action->callback = std::move(callback);
  action->pin = std::move(pin);
  action->setup_account = true;
  pending_actions_.emplace_back(std::move(action));
  Act();
}

bool EnclaveManager::AddDeviceToAccount(
    std::optional<trusted_vault::GpmPinMetadata> pin_metadata,
    EnclaveManager::Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(has_pending_keys());

  std::unique_ptr<EnclaveLocalState::WrappedPIN> wrapped_pin;
  if (pin_metadata.has_value()) {
    wrapped_pin = std::make_unique<EnclaveLocalState::WrappedPIN>();
    if (!wrapped_pin->ParseFromString(pin_metadata->wrapped_pin) ||
        CheckPINInvariants(*wrapped_pin).has_value()) {
      return false;
    }
  }

  auto action = std::make_unique<PendingAction>();
  action->callback = std::move(callback);
  action->store_keys_args = std::move(pending_keys_);
  action->wrapped_pin = std::move(wrapped_pin);
  if (pin_metadata) {
    action->pin_public_key = std::move(pin_metadata->public_key);
  }
  pending_actions_.emplace_back(std::move(action));
  Act();
  return true;
}

void EnclaveManager::AddDeviceAndPINToAccount(
    std::string pin,
    EnclaveManager::Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto action = std::make_unique<PendingAction>();
  action->callback = std::move(callback);
  action->store_keys_args = std::move(pending_keys_);
  action->pin = std::move(pin);
  pending_actions_.emplace_back(std::move(action));
  Act();
}

void EnclaveManager::ChangePIN(std::string updated_pin,
                               std::optional<std::string> rapt,
                               EnclaveManager::Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(user_->registered());
  CHECK(user_->has_wrapped_pin());
  CHECK(rapt.has_value() || uv_key_state() != UvKeyState::kNone);

  auto action = std::make_unique<PendingAction>();
  action->callback = std::move(callback);
  action->updated_pin = std::move(updated_pin);
  action->rapt = std::move(rapt);
  pending_actions_.emplace_back(std::move(action));
  Act();
}

void EnclaveManager::GetHardwareKeyForSignature(
    base::OnceCallback<void(
        scoped_refptr<unexportable_keys::RefCountedUnexportableSigningKey>)>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!user_ || user_->wrapped_hardware_private_key().empty()) {
    std::move(callback).Run(nullptr);
    return;
  }

  if (hardware_key_) {
    std::move(callback).Run(hardware_key_);
    return;
  }

  auto key_callback = base::BindOnce(
      [](base::WeakPtr<EnclaveManager> enclave_manager,
         CoreAccountId account_id,
         base::OnceCallback<void(
             scoped_refptr<
                 unexportable_keys::RefCountedUnexportableSigningKey>)>
             callback,
         std::unique_ptr<crypto::UnexportableSigningKey> key) {
        if (!enclave_manager ||
            enclave_manager->primary_account_info_->account_id != account_id) {
          std::move(callback).Run(nullptr);
          return;
        }
        DCHECK_CALLED_ON_VALID_SEQUENCE(enclave_manager->sequence_checker_);
        if (!key) {
          enclave_manager->ClearRegistration();
          std::move(callback).Run(nullptr);
          return;
        }
        enclave_manager->hardware_key_ = base::MakeRefCounted<
            unexportable_keys::RefCountedUnexportableSigningKey>(
            std::move(key), unexportable_keys::UnexportableKeyId());
        std::move(callback).Run(enclave_manager->hardware_key_);
      },
      weak_ptr_factory_.GetWeakPtr(), primary_account_info_->account_id,
      std::move(callback));

  // Retrieve the key on a non-UI thread, and post a task back to the current
  // thread that invokes `key_callback` with the obtained key.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(
          [](std::string wrapped_hardware_private_key)
              -> std::unique_ptr<crypto::UnexportableSigningKey> {
            std::unique_ptr<crypto::UnexportableKeyProvider> provider =
                GetUnexportableKeyProvider();
            if (!provider) {
              return nullptr;
            }
            return provider->FromWrappedSigningKeySlowly(
                ToVector(wrapped_hardware_private_key));
          },
          user_->wrapped_hardware_private_key()),
      std::move(key_callback));
}

enclave::SigningCallback EnclaveManager::HardwareKeySigningCallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!user_->wrapped_hardware_private_key().empty());
  CHECK(user_->registered());

  return base::BindOnce(
      [](base::WeakPtr<EnclaveManager> enclave_manager,
         enclave::SignedMessage message_to_be_signed,
         base::OnceCallback<void(std::optional<enclave::ClientSignature>)>
             result_callback) {
        if (!enclave_manager || !enclave_manager->user_) {
          std::move(result_callback).Run(std::nullopt);
          return;
        }
        DCHECK_CALLED_ON_VALID_SEQUENCE(enclave_manager->sequence_checker_);

        auto signing_callback = base::BindOnce(
            [](std::string device_id,
               enclave::SignedMessage message_to_be_signed,
               base::OnceCallback<void(std::optional<enclave::ClientSignature>)>
                   result_callback,
               scoped_refptr<
                   unexportable_keys::RefCountedUnexportableSigningKey> key) {
              if (!key) {
                std::move(result_callback).Run(std::nullopt);
                return;
              }
              base::ThreadPool::PostTaskAndReplyWithResult(
                  FROM_HERE,
                  {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
                  base::BindOnce(
                      [](std::string device_id,
                         enclave::SignedMessage message_to_be_signed,
                         scoped_refptr<unexportable_keys::
                                           RefCountedUnexportableSigningKey>
                             key) -> std::optional<enclave::ClientSignature> {
                        std::optional<std::vector<uint8_t>> signature =
                            key->key().SignSlowly(message_to_be_signed);
                        if (!signature) {
                          return std::nullopt;
                        }
                        enclave::ClientSignature client_signature;
                        client_signature.device_id = ToVector(device_id);
                        client_signature.signature = std::move(*signature);
                        client_signature.key_type =
                            enclave::ClientKeyType::kHardware;
                        return std::move(client_signature);
                      },
                      std::move(device_id), std::move(message_to_be_signed),
                      key),
                  std::move(result_callback));
            },
            enclave_manager->user_->device_id(),
            std::move(message_to_be_signed), std::move(result_callback));

        enclave_manager->GetHardwareKeyForSignature(
            std::move(signing_callback));
      },
      weak_ptr_factory_.GetWeakPtr());
}

void EnclaveManager::GetUserVerifyingKeyForSignature(
    UVKeyOptions options,
    base::OnceCallback<void(
        scoped_refptr<crypto::RefCountedUserVerifyingSigningKey>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!user_ || user_->wrapped_uv_private_key().empty()) {
    std::move(callback).Run(nullptr);
    return;
  }

#if BUILDFLAG(IS_WIN)
  // On Windows, retrieving the UV key is slow so we cache it. On Mac, we avoid
  // caching the key as we need to use a fresh LAContext every time we retrieve
  // the key.
  if (user_verifying_key_) {
    std::move(callback).Run(user_verifying_key_);
    return;
  }
#endif  // BUILDFLAG(IS_WIN)

  auto user_verifying_key_provider = crypto::GetUserVerifyingKeyProvider(
      MakeUserVerifyingKeyConfig(std::move(options)));
  if (!user_verifying_key_provider) {
    // This indicates the platform key provider was available, but now is not.
    ClearRegistration();
    std::move(callback).Run(nullptr);
    return;
  }

  auto key_callback = base::BindOnce(
      [](base::WeakPtr<EnclaveManager> enclave_manager,
         CoreAccountId account_id,
         base::OnceCallback<void(
             scoped_refptr<crypto::RefCountedUserVerifyingSigningKey>)>
             callback,
         std::unique_ptr<crypto::UserVerifyingSigningKey> key) {
        if (!enclave_manager ||
            enclave_manager->primary_account_info_->account_id != account_id) {
          std::move(callback).Run(nullptr);
          return;
        }
        if (!key) {
          enclave_manager->ClearRegistration();
          std::move(callback).Run(nullptr);
          return;
        }
        enclave_manager->user_verifying_key_ =
            base::MakeRefCounted<crypto::RefCountedUserVerifyingSigningKey>(
                std::move(key));
        std::move(callback).Run(enclave_manager->user_verifying_key_);
      },
      weak_ptr_factory_.GetWeakPtr(), primary_account_info_->account_id,
      std::move(callback));

  auto key_label =
      UserVerifyingKeyLabelFromString(user_->wrapped_uv_private_key());
  CHECK(key_label);

  user_verifying_key_provider->GetUserVerifyingSigningKey(
      std::move(*key_label), std::move(key_callback));
}

enclave::SigningCallback EnclaveManager::UserVerifyingKeySigningCallback(
    UVKeyOptions options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!user_->wrapped_uv_private_key().empty());
  CHECK(user_->registered());

  return base::BindOnce(
      [](UVKeyOptions options, base::WeakPtr<EnclaveManager> enclave_manager,
         enclave::SignedMessage message_to_be_signed,
         base::OnceCallback<void(std::optional<enclave::ClientSignature>)>
             result_callback) {
        if (!enclave_manager) {
          std::move(result_callback).Run(std::nullopt);
          return;
        }
        DCHECK_CALLED_ON_VALID_SEQUENCE(enclave_manager->sequence_checker_);

        auto signing_callback = base::BindOnce(
            [](std::string device_id,
               enclave::SignedMessage message_to_be_signed,
               base::OnceCallback<void(std::optional<enclave::ClientSignature>)>
                   result_callback,
               scoped_refptr<crypto::RefCountedUserVerifyingSigningKey>
                   uv_signing_key) {
              if (!uv_signing_key) {
                std::move(result_callback).Run(std::nullopt);
                return;
              }
              uv_signing_key->key().Sign(
                  message_to_be_signed,
                  base::BindOnce(
                      [](std::string device_id,
                         base::OnceCallback<void(
                             std::optional<enclave::ClientSignature>)>
                             result_callback,
                         std::optional<std::vector<uint8_t>> signature) {
                        if (!signature) {
                          std::move(result_callback).Run(std::nullopt);
                          return;
                        }
                        enclave::ClientSignature client_signature;
                        client_signature.device_id = ToVector(device_id);
                        client_signature.signature = std::move(*signature);
                        client_signature.key_type =
                            enclave::ClientKeyType::kUserVerified;
                        std::move(result_callback)
                            .Run(std::move(client_signature));
                      },
                      std::move(device_id), std::move(result_callback)));
            },
            enclave_manager->user_->device_id(),
            std::move(message_to_be_signed), std::move(result_callback));

        enclave_manager->GetUserVerifyingKeyForSignature(
            std::move(options), std::move(signing_callback));
      },
      std::move(options), weak_ptr_factory_.GetWeakPtr());
}

std::optional<std::vector<uint8_t>> EnclaveManager::GetWrappedSecret(
    int32_t version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(is_ready());
  const auto it = user_->wrapped_security_domain_secrets().find(version);
  if (it == user_->wrapped_security_domain_secrets().end()) {
    return std::nullopt;
  }
  return ToVector(it->second);
}

std::pair<int32_t, std::vector<uint8_t>>
EnclaveManager::GetCurrentWrappedSecret() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(is_ready());

  return GetCurrentWrappedSecretForUser(user_);
}

std::optional<std::pair<int32_t, std::vector<uint8_t>>>
EnclaveManager::TakeSecret() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (secret_.empty()) {
    return std::nullopt;
  }
  return std::make_pair(secret_version_, std::move(secret_));
}

bool EnclaveManager::has_wrapped_pin() const {
  CHECK(is_ready());
  return user_->has_wrapped_pin();
}

bool EnclaveManager::wrapped_pin_is_arbitrary() const {
  CHECK(has_wrapped_pin());
  return user_->wrapped_pin().form() ==
         EnclaveLocalState::WrappedPIN::FORM_ARBITRARY;
}

std::unique_ptr<webauthn_pb::EnclaveLocalState_WrappedPIN>
EnclaveManager::GetWrappedPIN() {
  CHECK(has_wrapped_pin());
  return std::make_unique<webauthn_pb::EnclaveLocalState_WrappedPIN>(
      user_->wrapped_pin());
}

EnclaveManager::UvKeyState EnclaveManager::uv_key_state() const {
  CHECK(is_ready());
  if (user_->wrapped_uv_private_key().empty()) {
    return UvKeyState::kNone;
  }
#if BUILDFLAG(IS_MAC)
  if (device::fido::mac::DeviceHasBiometricsAvailable()) {
    // LAAuthenticationView is only supported on macOS 12+.
    if (__builtin_available(macOS 12.0, *)) {
      // Chrome will display an LAAuthenticationView with a Touch ID prompt.
      return UvKeyState::kUsesChromeUI;
    }
  }
  // Delegate prompting the user for their screen lock to macOS.
  return UvKeyState::kUsesSystemUI;
#elif BUILDFLAG(IS_WIN)
  return UvKeyState::kUsesSystemUI;
#else
  return UvKeyState::kNone;
#endif
}

std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
EnclaveManager::GetAccessToken(
    base::OnceCallback<void(std::optional<std::string>)> callback) {
  return std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
      "passkeys_enclave", identity_manager_,
      signin::ScopeSet{GaiaConstants::kPasskeysEnclaveOAuth2Scope},
      base::BindOnce(
          [](base::OnceCallback<void(std::optional<std::string>)> callback,
             GoogleServiceAuthError error,
             signin::AccessTokenInfo access_token_info) {
            if (error.state() == GoogleServiceAuthError::NONE) {
              std::move(callback).Run(std::move(access_token_info.token));
            } else {
              FIDO_LOG(ERROR)
                  << "Failed to get access token: " << error.error_message();
              std::move(callback).Run(std::nullopt);
            }
          },
          std::move(callback)),
      signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
      signin::ConsentLevel::kSignin);
}

void EnclaveManager::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.AddObserver(observer);
}

void EnclaveManager::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.RemoveObserver(observer);
}

void EnclaveManager::StoreKeys(const std::string& gaia_id,
                               std::vector<std::vector<uint8_t>> keys,
                               int last_key_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  pending_keys_ = std::make_unique<StoreKeysArgs>();
  pending_keys_->gaia_id = gaia_id;
  pending_keys_->keys = std::move(keys);
  pending_keys_->last_key_version = last_key_version;

  store_keys_count_++;

  for (Observer& observer : observer_list_) {
    observer.OnKeysStored();
  }
}

std::unique_ptr<enclave::ClaimedPIN> EnclaveManager::MakeClaimedPINSlowly(
    std::string pin,
    std::unique_ptr<webauthn_pb::EnclaveLocalState_WrappedPIN> wrapped_pin) {
  uint8_t hashed[32];
  const std::string& salt = wrapped_pin->hash_salt();
  CHECK(EVP_PBE_scrypt(pin.data(), pin.size(),
                       reinterpret_cast<const uint8_t*>(salt.data()),
                       salt.size(), wrapped_pin->hash_difficulty(), 8, 1,
                       1ul << 28, hashed, sizeof(hashed)));

  static constexpr uint8_t kAAD[] = {'P', 'I', 'N', ' ', 'c',
                                     'l', 'a', 'i', 'm'};
  crypto::Aead aead(crypto::Aead::AeadAlgorithm::AES_256_GCM);
  aead.Init(ToSpan(wrapped_pin->claim_key()));
  uint8_t nonce[12];
  crypto::RandBytes(nonce);
  std::vector<uint8_t> ciphertext = aead.Seal(hashed, nonce, kAAD);
  ciphertext.insert(ciphertext.begin(), std::begin(nonce), std::end(nonce));

  return std::make_unique<enclave::ClaimedPIN>(
      std::move(ciphertext), ToVector(wrapped_pin->wrapped_pin()));
}

bool EnclaveManager::RunWhenStoppedForTesting(base::OnceClosure on_stop) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!state_machine_ && !loading_);
  if (!currently_writing_) {
    return false;
  }
  write_finished_callback_ = std::move(on_stop);
  return true;
}

EnclaveLocalState& EnclaveManager::local_state_for_testing() const {
  return *local_state_;
}

void EnclaveManager::ClearCachedKeysForTesting() {
  user_verifying_key_ = nullptr;
  hardware_key_ = nullptr;
}

// static
std::string EnclaveManager::MakeWrappedPINForTesting(
    base::span<const uint8_t> security_domain_secret,
    std::string_view pin) {
  constexpr int32_t kGeneration = 0;
  std::unique_ptr<HashedPIN> hashed = HashPINSlowly(pin);
  std::unique_ptr<EnclaveLocalState::WrappedPIN> wrapped_pin =
      hashed->ToWrappedPIN(kGeneration);
  const uint8_t kFakeCounterId[8] = {0};
  const uint8_t kFakeVaultHandle[16] = {0};

  cbor::Value::MapValue map;
  map.emplace(1, base::span<const uint8_t>(hashed->hashed));
  map.emplace(2, kGeneration);
  map.emplace(3, ToSizedSpan<32>(wrapped_pin->claim_key()));
  map.emplace(4, base::span<const uint8_t>(kFakeCounterId));
  map.emplace(5, base::span<const uint8_t>(kFakeVaultHandle));
  const std::vector<uint8_t> cbor_bytes =
      cbor::Writer::Write(cbor::Value(std::move(map))).value();
  wrapped_pin->set_wrapped_pin(
      VecToString(EncryptWrappedPIN(security_domain_secret, cbor_bytes)));
  return wrapped_pin->SerializeAsString();
}

// Observes the `IdentityManager` and tells the `EnclaveManager` when the
// primary account for the profile has changed.
class EnclaveManager::IdentityObserver
    : public signin::IdentityManager::Observer {
 public:
  IdentityObserver(signin::IdentityManager* identity_manager,
                   EnclaveManager* manager)
      : identity_manager_(identity_manager), manager_(manager) {
    identity_manager_->AddObserver(this);
  }

  ~IdentityObserver() override {
    if (observing_) {
      identity_manager_->RemoveObserver(this);
    }
  }

  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override {
    manager_->HandleIdentityChange();
  }

  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override {
    manager_->HandleIdentityChange();
  }

  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override {
    if (observing_) {
      identity_manager_->RemoveObserver(this);
      observing_ = false;
    }
  }

 private:
  bool observing_ = true;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  const raw_ptr<EnclaveManager> manager_;
};

void EnclaveManager::Act() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!local_state_) {
    if (loading_) {
      return;
    }

    loading_ = true;
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::USER_BLOCKING, base::MayBlock()},
        base::BindOnce(
            [](base::FilePath path) -> std::optional<std::string> {
              std::string contents, decrypted;
              if (!base::ReadFileToString(path, &contents) ||
                  !OSCrypt::DecryptString(contents, &decrypted)) {
                return std::nullopt;
              }

              return std::move(decrypted);
            },
            file_path_),
        base::BindOnce(&EnclaveManager::LoadComplete,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  if (!load_callbacks_.empty()) {
    std::vector<base::OnceClosure> callbacks = std::move(load_callbacks_);
    load_callbacks_.clear();

    for (auto& callback : callbacks) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(callback));
    }
  }

  if (pending_actions_.empty() || state_machine_) {
    return;
  }

  if (!user_) {
    CancelAllActions();
    return;
  }

  std::unique_ptr<PendingAction> action = std::move(pending_actions_.front());
  pending_actions_.pop_front();

  EnclaveLocalState copy;
  copy.CopyFrom(*local_state_);
  state_machine_ = std::make_unique<StateMachine>(
      this, std::move(copy),
      std::make_unique<CoreAccountInfo>(*primary_account_info_),
      std::move(action));
}

void EnclaveManager::LoadComplete(std::optional<std::string> contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  loading_ = false;
  if (contents) {
    local_state_ = ParseStateFile(std::move(*contents));
  } else {
    local_state_ = std::make_unique<EnclaveLocalState>();
  }

  for (const auto& it : local_state_->users()) {
    std::optional<int> error_line = CheckInvariants(it.second);
    if (error_line.has_value()) {
      FIDO_LOG(ERROR) << "State invariant failed on line " << *error_line;
      local_state_ = std::make_unique<EnclaveLocalState>();
      break;
    }
  }

  HandleIdentityChange(/*is_post_load=*/true);
  Act();
}

void EnclaveManager::HandleIdentityChange(bool is_post_load) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This function is called when local state finishes loading. Prior to that
  // identity changes are ignored.
  if (!local_state_) {
    return;
  }

  // If a state machine is running, there must be a current user.
  CHECK(!state_machine_ || user_);
  bool need_to_stop = true;

  CoreAccountInfo primary_account_info =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (!primary_account_info.IsEmpty()) {
    if (primary_account_info_ &&
        primary_account_info_->account_id != primary_account_info.account_id) {
      // If the signed-in user has changed, the state machine must be halted
      // because otherwise it could act on the wrong account.
      need_to_stop = true;
    }
    user_ = StateForUser(local_state_.get(), primary_account_info);
    if (!user_) {
      user_ = CreateStateForUser(local_state_.get(), primary_account_info);
    }
    if (pending_keys_ && pending_keys_->gaia_id != primary_account_info.gaia) {
      pending_keys_.reset();
    }
    primary_account_info_ =
        std::make_unique<CoreAccountInfo>(std::move(primary_account_info));
  } else {
    if (user_) {
      // If the users signs out, the state machine is stopped because it only
      // operates in the context of an account.
      need_to_stop = true;
    }
    user_ = nullptr;
    primary_account_info_.reset();
    pending_keys_.reset();
  }

  user_verifying_key_.reset();
  hardware_key_.reset();

  const signin::AccountsInCookieJarInfo in_jar =
      identity_manager_->GetAccountsInCookieJar();
  if (in_jar.accounts_are_fresh) {
    // If the user has signed out of any non-primary accounts, erase their
    // enclave state.
    const base::flat_set<std::string> gaia_ids_in_cookie_jar =
        base::STLSetUnion<base::flat_set<std::string>>(
            GetGaiaIDs(in_jar.signed_in_accounts),
            GetGaiaIDs(in_jar.signed_out_accounts));
    const base::flat_set<std::string> gaia_ids_in_state =
        GetGaiaIDs(local_state_->users());
    base::flat_set<std::string> to_remove =
        base::STLSetDifference<base::flat_set<std::string>>(
            gaia_ids_in_state, gaia_ids_in_cookie_jar);
    if (primary_account_info_) {
      to_remove.erase(primary_account_info_->gaia);
    }
    // A `StateMachine` can also mutate the enclave state. Thus if we're about
    // to mutate it ourselves, confirm that any `StateMachine` is about to be
    // stopped and thus cannot overwrite these changes.
    CHECK(need_to_stop);
    for (const auto& gaia_id : to_remove) {
      CHECK(local_state_->mutable_users()->erase(gaia_id));
    }
    WriteState(local_state_.get());
  }

  if (need_to_stop && !is_post_load) {
    CancelAllActions();
    Stopped();
  }
}

void EnclaveManager::Stopped() {
  state_machine_.reset();
  Act();
}

void EnclaveManager::CancelAllActions() {
  std::deque<std::unique_ptr<PendingAction>> actions =
      std::move(pending_actions_);
  pending_actions_.clear();

  for (const auto& action : actions) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(action->callback), false));
  }
}

void EnclaveManager::WriteState(EnclaveLocalState* new_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const auto& it : new_state->users()) {
    std::optional<int> error_line = CheckInvariants(it.second);
    CHECK(!error_line.has_value())
        << "State invariant failed on line " << *error_line;
  }

  std::string serialized;
  serialized.reserve(1024);
  new_state->AppendToString(&serialized);

  if (new_state != local_state_.get()) {
    user_ = nullptr;
    local_state_ = std::make_unique<EnclaveLocalState>();
    CHECK(local_state_->ParseFromString(serialized));
    user_ = StateForUser(local_state_.get(), *primary_account_info_);
  }

  const std::array<uint8_t, crypto::kSHA256Length> digest =
      crypto::SHA256Hash(base::as_bytes(base::make_span(serialized)));
  serialized.append(std::begin(kHashPrefix), std::end(kHashPrefix));
  serialized.append(digest.begin(), digest.end());

  if (currently_writing_) {
    pending_write_ = std::move(serialized);
    return;
  }

  DoWriteState(std::move(serialized));
}

void EnclaveManager::DoWriteState(std::string serialized) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  currently_writing_ = true;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(
          [](base::FilePath path, std::string contents) -> bool {
            std::string encrypted;
            return OSCrypt::EncryptString(contents, &encrypted) &&
                   base::ImportantFileWriter::WriteFileAtomically(path,
                                                                  contents);
          },
          file_path_, std::move(serialized)),
      base::BindOnce(&EnclaveManager::WriteStateComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EnclaveManager::WriteStateComplete(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  currently_writing_ = false;
  if (!success) {
    FIDO_LOG(ERROR) << "Failed to write enclave state";
  }

  if (pending_write_) {
    DoWriteState(std::move(*pending_write_));
    pending_write_.reset();
    return;
  }

  if (write_finished_callback_) {
    std::move(write_finished_callback_).Run();
  }
}

void EnclaveManager::ClearRegistration() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!user_) {
    return;
  }

  user_verifying_key_.reset();
  hardware_key_.reset();

  // TODO(enclave): Attempt to delete UV keys from system, since these can
  // sometimes be stored.
  user_ = nullptr;  // Prevent dangling raw_ptr error on next line.
  CHECK(local_state_->mutable_users()->erase(primary_account_info_->gaia));
  user_ = CreateStateForUser(local_state_.get(), *primary_account_info_);
  WriteState(local_state_.get());

  CancelAllActions();
  Stopped();
}

void EnclaveManager::SetSecret(int32_t key_version,
                               base::span<const uint8_t> secret) {
  secret_version_ = key_version;
  secret_ = std::vector<uint8_t>(secret.begin(), secret.end());
}
