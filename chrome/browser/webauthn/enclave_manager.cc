// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/enclave_manager.h"

#include <utility>
#include <variant>

#include "base/base64.h"
#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/functional/overloaded.h"
#include "base/memory/scoped_refptr.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/strong_alias.h"
#include "build/build_config.h"
#include "chrome/browser/webauthn/proto/enclave_local_state.pb.h"
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
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "crypto/random.h"
#include "crypto/sha2.h"
#include "crypto/unexportable_key.h"
#include "crypto/user_verifying_key.h"
#include "device/fido/enclave/constants.h"
#include "device/fido/enclave/enclave_websocket_client.h"
#include "device/fido/enclave/transact.h"
#include "device/fido/enclave/types.h"
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

#if BUILDFLAG(IS_WIN)
#include "base/strings/strcat.h"
#endif

namespace enclave = device::enclave;
using webauthn_pb::EnclaveLocalState;

// Holds the arguments to `StoreKeys` so that they can be processed when the
// state machine is ready for them.
struct EnclaveManager::StoreKeysArgs {
  std::string gaia_id;
  std::vector<std::vector<uint8_t>> keys;
  int last_key_version;
};

struct EnclaveManager::PendingActions {
  bool want_registration;
  std::unique_ptr<StoreKeysArgs> store_keys_args;
  std::string pin;
};

namespace {

// These URLs distribute the public keys for the recovery key store.
constexpr std::string_view kCertFileURL =
    "https://www.gstatic.com/cryptauthvault/v0/cert.xml";
constexpr std::string_view kSigFileURL =
    "https://www.gstatic.com/cryptauthvault/v0/cert.sig.xml";

// The maximum number of bytes that will be downloaded from the above two URLs.
constexpr size_t kMaxFetchBodyBytes = 128 * 1024;

// This URL is used for uploading to the recovery key store.
constexpr std::string_view kRecoveryKeyStoreURL =
    "https://cryptauthvault.googleapis.com/v1/vaults/";

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
                                 /*bn_ctx=*/nullptr);
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

  return std::nullopt;
}

// Build an enclave request that registers a new device and requests a new
// wrapped asymmetric key which will be used to join the security domain.
cbor::Value BuildRegistrationMessage(
    const std::string& device_id,
    crypto::UnexportableSigningKey* hardware_key) {
  cbor::Value::MapValue pub_keys;
  pub_keys.emplace(enclave::kHardwareKey,
                   hardware_key->GetSubjectPublicKeyInfo());

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
cbor::Value BuildWrappingMessage(
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

  return cbor::Value(std::move(requests));
}

// Build an enclave request to wrap the given security domain secrets.
cbor::Value BuildPINWrappingMessage(base::span<const uint8_t> hashed_pin,
                                    std::string cert_xml,
                                    std::string sig_xml) {
  cbor::Value::ArrayValue requests;
  cbor::Value::MapValue request;
  request.emplace(enclave::kRequestCommandKey,
                  enclave::kRecoveryKeyStoreWrapCommandName);
  request.emplace(enclave::kRecoveryKeyStorePinHash, std::move(hashed_pin));
  request.emplace(enclave::kRecoveryKeyStoreCertXml, ToVector(cert_xml));
  request.emplace(enclave::kRecoveryKeyStoreSigXml, ToVector(sig_xml));
  requests.emplace_back(std::move(request));

  return cbor::Value(std::move(requests));
}

// Update `user` with the wrapped secrets in `response`. The
// `new_security_domain_secrets` argument is used to determine the version
// numbers of the wrapped secrets and this value must be the same as was passed
// to `BuildWrappingMessage` to generate the enclave request.
bool StoreWrappedSecrets(EnclaveLocalState::User* user,
                         const base::flat_map<int32_t, std::vector<uint8_t>>
                             new_security_domain_secrets,
                         const cbor::Value& response) {
  const cbor::Value::ArrayValue& responses = response.GetArray();
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

std::optional<crypto::UserVerifyingKeyLabel> CreateUserVerifyingKeyLabel() {
#if BUILDFLAG(IS_WIN)
  std::vector<uint8_t> random(16);
  crypto::RandBytes(random);
  return base::StrCat({"enclave-uvkey-", base::Base64Encode(random)});
#else
  return std::nullopt;
#endif
}

std::string UserVerifyingLabelToString(crypto::UserVerifyingKeyLabel label) {
#if BUILDFLAG(IS_WIN)
  return label;
#else
  return std::string();
#endif
}

std::optional<crypto::UserVerifyingKeyLabel> UserVerifyingKeyLabelFromString(
    std::string saved_label) {
#if BUILDFLAG(IS_WIN)
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
  metadata.set_lskf_type(trusted_vault_pb::VaultMetadata::PIN);
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

}  // namespace

class EnclaveManager::StateMachine {
 public:
  explicit StateMachine(EnclaveManager* manager,
                        webauthn_pb::EnclaveLocalState_User* user,
                        std::unique_ptr<CoreAccountInfo> primary_account_info)
      : manager_(manager),
        primary_account_info_(std::move(primary_account_info)) {}

  void set_want_registration() { want_registration_ = true; }

  void set_store_keys_args(
      std::unique_ptr<EnclaveManager::StoreKeysArgs> args) {
    store_keys_args_ = std::move(args);
  }

  void set_pending_pin(std::string pending_pin) {
    pending_pin_ = std::move(pending_pin);
  }

  void Start() {
    if (state_ == State::kInit) {
      state_ = State::kNextAction;
      Process(None());
    }
  }

 private:
  // This class is a state machine that uses the following states. It moves from
  // state to state in response to `Event` values. Fields such as
  // `want_registration_` and `identity_updated_` are set in order to record
  // that the state machine needs to process those requests once the current
  // processing has completed.
  enum class State {
    kInit,
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
    kWaitingForRecoveryKeyStoreTokenForUpload,
    kWaitingForRecoveryKeyStore,
  };

  enum class FetchedFile {
    kCertFile,
    kSigFile,
  };

  struct HashedPIN {
    ~HashedPIN() { memset(hashed, 0, sizeof(hashed)); }

    int n = 0;  // The scrypt `N` parameter.
    uint8_t salt[16];
    uint8_t hashed[32];
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
                        trusted_vault::TrustedVaultRegistrationStatus>;
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
                              Response>;

  void Process(Event event) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    const State initial_state = state_;
    const std::string event_str = ToString(event);

    switch (state_) {
      case State::kInit:
        // This state should never be observed. `Start` should set the
        // state to `kNextAction` before starting the event Process for the
        // first time.
        NOTREACHED();
        break;

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

      case State::kWaitingForRecoveryKeyStoreTokenForUpload:
        DoWaitingForRecoveryKeyStoreTokenForUpload(std::move(event));
        break;

      case State::kWaitingForRecoveryKeyStore:
        DoWaitingForRecoveryKeyStore(std::move(event));
        break;
    }

    FIDO_LOG(EVENT) << ToString(initial_state) << " -" << event_str << "-> "
                    << ToString(state_);

    if (state_ == State::kStop) {
      manager_->Stopped();
      // `this` has been deleted now.
      return;
    }

    // The only internal state transition (i.e. where one state moves to another
    // without waiting for an external event) allowed is to `kNextAction`.
    if (state_ != State::kNextAction) {
      return;
    }

    const State prior_state = state_;
    DoNextAction();
    FIDO_LOG(EVENT) << ToString(prior_state) << " --> " << ToString(state_);

    if (state_ == State::kStop) {
      manager_->Stopped();
      // `this` has been deleted now.
    }
  }

  static std::string ToString(State state) {
    switch (state) {
      case State::kInit:
        return "Init";
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
      case State::kWaitingForRecoveryKeyStoreTokenForUpload:
        return "WaitingForRecoveryKeyStoreTokenForUpload";
      case State::kWaitingForRecoveryKeyStore:
        return "WaitingForRecoveryKeyStore";
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
              return std::string("JoinStatus(") +
                     TrustedVaultRegistrationStatusToString(status.value()) +
                     ")";
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
            }},
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

    if ((want_registration_ || store_keys_args_ || !pending_pin_.empty()) &&
        !user()->registered()) {
      want_registration_ = false;
      StartEnclaveRegistration();
      return;
    }

    if (user()->registered() && store_keys_args_) {
      CHECK_EQ(primary_account_info_->gaia, store_keys_args_->gaia_id);
      auto store_keys_args = std::move(store_keys_args_);
      store_keys_args_.reset();

      new_security_domain_secrets_ =
          GetNewSecretsToStore(*user(), *store_keys_args);
      if (!new_security_domain_secrets_.empty()) {
        state_ = State::kWaitingForEnclaveTokenForWrapping;
        store_keys_args_for_joining_ = std::move(store_keys_args);
        GetAccessTokenInternal(GaiaConstants::kPasskeysEnclaveOAuth2Scope);
      } else if (!user()->joined() && !user()->member_public_key().empty()) {
        store_keys_args_for_joining_ = std::move(store_keys_args);
        JoinDomain();
      }
      return;
    }

    if (user()->registered() && !pending_pin_.empty()) {
      state_ = State::kHashingPIN;
      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE, {base::TaskPriority::USER_BLOCKING, base::MayBlock()},
          base::BindOnce(
              [](std::string pin) -> std::unique_ptr<HashedPIN> {
                auto hashed = std::make_unique<HashedPIN>();
                RAND_bytes(hashed->salt, sizeof(hashed->salt));
                // This is the primary work factor in scrypt. This value matches
                // the original recommended parameters. Those are a little out
                // of data in 2024, but Android is using 4096. Since this work
                // factor falls on the server when MagicArch is used, I've stuck
                // with this norm.
                hashed->n = 16384;
                CHECK(EVP_PBE_scrypt(pin.data(), pin.size(), hashed->salt,
                                     sizeof(hashed->salt), hashed->n, 8, 1,
                                     /*max_mem=*/0, hashed->hashed,
                                     sizeof(hashed->hashed)));
                return hashed;
              },
              std::move(pending_pin_)),
          base::BindOnce(
              [](base::WeakPtr<StateMachine> machine,
                 std::unique_ptr<HashedPIN> hashed) {
                if (!machine) {
                  return;
                }
                machine->Process(PINHashed(std::move(hashed)));
              },
              weak_ptr_factory_.GetWeakPtr()));
      return;
    }

    state_ = State::kStop;
  }

  void FetchComplete(FetchedFile file, std::optional<std::string> contents) {
    Process(FileFetched(std::make_pair(file, std::move(contents))));
  }

  void StartEnclaveRegistration() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    state_ = State::kGeneratingKeys;

    user_verifying_key_provider_ = crypto::GetUserVerifyingKeyProvider();
    std::optional<crypto::UserVerifyingKeyLabel> key_label;
    // TODO(enclave): Reusing the label makes sense on Windows because it will
    // overwrite the existing key with a new one. This might be different on
    // other platforms.
    if (!user()->wrapped_uv_private_key().empty()) {
      key_label =
          UserVerifyingKeyLabelFromString(user()->wrapped_uv_private_key());
    }
    if (!key_label) {
      key_label = CreateUserVerifyingKeyLabel();
    }
    if (!user_verifying_key_provider_ || !key_label) {
      // Null `user_verifying_key_provider_` means the current
      // platform does not support user-verifying keys. nullopt for |key_label|
      // means Chrome does not support them on this OS.
      GenerateHardwareKey(nullptr);
      return;
    }
    user_verifying_key_provider_->GenerateUserVerifyingSigningKey(
        *key_label, kSigningAlgorithms,
        base::BindOnce(&StateMachine::GenerateHardwareKey,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void GenerateHardwareKey(
      std::unique_ptr<crypto::UserVerifyingSigningKey> uv_key) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(state_ == State::kGeneratingKeys);
    std::optional<std::vector<uint8_t>> existing_key_id;
    if (!user()->wrapped_hardware_private_key().empty()) {
      existing_key_id = ToVector(user()->wrapped_hardware_private_key());
    }
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
        base::BindOnce(
            [](std::optional<std::vector<uint8_t>> key_id,
               std::unique_ptr<crypto::UserVerifyingSigningKey> uv_key)
                -> Event {
              auto provider =
                  crypto::GetSoftwareUnsecureUnexportableKeyProvider();
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
    user_verifying_key_ =
        std::move(absl::get_if<KeyReady>(&event)->value().first);
    hardware_key_ = std::move(absl::get_if<KeyReady>(&event)->value().second);

    EnclaveLocalState::User* const user_state = user();
    if (user_verifying_key_) {
      const std::vector<uint8_t> uv_public_key =
          user_verifying_key_->GetPublicKey();
      const std::string uv_public_key_str = VecToString(uv_public_key);
      if (user_state->uv_public_key() != uv_public_key_str) {
        user_state->set_uv_public_key(uv_public_key_str);
        user_state->set_wrapped_uv_private_key(
            UserVerifyingLabelToString(user_verifying_key_->GetKeyLabel()));
        state_dirty = true;
      }
    }

    const std::vector<uint8_t> spki = hardware_key_->GetSubjectPublicKeyInfo();
    const std::string spki_str = VecToString(spki);
    if (user_state->hardware_public_key() != spki_str) {
      std::array<uint8_t, crypto::kSHA256Length> device_id =
          crypto::SHA256Hash(spki);
      user_state->set_hardware_public_key(spki_str);
      user_state->set_wrapped_hardware_private_key(
          VecToString(hardware_key_->GetWrappedKey()));
      user_state->set_device_id(VecToString(device_id));
      state_dirty = true;
    }

    if (state_dirty) {
      manager_->WriteState();
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
    enclave::Transact(
        manager_->network_context_, enclave::GetEnclaveIdentity(),
        std::move(token),
        BuildRegistrationMessage(user()->device_id(), hardware_key_.get()),
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
            user(), response.GetArray()[1]
                        .GetMap()
                        .find(cbor::Value(enclave::kResponseSuccessKey))
                        ->second)) {
      FIDO_LOG(ERROR) << "Wrapped member key was invalid: "
                      << cbor::DiagnosticWriter::Write(response);
      state_ = State::kNextAction;
      return;
    }

    user()->set_registered(true);
    manager_->WriteState();
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
        manager_->network_context_, enclave::GetEnclaveIdentity(),
        std::move(token), BuildWrappingMessage(new_security_domain_secrets_),
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

    if (!StoreWrappedSecrets(user(), new_security_domain_secrets, response)) {
      FIDO_LOG(ERROR) << "Failed to store wrapped secrets";
      state_ = State::kStop;
      return;
    }

    if (!user()->joined()) {
      JoinDomain();
    } else {
      manager_->WriteState();
      state_ = State::kNextAction;
    }
  }

  void JoinDomain() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    state_ = State::kJoiningDomain;
    const auto secure_box_pub_key =
        trusted_vault::SecureBoxPublicKey::CreateByImport(
            ToSpan(user()->member_public_key()));
    join_request_ = manager_->trusted_vault_conn_->RegisterAuthenticationFactor(
        *primary_account_info_, store_keys_args_for_joining_->keys,
        store_keys_args_for_joining_->last_key_version, *secure_box_pub_key,
        trusted_vault::AuthenticationFactorType::kPhysicalDevice,
        /*authentication_factor_type_hint=*/std::nullopt,
        base::BindOnce(
            [](base::WeakPtr<StateMachine> machine,
               trusted_vault::TrustedVaultRegistrationStatus status) {
              if (!machine) {
                return;
              }
              machine->Process(JoinStatus(status));
            },
            weak_ptr_factory_.GetWeakPtr()));
  }

  void DoJoiningDomain(Event event) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    join_request_.reset();
    store_keys_args_for_joining_.reset();

    CHECK(absl::holds_alternative<JoinStatus>(event));
    const trusted_vault::TrustedVaultRegistrationStatus status =
        absl::get_if<JoinStatus>(&event)->value();

    switch (status) {
      case trusted_vault::TrustedVaultRegistrationStatus::kSuccess:
      case trusted_vault::TrustedVaultRegistrationStatus::kAlreadyRegistered:
        user()->set_joined(true);
        break;
      default:
        user()->mutable_wrapped_security_domain_secrets()->clear();
        break;
    }

    manager_->WriteState();
    state_ = State::kNextAction;
  }

  void DoHashingPIN(Event event) {
    // The new PIN has been hashed. Next we fetch the public keys of the
    // recovery key store.
    CHECK(absl::holds_alternative<PINHashed>(event));
    hashed_pin_ = std::move(absl::get_if<PINHashed>(&event)->value());

    cert_xml_loader_ = FetchURL(
        manager_->url_loader_factory_.get(), kCertFileURL,
        base::BindOnce(&StateMachine::FetchComplete,
                       weak_ptr_factory_.GetWeakPtr(), FetchedFile::kCertFile));
    sig_xml_loader_ = FetchURL(
        manager_->url_loader_factory_.get(), kSigFileURL,
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

    // We have everything needed to make the enclave request to wrap the hashed
    // PIN for transmission to the recovery key store.
    state_ = State::kWrappingPIN;
    std::string token = std::move(absl::get_if<AccessToken>(&event)->value());
    enclave::Transact(
        manager_->network_context_, enclave::GetEnclaveIdentity(),
        std::move(token),
        BuildPINWrappingMessage(hashed_pin_->hashed, std::move(*cert_xml_),
                                std::move(*sig_xml_)),
        enclave::SigningCallback(),
        base::BindOnce(&StateMachine::OnEnclaveResponse,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void DoWrappingPIN(Event event) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    cbor::Value response =
        std::move(absl::get_if<EnclaveResponse>(&event)->value());
    if (!IsAllOk(response, 1)) {
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
                                            recovery_key_store_wrap_response);
    if (!vault) {
      FIDO_LOG(ERROR)
          << "Failed to translate response into an UpdateVaultProto";
      state_ = State::kStop;
      return;
    }
    vault_ = std::move(*vault);

    state_ = State::kWaitingForRecoveryKeyStoreTokenForUpload;
    GetAccessTokenInternal(GaiaConstants::kCryptAuthOAuth2Scope);
  }

  void DoWaitingForRecoveryKeyStoreTokenForUpload(Event event) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    access_token_fetcher_.reset();
    if (absl::holds_alternative<Failure>(event)) {
      FIDO_LOG(ERROR) << "Failed to get access token for cryptauth";
      state_ = State::kStop;
      return;
    }
    CHECK(absl::holds_alternative<AccessToken>(event)) << ToString(event);

    std::string token = std::move(absl::get_if<AccessToken>(&event)->value());
    auto request = std::make_unique<network::ResourceRequest>();
    GURL base_url(std::string(kRecoveryKeyStoreURL) +
                  // We're uploading a new entry, rather than refreshing an
                  // existing one, so the ID is just a placeholder:
                  "0");
    request->url = net::AppendQueryParameter(base_url, "alt", "proto");
    request->method = "PATCH";
    request->headers.SetHeader("Authorization",
                               base::StrCat({"Bearer ", token}));

    upload_loader_ = network::SimpleURLLoader::Create(std::move(request),
                                                      kTrafficAnnotation);
    upload_loader_->SetTimeoutDuration(base::Seconds(10));
    upload_loader_->SetURLLoaderFactoryOptions(
        network::mojom::kURLLoadOptionBlockAllCookies);
    std::string serialized_vault;
    CHECK(vault_->SerializeToString(&serialized_vault));
    upload_loader_->AttachStringForUpload(serialized_vault,
                                          "application/x-protobuf");

    state_ = State::kWaitingForRecoveryKeyStore;
    upload_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        manager_->url_loader_factory_.get(),
        base::BindOnce(
            [](base::WeakPtr<StateMachine> machine,
               std::optional<std::string> response) {
              if (!machine) {
                return;
              }
              if (response) {
                machine->Process(Response(std::move(*response)));
              } else {
                machine->Process(Failure());
              }
            },
            weak_ptr_factory_.GetWeakPtr()));
  }

  void DoWaitingForRecoveryKeyStore(Event event) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    access_token_fetcher_.reset();
    if (absl::holds_alternative<Failure>(event)) {
      FIDO_LOG(ERROR) << "Failed to upload to recovery key store";
      state_ = State::kNextAction;
      return;
    }
    CHECK(absl::holds_alternative<Response>(event)) << ToString(event);

    const std::string& response_str = absl::get_if<Response>(&event)->value();
    trusted_vault_pb::Vault vault;
    if (!vault.ParseFromString(response_str)) {
      FIDO_LOG(ERROR) << "Failed to parse Vault: "
                      << base::HexEncode(base::as_byte_span(response_str));
      state_ = State::kNextAction;
      return;
    }

    /// TODO(enclave): wrap the PIN hash for the enclave.
    state_ = State::kNextAction;
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

  EnclaveLocalState::User* user() const {
    return manager_->current_user_state();
  }

  const raw_ptr<EnclaveManager> manager_;
  const std::unique_ptr<CoreAccountInfo> primary_account_info_;

  State state_ = State::kInit;

  std::unique_ptr<StoreKeysArgs> store_keys_args_;
  std::string pending_pin_;
  bool want_registration_ = false;

  std::unique_ptr<StoreKeysArgs> store_keys_args_for_joining_;
  std::unique_ptr<crypto::UserVerifyingSigningKey> user_verifying_key_;
  std::unique_ptr<crypto::UserVerifyingKeyProvider>
      user_verifying_key_provider_;
  std::unique_ptr<crypto::UnexportableSigningKey> hardware_key_;
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

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<StateMachine> weak_ptr_factory_{this};
};

EnclaveManager::EnclaveManager(
    const base::FilePath& base_dir,
    signin::IdentityManager* identity_manager,
    raw_ptr<network::mojom::NetworkContext> network_context,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : file_path_(base_dir.Append(FILE_PATH_LITERAL("passkey_enclave_state"))),
      identity_manager_(identity_manager),
      network_context_(network_context),
      url_loader_factory_(url_loader_factory),
      trusted_vault_conn_(trusted_vault::NewFrontendTrustedVaultConnection(
          trusted_vault::SecurityDomainId::kPasskeys,
          identity_manager,
          url_loader_factory_)),
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

bool EnclaveManager::is_ready() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_registered() && !user_->wrapped_security_domain_secrets().empty();
}

unsigned EnclaveManager::store_keys_count() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return store_keys_count_;
}

bool EnclaveManager::is_uv_key_available() const {
  return user_ && !user_->wrapped_uv_private_key().empty();
}

void EnclaveManager::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Act();
}

void EnclaveManager::RegisterIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (user_ && user_->registered()) {
    return;
  }
  if (!pending_actions_) {
    pending_actions_ = std::make_unique<PendingActions>();
  }
  pending_actions_->want_registration = true;
  Act();
}

void EnclaveManager::SetupWithPIN(std::string pin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pending_actions_) {
    pending_actions_ = std::make_unique<PendingActions>();
  }
  pending_actions_->pin = std::move(pin);
  Act();
}

enclave::SigningCallback EnclaveManager::HardwareKeySigningCallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!user_->wrapped_hardware_private_key().empty());
  CHECK(user_->registered());

  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner =
      base::SingleThreadTaskRunner::GetCurrentDefault();

  return base::BindOnce(
      // TODO: this callback should also take a WeakPtr to the EnclaveManager so
      // that the EnclaveManager can hold a cache of loaded keys and so that
      // signing errors can be signaled up and cause the registration to be
      // erased. (TPMs sometimes lose keys in practice.)
      [](scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
         std::string wrapped_hardware_private_key, std::string device_id,
         enclave::SignedMessage message_to_be_signed,
         base::OnceCallback<void(std::optional<enclave::ClientSignature>)>
             result_callback) {
        // Post to a blocking thread for the slow operation.
        base::ThreadPool::PostTask(
            FROM_HERE, {base::MayBlock()},
            base::BindOnce(
                [](scoped_refptr<base::SingleThreadTaskRunner>
                       caller_task_runner,
                   std::string wrapped_hardware_private_key,
                   std::string device_id,
                   enclave::SignedMessage message_to_be_signed,
                   base::OnceCallback<void(
                       std::optional<enclave::ClientSignature>)>
                       result_callback) {
                  // TODO(enclave): cache the key loading. TPMs are slow.
                  auto provider =
                      crypto::GetSoftwareUnsecureUnexportableKeyProvider();
                  std::unique_ptr<crypto::UnexportableSigningKey> key =
                      provider->FromWrappedSigningKeySlowly(
                          ToVector(wrapped_hardware_private_key));
                  if (!key) {
                    caller_task_runner->PostTask(
                        FROM_HERE,
                        base::BindOnce(
                            [](base::OnceCallback<void(
                                   std::optional<enclave::ClientSignature>)>
                                   result_callback) {
                              std::move(result_callback).Run(std::nullopt);
                            },
                            std::move(result_callback)));
                    return;
                  }
                  std::optional<std::vector<uint8_t>> signature =
                      key->SignSlowly(message_to_be_signed);
                  if (!signature) {
                    caller_task_runner->PostTask(
                        FROM_HERE,
                        base::BindOnce(
                            [](base::OnceCallback<void(
                                   std::optional<enclave::ClientSignature>)>
                                   result_callback) {
                              std::move(result_callback).Run(std::nullopt);
                            },
                            std::move(result_callback)));
                    return;
                  }

                  enclave::ClientSignature client_signature;
                  client_signature.device_id = ToVector(device_id);
                  client_signature.signature = std::move(*signature);
                  client_signature.key_type = enclave::ClientKeyType::kHardware;
                  caller_task_runner->PostTask(
                      FROM_HERE,
                      base::BindOnce(
                          [](base::OnceCallback<void(
                                 std::optional<enclave::ClientSignature>)>
                                 result_callback,
                             enclave::ClientSignature client_signature) {
                            std::move(result_callback)
                                .Run(std::move(client_signature));
                          },
                          std::move(result_callback),
                          std::move(client_signature)));
                },
                caller_task_runner, std::move(wrapped_hardware_private_key),
                std::move(device_id), std::move(message_to_be_signed),
                std::move(result_callback)));
      },
      caller_task_runner, user_->wrapped_hardware_private_key(),
      user_->device_id());
}

enclave::SigningCallback EnclaveManager::UserVerifyingKeySigningCallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!user_->wrapped_uv_private_key().empty());
  CHECK(user_->registered());

  auto key_label =
      UserVerifyingKeyLabelFromString(user_->wrapped_uv_private_key());
  CHECK(key_label);

  return base::BindOnce(
      [](crypto::UserVerifyingKeyLabel uv_key_label, std::string device_id,
         enclave::SignedMessage message_to_be_signed,
         base::OnceCallback<void(std::optional<enclave::ClientSignature>)>
             result_callback) {
        auto provider = crypto::GetUserVerifyingKeyProvider();
        provider->GetUserVerifyingSigningKey(
            std::move(uv_key_label),
            base::BindOnce(
                [](std::string device_id,
                   enclave::SignedMessage message_to_be_signed,
                   base::OnceCallback<void(
                       std::optional<enclave::ClientSignature>)>
                       result_callback,
                   std::unique_ptr<crypto::UserVerifyingSigningKey>
                       uv_signing_key) {
                  if (!uv_signing_key) {
                    std::move(result_callback).Run(std::nullopt);
                    return;
                  }
                  uv_signing_key->Sign(
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
                std::move(device_id), std::move(message_to_be_signed),
                std::move(result_callback)));
      },
      std::move(*key_label), user_->device_id());
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

std::vector<std::vector<uint8_t>> EnclaveManager::GetWrappedSecrets() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(is_ready());
  std::vector<std::vector<uint8_t>> ret;
  for (const auto& it : user_->wrapped_security_domain_secrets()) {
    ret.emplace_back(ToVector(it.second));
  }
  return ret;
}

std::pair<int32_t, std::vector<uint8_t>>
EnclaveManager::GetCurrentWrappedSecret() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(is_ready());
  CHECK(!user_->wrapped_security_domain_secrets().empty());

  std::optional<int32_t> max_version;
  for (const auto& it : user_->wrapped_security_domain_secrets()) {
    if (!max_version.has_value() || *max_version < it.first) {
      max_version = it.first;
    }
  }
  const auto it = user_->wrapped_security_domain_secrets().find(*max_version);
  return std::make_pair(it->first, ToVector(it->second));
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

  auto store_keys_args = std::make_unique<StoreKeysArgs>();
  store_keys_args->gaia_id = gaia_id;
  store_keys_args->keys = std::move(keys);
  store_keys_args->last_key_version = last_key_version;
  store_keys_count_++;

  if (!pending_actions_) {
    pending_actions_ = std::make_unique<PendingActions>();
  }
  pending_actions_->store_keys_args = std::move(store_keys_args);
  Act();
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

const webauthn_pb::EnclaveLocalState& EnclaveManager::local_state_for_testing()
    const {
  return *local_state_;
}

// static
std::string_view EnclaveManager::recovery_key_store_url_for_testing() {
  return kRecoveryKeyStoreURL;
}

// static
std::string_view EnclaveManager::recovery_key_store_cert_url_for_testing() {
  return kCertFileURL;
}

// static
std::string_view EnclaveManager::recovery_key_store_sig_url_for_testing() {
  return kSigFileURL;
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

  if (!user_ || !pending_actions_) {
    Stopped();
    return;
  }

  if (!state_machine_) {
    state_machine_ = std::make_unique<StateMachine>(
        this, user_, std::make_unique<CoreAccountInfo>(*primary_account_info_));
  }

  if (pending_actions_->want_registration) {
    state_machine_->set_want_registration();
  }
  if (pending_actions_->store_keys_args) {
    state_machine_->set_store_keys_args(
        std::move(pending_actions_->store_keys_args));
  }
  if (!pending_actions_->pin.empty()) {
    state_machine_->set_pending_pin(std::move(pending_actions_->pin));
  }

  pending_actions_.reset();
  state_machine_->Start();
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
  }

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
    for (const auto& gaia_id : to_remove) {
      CHECK(local_state_->mutable_users()->erase(gaia_id));
    }
    WriteState();
  }

  if (need_to_stop && !is_post_load) {
    Stopped();
  }
}

void EnclaveManager::Stopped() {
  state_machine_.reset();
  pending_actions_.reset();

  for (Observer& observer : observer_list_) {
    observer.OnEnclaveManagerIdle();
  }
}

void EnclaveManager::WriteState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const auto& it : local_state_->users()) {
    std::optional<int> error_line = CheckInvariants(it.second);
    CHECK(!error_line.has_value())
        << "State invariant failed on line " << *error_line;
  }

  std::string serialized;
  serialized.reserve(1024);
  local_state_->AppendToString(&serialized);
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

EnclaveLocalState::User* EnclaveManager::current_user_state() const {
  DCHECK(user_);
  return user_;
}
