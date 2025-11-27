// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/enclave_manager.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/sequence_checker.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_view_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/expected.h"
#include "base/types/strong_alias.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/webauthn/proto/enclave_local_state.pb.h"
#include "chrome/browser/webauthn/unexportable_key_utils.h"
#include "chrome/browser/webauthn/webauthn_metrics_util.h"
#include "components/cbor/diagnostic_writer.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "components/device_event_log/device_event_log.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "components/signin/public/identity_manager/scope_set.h"
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
#include "content/public/browser/render_frame_host.h"
#include "crypto/aead.h"
#include "crypto/hkdf.h"
#include "crypto/random.h"
#include "crypto/sha2.h"
#include "crypto/unexportable_key.h"
#include "crypto/user_verifying_key.h"
#include "device/fido/enclave/constants.h"
#include "device/fido/enclave/enclave_authenticator.h"
#include "device/fido/enclave/transact.h"
#include "device/fido/enclave/types.h"
#include "device/fido/features.h"
#include "device/fido/network_context_factory.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/url_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/rand.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/shell.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "components/trusted_vault/icloud_recovery_key_mac.h"
#endif  // BUILDFLAG(IS_MAC)

namespace enclave = device::enclave;
using webauthn_pb::EnclaveLocalState;

// Holds the arguments to `StoreKeys` so that they can be processed when the
// state machine is ready for them.
struct EnclaveManager::StoreKeysArgs {
  GaiaId gaia_id;
  std::vector<std::vector<uint8_t>> keys;
  int last_key_version;
};

struct EnclaveManager::PendingAction {
  EnclaveManager::Callback callback;
  bool want_registration = false;
  bool renew_pin = false;
  std::unique_ptr<StoreKeysArgs> store_keys_args;
  bool setup_account = false;
  std::string pin;          // the PIN to add to set up an account with.
  std::string set_pin;      // the PIN to set on an existing account.
  std::string updated_pin;  // a new PIN, to replace the current PIN.
  std::string rapt;         // ReAuthentication Proof Token.
  bool update_wrapped_pin;  // copy `wrapped_pin` to the state.
  std::unique_ptr<EnclaveLocalState::WrappedPIN> wrapped_pin;
  std::optional<std::string> pin_public_key;  // the current PIN PK in the SDS.
#if BUILDFLAG(IS_MAC)
  std::unique_ptr<trusted_vault::ICloudRecoveryKey> icloud_recovery_key;
#endif                      // BUILDFLAG(IS_MAC)
  bool unregister = false;  // whether to unregister from the enclave.
};

EnclaveManager::StoreKeysLock::StoreKeysLock(
    base::WeakPtr<EnclaveManager> manager)
    : manager_(std::move(manager)) {}

EnclaveManager::StoreKeysLock::~StoreKeysLock() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!manager_) {
    return;
  }

  CHECK_GT(manager_->store_keys_lock_depth_, 0u);
  manager_->store_keys_lock_depth_--;
}

namespace {

// Used so the EnclaveManager can be forced into invalid states for testing.
static bool g_invariant_override_ = false;

// The maximum number of bytes that will be downloaded from the above two URLs.
constexpr size_t kMaxFetchBodyBytes = 128 * 1024;

// The number of days between GPM PIN Vault refreshes.
constexpr int kRefreshDays = 30;

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
// valid protobuf, which is handy for debugging.
static const uint8_t kHashPrefix[] = {0x82, 0x40, 32};

static const char kPinRenewalFailureHistogram[] =
    "WebAuthentication.PinRenewalFailureCause";

// The parsed response to an enclave "recovery_key_store/wrap" command.
struct EnclaveRecoveryKeyStoreWrapResponse {
  EnclaveRecoveryKeyStoreWrapResponse() = default;
  ~EnclaveRecoveryKeyStoreWrapResponse() = default;
  EnclaveRecoveryKeyStoreWrapResponse(
      const EnclaveRecoveryKeyStoreWrapResponse& other) = delete;
  EnclaveRecoveryKeyStoreWrapResponse& operator=(
      const EnclaveRecoveryKeyStoreWrapResponse& other) = delete;
  EnclaveRecoveryKeyStoreWrapResponse(
      EnclaveRecoveryKeyStoreWrapResponse&& other) = default;
  EnclaveRecoveryKeyStoreWrapResponse& operator=(
      EnclaveRecoveryKeyStoreWrapResponse&& other) = default;

  // The protobuf that can be sent to the recovery key store.
  std::unique_ptr<trusted_vault_pb::Vault> vault;

  // The chosen cohort public key.
  std::vector<uint8_t> cohort_public_key;

  // The cert.xml serial number used to select a cohort.
  int cert_xml_serial_number;
};

// Since protobuf maps `bytes` to `std::string` (rather than
// `std::vector<uint8_t>`), functions for jumping between these representations
// are needed.

template <size_t N>
base::span<const uint8_t, N> ToSizedSpan(const std::string& s) {
  CHECK_EQ(s.size(), N);
  return base::span<const uint8_t, N>(base::as_byte_span(s));
}

template <size_t N>
std::array<uint8_t, N> ToArray(base::span<const uint8_t, N> in) {
  std::array<uint8_t, N> ret;
  std::ranges::copy(in, ret.begin());
  return ret;
}

std::vector<uint8_t> ToVector(const std::string& s) {
  const auto span = base::as_byte_span(s);
  return std::vector<uint8_t>(span.begin(), span.end());
}

std::string VecToString(base::span<const uint8_t> v) {
  return std::string(base::as_string_view(v));
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
  if (g_invariant_override_) {
    return std::nullopt;
  }
  if (user.wrapped_identity_private_key().empty() !=
      user.identity_public_key().empty()) {
    return __LINE__;
  }
  if (!user.identity_public_key().empty() &&
      !IsValidSubjectPublicKeyInfo(
          base::as_byte_span(user.identity_public_key()))) {
    return __LINE__;
  }
  if (user.wrapped_identity_private_key().empty() != user.device_id().empty()) {
    return __LINE__;
  }

  if (user.wrapped_uv_private_key().empty() != user.uv_public_key().empty()) {
    return __LINE__;
  }
  if (!user.uv_public_key().empty() &&
      !IsValidSubjectPublicKeyInfo(base::as_byte_span(user.uv_public_key()))) {
    return __LINE__;
  }

  if (user.registered() && user.wrapped_identity_private_key().empty()) {
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
      !IsValidUncompressedP256X962(
          base::as_byte_span(user.member_public_key()))) {
    return __LINE__;
  }

  if (user.joined() && !user.registered()) {
    return __LINE__;
  }
  if (!user.wrapped_security_domain_secrets().empty() != user.joined()) {
    return __LINE__;
  }

  if (user.has_wrapped_pin()) {
    return CheckPINInvariants(user.wrapped_pin());
  }

  if (user.deferred_uv_key_creation() &&
      !user.wrapped_uv_private_key().empty()) {
    return __LINE__;
  }

  return std::nullopt;
}

// Parses the wrapped_pin value from an enclave CBOR response.
std::optional<std::string> ParseWrappedPinFromCbor(
    const cbor::Value& response) {
  const cbor::Value::MapValue& response_map = response.GetArray()[0].GetMap();
  const cbor::Value& ok_response =
      response_map.find(cbor::Value(enclave::kResponseSuccessKey))->second;
  if (!ok_response.is_map()) {
    FIDO_LOG(ERROR) << "PIN change response is not a map: "
                    << cbor::DiagnosticWriter::Write(response);
    return std::nullopt;
  }
  const cbor::Value::MapValue& ok_response_map = ok_response.GetMap();
  const auto wrapped_pin_value =
      ok_response_map.find(cbor::Value(enclave::kWrappedPinKey));
  if (wrapped_pin_value == ok_response_map.end() ||
      !wrapped_pin_value->second.is_bytestring()) {
    FIDO_LOG(ERROR) << "Wrapped PIN was not a bytestring";
    return std::nullopt;
  }
  return VecToString(wrapped_pin_value->second.GetBytestring());
}

// Build an enclave request that registers a new device and requests a new
// wrapped asymmetric key which will be used to join the security domain.
cbor::Value BuildRegistrationMessage(
    const std::string& device_id,
    const crypto::UnexportableSigningKey& identity_key,
    scoped_refptr<crypto::RefCountedUserVerifyingSigningKey> uv_key,
    bool defer_uv_key) {
  cbor::Value::MapValue pub_keys;

  const char* key_type = identity_key.IsHardwareBacked()
                             ? enclave::kHardwareKey
                             : enclave::kSoftwareKey;
  pub_keys.emplace(key_type, identity_key.GetSubjectPublicKeyInfo());
  if (uv_key) {
    const char* uv_key_type = uv_key->key().IsHardwareBacked()
                                  ? enclave::kUserVerificationKey
                                  : enclave::kSoftwareUserVerificationKey;
    pub_keys.emplace(uv_key_type, uv_key->key().GetPublicKey());
  }

  cbor::Value::MapValue request1;
  request1.emplace(enclave::kRequestCommandKey, enclave::kRegisterCommandName);
  request1.emplace(enclave::kRegisterDeviceIdKey,
                   std::vector<uint8_t>(device_id.begin(), device_id.end()));
  request1.emplace(enclave::kRegisterPubKeysKey, std::move(pub_keys));

  if (defer_uv_key) {
    CHECK(!uv_key);
    // The enclave ignores the value. The presence of the entry signals that the
    // UV key is pending.
    request1.emplace(enclave::kRegisterUVKeyPending, true);
  }

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

cbor::Value BuildUnregisterMessage(const std::string& device_id) {
  cbor::Value::MapValue request;
  request.emplace(enclave::kRequestCommandKey, enclave::kForgetCommandName);
  request.emplace(enclave::kRegisterDeviceIdKey,
                  std::vector<uint8_t>(device_id.begin(), device_id.end()));

  cbor::Value::ArrayValue requests;
  requests.emplace_back(std::move(request));

  return cbor::Value(std::move(requests));
}

EnclaveLocalState::User* StateForUser(EnclaveLocalState* local_state,
                                      const CoreAccountInfo& account) {
  auto it = local_state->mutable_users()->find(account.gaia.ToString());
  if (it == local_state->mutable_users()->end()) {
    return nullptr;
  }
  return &(it->second);
}

EnclaveLocalState::User* CreateStateForUser(EnclaveLocalState* local_state,
                                            const CoreAccountInfo& account) {
  auto pair = local_state->mutable_users()->insert(
      {account.gaia.ToString(), EnclaveLocalState::User()});
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

// Returns the request error, if present, for the |error_index|th response
// returned by the enclave. Returns nullopt for debug errors.
std::optional<device::enclave::RequestError> GetRequestError(
    const cbor::Value& response,
    const size_t error_index) {
  if (!response.is_array()) {
    return std::nullopt;
  }
  const cbor::Value::ArrayValue& responses = response.GetArray();
  if (responses.size() <= error_index) {
    return std::nullopt;
  }
  const cbor::Value& inner_response = responses.at(error_index);
  if (!inner_response.is_map()) {
    return std::nullopt;
  }
  const cbor::Value::MapValue& inner_response_map = inner_response.GetMap();
  const auto error_it =
      inner_response_map.find(cbor::Value(enclave::kResponseErrorKey));
  if (error_it == inner_response_map.end() || !error_it->second.is_integer()) {
    return std::nullopt;
  }
  return device::enclave::GetRequestError(error_it->second.GetInteger());
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
    base::span<const uint8_t, 32> claim_key,
    base::span<const uint8_t, enclave::kCounterIDLen> counter_id,
    base::span<const uint8_t, enclave::kVaultHandleLen - 1>
        vault_handle_without_type,
    base::span<const uint8_t> wrapped_secret) {
  cbor::Value::MapValue request;
  request.emplace(enclave::kRequestCommandKey,
                  enclave::kPasskeysWrapPinCommandName);
  request.emplace(enclave::kPinHash, hashed_pin);
  if (base::FeatureList::IsEnabled(device::kWebAuthnSendPinGeneration)) {
    request.emplace(enclave::kGeneration, 0);
  }
  request.emplace(enclave::kClaimKey, claim_key);
  request.emplace(enclave::kRequestWrappedSecretKey, wrapped_secret);
  request.emplace(enclave::kRequestCounterIDKey, counter_id);
  request.emplace(enclave::kRequestVaultHandleWithoutTypeKey,
                  vault_handle_without_type);

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
    base::span<const uint8_t, enclave::kCounterIDLen> counter_id,
    base::span<const uint8_t, enclave::kVaultHandleLen - 1>
        vault_handle_without_type,
    base::span<const uint8_t> wrapped_secret) {
  cbor::Value::MapValue request;
  request.emplace(enclave::kRequestCommandKey,
                  enclave::kRecoveryKeyStoreWrapAsMemberCommandName);
  request.emplace(enclave::kRecoveryKeyStorePinHash, hashed_pin);
  request.emplace(enclave::kRecoveryKeyStoreCertXml, ToVector(cert_xml));
  request.emplace(enclave::kRecoveryKeyStoreSigXml, ToVector(sig_xml));
  request.emplace(enclave::kRequestWrappedSecretKey, wrapped_secret);
  request.emplace(enclave::kRequestCounterIDKey, counter_id);
  request.emplace(enclave::kRequestVaultHandleWithoutTypeKey,
                  vault_handle_without_type);

  cbor::Value::ArrayValue requests;
  requests.emplace_back(std::move(request));
  return requests;
}

// Build an enclave request for recovery_key_store/wrap_pin_and_secret, which
// wraps a PIN with the security domain secret, and creates Vault parameters for
// the PIN, wrapping the security domain secret.
cbor::Value BuildPINAndSecurityDomainSecretWrappingEnclaveRequest(
    base::span<const uint8_t> hashed_pin,
    base::span<const uint8_t, 32> claim_key,
    std::string cert_xml,
    std::string sig_xml,
    base::span<const uint8_t> wrapped_secret) {
  cbor::Value::MapValue request;
  request.emplace(enclave::kRequestCommandKey,
                  enclave::kRecoveryKeyStoreWrapPinAndSecretCommandName);
  request.emplace(enclave::kRecoveryKeyStorePinHash, hashed_pin);
  request.emplace(enclave::kClaimKey, claim_key);
  request.emplace(enclave::kRecoveryKeyStoreCertXml, ToVector(cert_xml));
  request.emplace(enclave::kRecoveryKeyStoreSigXml, ToVector(sig_xml));
  request.emplace(enclave::kRequestWrappedSecretKey, wrapped_secret);
  return cbor::Value(request);
}

// Build an enclave request to renew a PIN.
cbor::Value BuildPINRenewalRequest(std::string cert_xml,
                                   std::string sig_xml,
                                   base::span<const uint8_t> wrapped_secret,
                                   base::span<const uint8_t> wrapped_pin) {
  cbor::Value::MapValue request;
  request.emplace(enclave::kRequestCommandKey,
                  enclave::kRecoveryKeyStoreRewrapCommandName);
  request.emplace(enclave::kRecoveryKeyStoreCertXml, ToVector(cert_xml));
  request.emplace(enclave::kRecoveryKeyStoreSigXml, ToVector(sig_xml));
  request.emplace(enclave::kRequestWrappedSecretKey, wrapped_secret);
  request.emplace(enclave::kRequestWrappedPINDataKey, wrapped_pin);
  if (base::FeatureList::IsEnabled(device::kWebAuthnNewRefreshFlow)) {
    request.emplace(enclave::kRecoveryKeyStoreCreateNewVault, true);
  }

  return cbor::Value(std::move(request));
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

// Parse the contents of the decrypted state file. In the event of an error, an
// empty state is returned. This causes a corrupt state file to reset the
// enclave state for the current profile. Users will have to re-register with
// the enclave.
std::unique_ptr<EnclaveLocalState> ParseStateFile(
    const std::string& contents_str) {
  auto ret = std::make_unique<EnclaveLocalState>();

  const base::span<const uint8_t> contents = base::as_byte_span(contents_str);
  if (contents.size() < crypto::kSHA256Length + sizeof(kHashPrefix)) {
    FIDO_LOG(ERROR) << "Enclave state too small to be valid";
    return ret;
  }

  const base::span<const uint8_t> digest = contents.last(crypto::kSHA256Length);
  const base::span<const uint8_t> payload = contents.first(
      contents.size() - crypto::kSHA256Length - sizeof(kHashPrefix));
  const std::array<uint8_t, crypto::kSHA256Length> calculated =
      crypto::SHA256Hash(payload);
  if (calculated != digest) {
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

base::flat_set<GaiaId> GetGaiaIDs(
    const std::vector<gaia::ListedAccount>& listed_accounts) {
  base::flat_set<GaiaId> result;
  for (const gaia::ListedAccount& listed_account : listed_accounts) {
    result.insert(listed_account.gaia_id);
  }
  return result;
}

base::flat_set<GaiaId> GetGaiaIDs(
    const google::protobuf::Map<std::string, EnclaveLocalState::User>& users) {
  base::flat_set<GaiaId> result;
  for (const auto& it : users) {
    result.insert(GaiaId(it.first));
  }
  return result;
}

std::string UserVerifyingLabelToString(crypto::UserVerifyingKeyLabel label) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  return label;
#else
  return std::string("placeholder");
#endif
}

std::optional<crypto::UserVerifyingKeyLabel> UserVerifyingKeyLabelFromString(
    std::string saved_label) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  return saved_label;
#else
  return std::nullopt;
#endif
}

// Returns a GURL from a feature param. If the feature does not result in a
// valid URL, the default is returned instead.
GURL GetUrl(const base::FeatureParam<std::string>& feature_param) {
  GURL url(feature_param.Get());
  if (url.is_valid()) {
    return url;
  }
  FIDO_LOG(ERROR) << "Finch provided " << feature_param.name
                  << " URL not valid: " << feature_param.Get();
  GURL default_url(feature_param.default_value);
  CHECK(default_url.is_valid());
  return default_url;
}

// Fetch the contents of the given URL.
std::unique_ptr<network::SimpleURLLoader> FetchURL(
    network::mojom::URLLoaderFactory* url_loader_factory,
    const GURL& url,
    base::OnceCallback<void(std::optional<std::string>)> callback) {
  auto network_request = std::make_unique<network::ResourceRequest>();
  CHECK(url.is_valid());
  network_request->url = std::move(url);

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

// Stores public metadata about a PIN. This is recorded in, for example, the
// Vault metadata so that MagicArch can show the correct UI and accept GPM PIN
// entries.
struct PinMetadata {
  static PinMetadata FromProto(const EnclaveLocalState::WrappedPIN& pin) {
    return PinMetadata{
        .n = pin.hash_difficulty(),
        .is_six_digits =
            pin.form() == EnclaveLocalState::WrappedPIN::FORM_SIX_DIGITS,
        .salt = ToArray<16>(ToSizedSpan<16>(pin.hash_salt()))};
  }

  int n = 0;  // The scrypt `N` parameter.
  bool is_six_digits = false;
  std::array<uint8_t, 16> salt;
};

// Convert the response to an enclave "recovery_key_store/wrap" command, into a
// protobuf that can be sent to the recovery key store service and extracts the
// data required to build a wrapped PIN.
std::optional<EnclaveRecoveryKeyStoreWrapResponse>
ParseRecoveryKeyStoreWrapResponse(
    const PinMetadata& pin_metadata,
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

  int cert_xml_serial_number = 0;
  if (base::FeatureList::IsEnabled(device::kWebAuthnWrapCohortData)) {
    it = response.find(cbor::Value("serial"));
    if (it == response.end() || !it->second.is_integer()) {
      return std::nullopt;
    }
    cert_xml_serial_number = it->second.GetInteger();
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
  metadata.set_lskf_type(pin_metadata.is_six_digits
                             ? trusted_vault_pb::VaultMetadata::PIN
                             : trusted_vault_pb::VaultMetadata::PASSWORD);
  metadata.set_hash_type(trusted_vault_pb::VaultMetadata::SCRYPT);
  metadata.set_hash_salt(VecToString(pin_metadata.salt));
  metadata.set_hash_difficulty(pin_metadata.n);
  metadata.set_cert_path(std::move(*cert_path));

  std::string metadata_bytes;
  if (!metadata.SerializeToString(&metadata_bytes)) {
    return std::nullopt;
  }
  vault->set_vault_metadata(std::move(metadata_bytes));

  EnclaveRecoveryKeyStoreWrapResponse result;
  result.vault = std::move(vault);
  result.cohort_public_key = std::move(cohort_public_key);
  result.cert_xml_serial_number = cert_xml_serial_number;
  return result;
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

#if BUILDFLAG(IS_CHROMEOS)
UserVerifyingKeyProviderConfigChromeos MakeUserVerifyingKeyConfig(
    EnclaveManager::UVKeyOptions options) {
  UserVerifyingKeyProviderConfigChromeos config{options.dialog_controller,
                                                /*window=*/nullptr,
                                                options.rp_id};
  if (options.render_frame_host_id) {
    auto* rfh = content::RenderFrameHost::FromID(options.render_frame_host_id);
    // This is ultimately invoked from GpmEnclaveController, which can't outlive
    // the RFH where the request originated.
    CHECK(rfh);
    config.window = rfh->GetNativeView()->GetToplevelWindow();
  }
  return config;
}
#else
crypto::UserVerifyingKeyProvider::Config MakeUserVerifyingKeyConfig(
    EnclaveManager::UVKeyOptions options) {
  crypto::UserVerifyingKeyProvider::Config config;
#if BUILDFLAG(IS_MAC)
  config.keychain_access_group =
      EnclaveManager::kEnclaveKeysKeychainAccessGroup;
  config.lacontext = std::move(options.local_auth_token);
#endif  // BUILDFLAG(IS_MAC)
  return config;
}
#endif

std::unique_ptr<crypto::UserVerifyingKeyProvider>
GetUserVerifyingKeyProviderForSigning(EnclaveManager::UVKeyOptions options) {
  return GetWebAuthnUserVerifyingKeyProvider(
      MakeUserVerifyingKeyConfig(std::move(options)));
}

std::unique_ptr<crypto::UserVerifyingKeyProvider>
GetUserVerifyingKeyProviderForCreateAndDeleteOnly() {
  // Passing an empty UVKeyOptions suffices to call
  // `GenerateUserVerifyingSigningKey()` and `DeleteUserVerifyingSigningKey()`,
  // but you must not attempt to generate a signature.
  return GetWebAuthnUserVerifyingKeyProvider(
      MakeUserVerifyingKeyConfig(EnclaveManager::UVKeyOptions{}));
}

struct HashedPIN {
  ~HashedPIN() { std::ranges::fill(hashed, 0); }

  // Copies the values of this structure into a `WrappedPIN` protobuf with a
  // random claim key. The inner `wrapped_pin` member is not set and needs to be
  // filled in by the caller once that value is available.
  std::unique_ptr<EnclaveLocalState::WrappedPIN> ToWrappedPIN() const {
    uint8_t claim_key[32];
    crypto::RandBytes(claim_key);

    auto ret = std::make_unique<EnclaveLocalState::WrappedPIN>();
    ret->set_claim_key(VecToString(claim_key));
    ret->set_form(this->metadata.is_six_digits
                      ? EnclaveLocalState::WrappedPIN::FORM_SIX_DIGITS
                      : EnclaveLocalState::WrappedPIN::FORM_ARBITRARY);
    ret->set_hash(EnclaveLocalState::WrappedPIN::HASH_SCRYPT);
    ret->set_hash_difficulty(this->metadata.n);
    ret->set_hash_salt(VecToString(this->metadata.salt));

    return ret;
  }

  PinMetadata metadata;
  uint8_t hashed[32];
};

std::unique_ptr<HashedPIN> HashPINSlowly(std::string_view pin) {
  auto hashed = std::make_unique<HashedPIN>();
  RAND_bytes(hashed->metadata.salt.data(), hashed->metadata.salt.size());
  // This is the primary work factor in scrypt. This value matches
  // the original recommended parameters. Those are a little out
  // of date in 2024, but Android is using 4096. Since this work
  // factor falls on the server when MagicArch is used, I've stuck
  // with this norm.
  hashed->metadata.n = 16384;
  hashed->metadata.is_six_digits =
      pin.size() == 6 && std::ranges::all_of(pin, [](char c) -> bool {
        return c >= '0' && c <= '9';
      });
  CHECK(EVP_PBE_scrypt(pin.data(), pin.size(), hashed->metadata.salt.data(),
                       hashed->metadata.salt.size(), hashed->metadata.n, 8, 1,
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

// Parse a Vault and security domain member keys from a CBOR map. These maps
// result from enclave operations that return a Vault for insertion into the
// security domain.
static std::optional<std::pair<EnclaveRecoveryKeyStoreWrapResponse,
                               trusted_vault::MemberKeysSource>>
ParseVaultAndMemberResponse(const int32_t key_version,
                            const PinMetadata& pin_metadata,
                            const cbor::Value::MapValue& response) {
  auto it = response.find(cbor::Value("wrapped"));
  if (it == response.end()) {
    FIDO_LOG(ERROR) << "response missing 'wrapped'";
    return std::nullopt;
  }
  std::optional<EnclaveRecoveryKeyStoreWrapResponse> wrap_response =
      ParseRecoveryKeyStoreWrapResponse(pin_metadata, it->second);
  if (!wrap_response) {
    FIDO_LOG(ERROR) << "Failed to translate response into an UpdateVaultProto";
    return std::nullopt;
  }

  it = response.find(cbor::Value("wrapped_sds"));
  if (it == response.end() || !it->second.is_bytestring()) {
    FIDO_LOG(ERROR) << "response has invalid 'wrapped_sds'";
    return std::nullopt;
  }
  const std::vector<uint8_t>& wrapped_sds = it->second.GetBytestring();

  it = response.find(cbor::Value("member_proof"));
  if (it == response.end() || !it->second.is_bytestring()) {
    FIDO_LOG(ERROR) << "response has invalid 'member_proof'";
    return std::nullopt;
  }
  const std::vector<uint8_t>& member_proof = it->second.GetBytestring();

  auto member_keys_source =
      trusted_vault::MemberKeys(key_version, wrapped_sds, member_proof);

  return std::make_pair(std::move(*wrap_response),
                        std::move(member_keys_source));
}

class UvKeyCreationLockImpl : public EnclaveManager::UvKeyCreationLock {
 public:
  explicit UvKeyCreationLockImpl(base::OnceClosure release_callback) {
    on_release_ = std::move(release_callback);
  }
  ~UvKeyCreationLockImpl() override { std::move(on_release_).Run(); }

 private:
  base::OnceClosure on_release_;
};

webauthn::metrics::WebAuthenticationGPMRecoveryEvent
ToWebAuthenticationGPMRecoveryEvent(
    EnclaveManager::OutOfContextRecoveryOutcome outcome) {
  switch (outcome) {
    case EnclaveManager::OutOfContextRecoveryOutcome::
        kStoreKeysFromOpportunisticFlowSucceeded:
      return webauthn::metrics::WebAuthenticationGPMRecoveryEvent::
          kStoreKeysFromOpportunisticFlowSucceeded;
    case EnclaveManager::OutOfContextRecoveryOutcome::
        kStoreKeysFromOpportunisticFlowIgnoredNoUV:
      return webauthn::metrics::WebAuthenticationGPMRecoveryEvent::
          kStoreKeysFromOpportunisticFlowIgnoredNoUV;
    case EnclaveManager::OutOfContextRecoveryOutcome::
        kStoreKeysFromOpportunisticFlowIgnoredRedundant:
      return webauthn::metrics::WebAuthenticationGPMRecoveryEvent::
          kStoreKeysFromOpportunisticFlowIgnoredRedundant;
    case EnclaveManager::OutOfContextRecoveryOutcome::
        kStoreKeysFromOpportunisticFlowFailed:
      return webauthn::metrics::WebAuthenticationGPMRecoveryEvent::
          kStoreKeysFromOpportunisticFlowFailed;
  }
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
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&StateMachine::Process,
                                  weak_ptr_factory_.GetWeakPtr(), None()));
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
    kWrappingPINAndSecret,
    kWaitingForRecoveryKeyStore,
    kJoiningPINToDomain,
    kJoiningUpdatedPINToDomain,
#if BUILDFLAG(IS_MAC)
    kJoiningICloudKeychainToDomain,
#endif  // BUILDFLAG(IS_MAC)
    // Setting the PIN using `recovery_key_store/wrap_as_member` and
    // `passkeys/wrap_pin`.
    kSettingPIN,
    // Setting the PIN using `recovery_key_store/wrap_pin_and_secret`.
    kSettingPINWithSingleCommand,
    kRenewingPIN,
    kWaitingForEnclaveTokenForUnregister,
    kUnregistering,
    kSyncingWithSecurityDomain,
  };

  enum class FetchedFile {
    kCertFile,
    kSigFile,
  };

  using DeferredUVKeyCreation =
      base::StrongAlias<class DeferredUVKeyCreation, std::monostate>;
  using MaybeUVKey =
      std::variant<DeferredUVKeyCreation,
                   std::unique_ptr<crypto::UserVerifyingSigningKey>>;

  using None = base::StrongAlias<class None, std::monostate>;
  using Failure = base::StrongAlias<class KeyGenerationFailure, std::monostate>;
  using FileContents = base::StrongAlias<class FileContents, std::string>;
  using KeyReady = base::StrongAlias<
      class KeyGenerated,
      std::pair<MaybeUVKey, std::unique_ptr<crypto::UnexportableSigningKey>>>;
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
  using Event = std::variant<
      None,
      Failure,
      FileContents,
      KeyReady,
      EnclaveResponse,
      AccessToken,
      JoinStatus,
      FileFetched,
      PINHashed,
      Response,
      trusted_vault::RecoveryKeyStoreStatus,
      trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult>;

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

      case State::kNextAction:
        CHECK(std::holds_alternative<None>(event)) << ToString(event);
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

      case State::kWrappingPINAndSecret:
        DoWrappingPINAndSecret(std::move(event));
        break;

      case State::kWaitingForRecoveryKeyStore:
        DoWaitingForRecoveryKeyStore(std::move(event));
        break;

      case State::kJoiningPINToDomain:
        DoJoiningPINToDomain(std::move(event));
        break;

      case State::kSettingPIN:
        DoSettingPIN(std::move(event));
        break;

      case State::kSettingPINWithSingleCommand:
        DoSettingPINWithSingleCommand(std::move(event));
        break;

      case State::kJoiningUpdatedPINToDomain:
        DoJoiningUpdatedPINToDomain(std::move(event));
        break;

      case State::kRenewingPIN:
        DoRenewingPIN(std::move(event));
        break;

#if BUILDFLAG(IS_MAC)
      case State::kJoiningICloudKeychainToDomain:
        DoJoiningICloudKeychainToDomain(std::move(event));
        break;
#endif  // BUILDFLAG(IS_MAC)

      case State::kWaitingForEnclaveTokenForUnregister:
        DoWaitingForEnclaveTokenForUnregister(std::move(event));
        break;

      case State::kUnregistering:
        DoUnregistering(std::move(event));
        break;

      case State::kSyncingWithSecurityDomain:
        DoSyncingWithSecurityDomain(std::move(event));
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
      case State::kWrappingPINAndSecret:
        return "WrappingPINAndSecret";
      case State::kWaitingForRecoveryKeyStore:
        return "WaitingForRecoveryKeyStore";
      case State::kJoiningPINToDomain:
        return "JoiningPINToDomain";
      case State::kSettingPIN:
        return "SettingPIN";
      case State::kSettingPINWithSingleCommand:
        return "SettingPINWithSingleCommand";
      case State::kJoiningUpdatedPINToDomain:
        return "JoiningUpdatedPINToDomain";
      case State::kRenewingPIN:
        return "RenewingPIN";
#if BUILDFLAG(IS_MAC)
      case State::kJoiningICloudKeychainToDomain:
        return "JoiningICloudKeychainToDomain";
#endif  // BUILDFLAG(IS_MAC)
      case State::kWaitingForEnclaveTokenForUnregister:
        return "WaitingForEnclaveTokenForUnregister";
      case State::kUnregistering:
        return "Unregistering";
      case State::kSyncingWithSecurityDomain:
        return "kSyncingWithSecurityDomain";
    }
  }

  static const char* ToString(trusted_vault::RecoveryKeyStoreStatus status) {
    switch (status) {
      case trusted_vault::RecoveryKeyStoreStatus::kSuccess:
        return "Success";
      case trusted_vault::RecoveryKeyStoreStatus::
          kTransientAccessTokenFetchError:
        return "TransientError";
      case trusted_vault::RecoveryKeyStoreStatus::
          kPersistentAccessTokenFetchError:
        return "AccessTokenError";
      case trusted_vault::RecoveryKeyStoreStatus::
          kPrimaryAccountChangeAccessTokenFetchError:
        return "AccountChangedError";
      case trusted_vault::RecoveryKeyStoreStatus::kNetworkError:
        return "NetworkError";
      case trusted_vault::RecoveryKeyStoreStatus::kOtherError:
        return "OtherError";
    }
  }

  static const char* ToString(
      trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult::State
          state) {
    switch (state) {
      case trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult::
          State::kError:
        return "Error";
      case trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult::
          State::kEmpty:
        return "kEmpty";
      case trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult::
          State::kRecoverable:
        return "kRecoverable";
      case trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult::
          State::kIrrecoverable:
        return "Irrecoverable";
    }
  }

  static std::string ToString(const Event& event) {
    return std::visit(
        absl::Overload{
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
            [](const trusted_vault::RecoveryKeyStoreStatus& status) {
              return base::StrCat(
                  {"UpdateRecoveryKeyStoreStatus(", ToString(status), ")"});
            },
            [](const trusted_vault::
                   DownloadAuthenticationFactorsRegistrationStateResult&
                       result) {
              return base::StrCat(
                  {"DownloadAuthenticationFactorsRegistrationStateResult(",
                   ToString(result.state), " ", "has_gpm_pin: ",
                   result.gpm_pin_metadata.has_value() ? "yes" : "no", ")"});
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
        GetAccessTokenInternal();
      } else if (!user_->joined() && !user_->member_public_key().empty()) {
        JoinSecurityDomain();
      }
      return;
    }

#if BUILDFLAG(IS_MAC)
    if (action_->icloud_recovery_key) {
      state_ = State::kJoiningICloudKeychainToDomain;
      JoinICloudKeychainToDomain(std::move(action_->icloud_recovery_key));
      return;
    }
#endif  // BUILDFLAG(IS_MAC)

    if (!action_->set_pin.empty() || !action_->updated_pin.empty()) {
      if (!user_->registered()) {
        state_ = State::kStop;
        return;
      }

      is_set_pin_ = !action_->set_pin.empty();
      is_pin_update_ = !action_->updated_pin.empty();
      CHECK(is_set_pin_ ^ is_pin_update_);
      rapt_ = std::move(action_->rapt);
      SyncWithSecurityDomain();
      return;
    }

    if (action_->renew_pin) {
      if (!user_->registered()) {
        state_ = State::kStop;
        return;
      }

      is_pin_renewal_ = true;
      SyncWithSecurityDomain();
      return;
    }

    if (action_->unregister) {
      if (!user_->registered()) {
        success_ = true;
        state_ = State::kStop;
        return;
      }

      state_ = State::kWaitingForEnclaveTokenForUnregister;
      GetAccessTokenInternal();
      return;
    }

    if (action_->update_wrapped_pin) {
      *user_->mutable_wrapped_pin() = std::move(*action_->wrapped_pin);
      manager_->WriteState(&local_state_);
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

    AreUserVerifyingKeysSupported(base::BindOnce(
        [](base::WeakPtr<StateMachine> state_machine,
           bool is_uv_key_supported) {
          if (!state_machine) {
            return;
          }
          // The key provider is only used to create a new key, but not sign
          // with it, so passing empty options here is ok.
          auto key_provider =
              GetUserVerifyingKeyProviderForCreateAndDeleteOnly();
          if (!is_uv_key_supported || !key_provider) {
            // UV keys are not available, so skip to generating an identity
            // key.
            state_machine->GenerateIdentityKey(nullptr);
            return;
          }
          if (state_machine->user_->wrapped_uv_private_key().empty()) {
#if BUILDFLAG(IS_WIN)
            // On Windows we don't want to create a UV key at registration
            // time. Instead we defer creation until one is going to be
            // used in a UV request.
            state_machine->GenerateIdentityKey(DeferredUVKeyCreation());
#else
            // Create a new UV key.
            key_provider->GenerateUserVerifyingSigningKey(
                device::enclave::kSigningAlgorithms,
                base::BindOnce(
                    [](base::WeakPtr<StateMachine> state_machine,
                       base::expected<
                           std::unique_ptr<crypto::UserVerifyingSigningKey>,
                           crypto::UserVerifyingKeyCreationError>
                           maybe_uv_key) {
                      if (!state_machine) {
                        return;
                      }
                      std::unique_ptr<crypto::UserVerifyingSigningKey> uv_key;
                      if (maybe_uv_key.has_value()) {
                        uv_key = std::move(maybe_uv_key.value());
                      } else {
                        FIDO_LOG(ERROR)
                            << "UV key creation failed with error "
                            << static_cast<int>(maybe_uv_key.error());
                      }
                      state_machine->GenerateIdentityKey(std::move(uv_key));
                    },
                    state_machine));
#endif
            return;
          }
          // Use the existing UV key.
          key_provider->GetUserVerifyingSigningKey(
              state_machine->user_->wrapped_uv_private_key(),
              base::BindOnce(
                  [](base::WeakPtr<StateMachine> state_machine,
                     base::expected<
                         std::unique_ptr<crypto::UserVerifyingSigningKey>,
                         crypto::UserVerifyingKeyCreationError> maybe_uv_key) {
                    if (!state_machine) {
                      return;
                    }
                    std::unique_ptr<crypto::UserVerifyingSigningKey> uv_key;
                    if (maybe_uv_key.has_value()) {
                      uv_key = std::move(maybe_uv_key.value());
                    } else {
                      FIDO_LOG(ERROR) << "UV key retrieval failed with error "
                                      << static_cast<int>(maybe_uv_key.error());
                    }
                    state_machine->GenerateIdentityKey(std::move(uv_key));
                  },
                  state_machine));
        },
        weak_ptr_factory_.GetWeakPtr()));
  }

  void GenerateIdentityKey(MaybeUVKey uv_key) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(state_ == State::kGeneratingKeys);
    std::optional<std::vector<uint8_t>> existing_key_id;
    if (!user_->wrapped_identity_private_key().empty()) {
      existing_key_id = ToVector(user_->wrapped_identity_private_key());
    }
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
        base::BindOnce(
            [](std::optional<std::vector<uint8_t>> key_id,
               MaybeUVKey uv_key) -> Event {
              std::unique_ptr<crypto::UnexportableKeyProvider> provider =
                  GetWebAuthnUnexportableKeyProvider();
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
                  provider->GenerateSigningKeySlowly(
                      device::enclave::kSigningAlgorithms);
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

    if (std::holds_alternative<Failure>(event)) {
      state_ = State::kStop;
      return;
    }
    CHECK(std::holds_alternative<KeyReady>(event)) << ToString(event);

    bool state_dirty = false;

    MaybeUVKey maybe_uv_key =
        std::move(std::get_if<KeyReady>(&event)->value().first);
    // TODO(crbug.com/40253837): There is a presubmit bug that makes the script
    // complain about the unique_ptr within the holds_alternative if they are
    // on different lines. The type alias is just to work around that.
    using UVSigningKey = std::unique_ptr<crypto::UserVerifyingSigningKey>;
    if (std::holds_alternative<UVSigningKey>(maybe_uv_key)) {
      auto uv_key = std::move(std::get<UVSigningKey>(maybe_uv_key));
      if (uv_key) {
        manager_->user_verifying_key_ =
            base::MakeRefCounted<crypto::RefCountedUserVerifyingSigningKey>(
                std::move(uv_key));
        user_->set_deferred_uv_key_creation(false);
      }
    } else {
      CHECK(std::holds_alternative<DeferredUVKeyCreation>(maybe_uv_key));
      user_->set_deferred_uv_key_creation(true);
    }

    manager_->identity_key_ = base::MakeRefCounted<
        unexportable_keys::RefCountedUnexportableSigningKey>(
        std::move(std::get_if<KeyReady>(&event)->value().second),
        unexportable_keys::UnexportableKeyId());

    if (manager_->user_verifying_key_) {
      const std::vector<uint8_t> uv_public_key =
          manager_->user_verifying_key_->key().GetPublicKey();
      const std::string uv_public_key_str = VecToString(uv_public_key);
      if (user_->uv_public_key() != uv_public_key_str) {
        user_->set_uv_public_key(uv_public_key_str);
        user_->set_wrapped_uv_private_key(UserVerifyingLabelToString(
            manager_->user_verifying_key_->key().GetKeyLabel()));
        state_dirty = true;
      }
    }

    const std::vector<uint8_t> spki =
        manager_->identity_key_->key().GetSubjectPublicKeyInfo();
    const std::string spki_str = VecToString(spki);
    if (user_->identity_public_key() != spki_str) {
      std::array<uint8_t, crypto::kSHA256Length> device_id =
          crypto::SHA256Hash(spki);
      user_->set_identity_public_key(spki_str);
      user_->set_wrapped_identity_private_key(
          VecToString(manager_->identity_key_->key().GetWrappedKey()));
      user_->set_identity_key_is_software_backed(
          !manager_->identity_key_->key().IsHardwareBacked());
      user_->set_device_id(VecToString(device_id));
      state_dirty = true;
    }

    if (state_dirty) {
      manager_->WriteState(&local_state_);
    }

    state_ = State::kWaitingForEnclaveTokenForRegistration;
    GetAccessTokenInternal();
  }

  void DoWaitingForEnclaveTokenForRegistration(Event event) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    access_token_fetcher_.reset();
    if (std::holds_alternative<Failure>(event)) {
      FIDO_LOG(ERROR) << "Failed to get access token for enclave";
      state_ = State::kStop;
      return;
    }
    CHECK(std::holds_alternative<AccessToken>(event)) << ToString(event);

    state_ = State::kRegisteringWithEnclave;
    std::string token = std::move(std::get_if<AccessToken>(&event)->value());
    enclave::Transact(
        manager_->network_context_factory_, enclave::GetEnclaveIdentity(),
        std::move(token),
        /*reauthentication_token=*/std::nullopt,
        BuildRegistrationMessage(
            user_->device_id(), manager_->identity_key_->key(),
            manager_->user_verifying_key_, user_->deferred_uv_key_creation()),
        enclave::SigningCallback(),
        base::BindOnce(&StateMachine::OnEnclaveResponse,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void DoRegisteringWithEnclave(Event event) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (std::holds_alternative<Failure>(event)) {
      state_ = State::kStop;
      return;
    }

    cbor::Value response =
        std::move(std::get_if<EnclaveResponse>(&event)->value());
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
    if (std::holds_alternative<Failure>(event)) {
      FIDO_LOG(ERROR) << "Failed to get access token for enclave";
      state_ = State::kStop;
      return;
    }

    state_ = State::kWrappingSecrets;
    std::string token = std::move(std::get_if<AccessToken>(&event)->value());
    enclave::Transact(
        manager_->network_context_factory_, enclave::GetEnclaveIdentity(),
        std::move(token),
        /*reauthentication_token=*/std::nullopt,
        cbor::Value(
            BuildSecretWrappingEnclaveRequest(new_security_domain_secrets_)),
        manager_->IdentityKeySigningCallback(),
        base::BindOnce(
            [](base::WeakPtr<StateMachine> machine,
               base::expected<cbor::Value, enclave::TransactError> response) {
              if (!machine) {
                return;
              }
              if (!response.has_value()) {
                machine->Process(Failure());
              } else {
                machine->Process(EnclaveResponse(std::move(response.value())));
              }
            },
            weak_ptr_factory_.GetWeakPtr()));
  }

  void DoWrappingSecrets(Event event) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    const auto new_security_domain_secrets =
        std::move(new_security_domain_secrets_);
    new_security_domain_secrets_.clear();

    if (std::holds_alternative<Failure>(event)) {
      FIDO_LOG(ERROR) << "Failed to wrap security domain secrets";
      state_ = State::kStop;
      return;
    }

    cbor::Value response =
        std::move(std::get_if<EnclaveResponse>(&event)->value());
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

    CHECK(std::holds_alternative<JoinStatus>(event));
    const trusted_vault::TrustedVaultRegistrationStatus status =
        std::get_if<JoinStatus>(&event)->value().first;

    switch (status) {
      case trusted_vault::TrustedVaultRegistrationStatus::kSuccess:
      case trusted_vault::TrustedVaultRegistrationStatus::kAlreadyRegistered:
        user_->set_joined(true);
        manager_->WriteState(&local_state_);
        state_ = State::kNextAction;
        break;
      default:
        manager_->ClearRegistration();
        state_ = State::kStop;
        break;
    }
  }

  void SyncWithSecurityDomain() {
    state_ = State::kSyncingWithSecurityDomain;
    download_account_state_request_ =
        manager_->trusted_vault_conn_
            ->DownloadAuthenticationFactorsRegistrationState(
                *primary_account_info_,
                base::BindOnce(
                    [](base::WeakPtr<EnclaveManager::StateMachine> machine,
                       trusted_vault::
                           DownloadAuthenticationFactorsRegistrationStateResult
                               result) {
                      if (!machine) {
                        return;
                      }
                      machine->Process(std::move(result));
                    },
                    weak_ptr_factory_.GetWeakPtr()),
                /*keep_alive_callback=*/base::DoNothing());
  }

  void DoSyncingWithSecurityDomain(Event event) {
    CHECK(std::holds_alternative<
          trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult>(
        event));

    const auto& result = std::get<
        trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult>(
        event);
    if (result.state ==
        trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult::
            State::kError) {
      state_ = State::kStop;
      return;
    }

    if (manager_->IsSecurityDomainReset(result)) {
      // The security domain has been reset. Clear the registration and bail
      // out.
      manager_->ClearRegistration();
      FIDO_LOG(ERROR) << "The security domain has been reset.";
      state_ = State::kStop;
      if (is_pin_renewal_) {
        base::UmaHistogramEnumeration(
            kPinRenewalFailureHistogram,
            PinRenewalFailureCause::kSecurityDomainReset);
      }
      return;
    }

    if (!result.gpm_pin_metadata && (is_pin_renewal_ || is_pin_update_)) {
      // Chrome is trying to renew or update a PIN but the security domain
      // reports there is no PIN. Don't delete the local PIN state in case
      // there's a bug in the server, but also don't try renewing or updating it
      // as this risks joining to an out of date PIN.
      FIDO_LOG(ERROR) << "Tried to change the PIN, but SDS repots no PIN";
      if (is_pin_renewal_) {
        base::UmaHistogramEnumeration(
            kPinRenewalFailureHistogram,
            PinRenewalFailureCause::kSecurityDomainReportsNoPin);
      }
      state_ = State::kStop;
      return;
    }
    if (result.gpm_pin_metadata) {
      // This code saves the PIN public key even if the security domain reports
      // it is not usable or if it is invalid. This is necessary because the
      // security domain requires the current PIN public key to be set when
      // joining a PIN, which Chrome will do later during processing.
      if (result.gpm_pin_metadata->public_key) {
        FIDO_LOG(EVENT) << "GPM PIN public key updated";
        action_->pin_public_key =
            std::move(*result.gpm_pin_metadata->public_key);
      }
      if (result.gpm_pin_metadata->usable_pin_metadata) {
        const auto& metadata = *result.gpm_pin_metadata->usable_pin_metadata;
        auto wrapped_pin = std::make_unique<EnclaveLocalState::WrappedPIN>();
        if (wrapped_pin->ParseFromString(metadata.wrapped_pin) &&
            !CheckPINInvariants(*wrapped_pin).has_value()) {
          FIDO_LOG(EVENT) << "Updating wrapped GPM PIN";
          *user_->mutable_wrapped_pin() = std::move(*wrapped_pin);
        } else {
          FIDO_LOG(ERROR)
              << "Wrapped PIN from security domain update is invalid: "
              << base::HexEncode(base::as_byte_span(metadata.wrapped_pin));
        }
      }
    }

    if (is_set_pin_ && result.gpm_pin_metadata) {
      // There is already a PIN.
      state_ = State::kStop;
      return;
    }

    if (is_pin_renewal_) {
      // The PIN isn't being changed, so no need to hash.
      DownloadRecoveryKeyStoreKeys();
      return;
    }

    state_ = State::kHashingPIN;
    HashPIN(action_->set_pin.empty() ? std::move(action_->updated_pin)
                                     : std::move(action_->set_pin));
  }

  void DoHashingPIN(Event event) {
    // The new PIN has been hashed. Next we fetch the public keys of the
    // recovery key store.
    CHECK(std::holds_alternative<PINHashed>(event));
    hashed_pin_ = std::move(std::get_if<PINHashed>(&event)->value());
    wrapped_pin_proto_ = hashed_pin_->ToWrappedPIN();
    DownloadRecoveryKeyStoreKeys();
  }

  void DoDownloadingRecoveryKeyStoreKeys(Event event) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    CHECK(std::holds_alternative<FileFetched>(event)) << ToString(event);
    auto& file_fetched = std::get_if<FileFetched>(&event)->value();
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
      if (is_pin_renewal_) {
        base::UmaHistogramEnumeration(kPinRenewalFailureHistogram,
                                      PinRenewalFailureCause::kDuringDownload);
      }
      return;
    }

    state_ = State::kWaitingForEnclaveTokenForPINWrapping;
    GetAccessTokenInternal();
  }

  void DoWaitingForEnclaveTokenForPINWrapping(Event event) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    access_token_fetcher_.reset();
    if (std::holds_alternative<Failure>(event)) {
      FIDO_LOG(ERROR) << "Failed to get access token for enclave";
      if (is_pin_renewal_) {
        base::UmaHistogramEnumeration(
            kPinRenewalFailureHistogram,
            PinRenewalFailureCause::kGettingAccessToken);
      }
      state_ = State::kStop;
      return;
    }
    CHECK(std::holds_alternative<AccessToken>(event)) << ToString(event);
    std::string token = std::move(std::get_if<AccessToken>(&event)->value());

    if (is_set_pin_ || is_pin_update_) {
      SendPINSetRequest(std::move(token));
    } else if (is_pin_renewal_) {
      SendPINRenewalRequest(std::move(token));
    } else {
      SendPINAndSecretWrappingRequest(std::move(token));
    }
  }

  void SendPINAndSecretWrappingRequest(std::string token) {
    state_ = State::kWrappingPINAndSecret;
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
        manager_->IdentityKeySigningCallback(),
        base::BindOnce(&StateMachine::OnEnclaveResponse,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void SendPINSetRequest(std::string token) {
    if (base::FeatureList::IsEnabled(device::kWebAuthnWrapCohortData)) {
      SendPINAndSecurityDomainSecretWrappingRequest(std::move(token));
      return;
    }
    uint8_t counter_id[enclave::kCounterIDLen];
    crypto::RandBytes(counter_id);
    uint8_t vault_handle_without_type[enclave::kVaultHandleLen - 1];
    crypto::RandBytes(vault_handle_without_type);

    state_ = State::kSettingPIN;
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
                hashed_pin_->hashed,
                ToSizedSpan<32>(wrapped_pin_proto_->claim_key()), counter_id,
                vault_handle_without_type, wrapped_secret),
            BuildRecoveryKeyStorePINChangeEnclaveRequest(
                hashed_pin_->hashed, std::move(*cert_xml_),
                std::move(*sig_xml_), counter_id, vault_handle_without_type,
                wrapped_secret)),
        manager_->IdentityKeySigningCallback(),
        base::BindOnce(&StateMachine::OnEnclaveResponse,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void SendPINAndSecurityDomainSecretWrappingRequest(std::string token) {
    state_ = State::kSettingPINWithSingleCommand;
    std::vector<uint8_t> wrapped_secret =
        GetCurrentWrappedSecretForUser(user_).second;
    enclave::Transact(
        manager_->network_context_factory_, enclave::GetEnclaveIdentity(),
        std::move(token), std::move(rapt_),
        BuildPINAndSecurityDomainSecretWrappingEnclaveRequest(
            hashed_pin_->hashed,
            ToSizedSpan<32>(wrapped_pin_proto_->claim_key()),
            std::move(*cert_xml_), std::move(*sig_xml_), wrapped_secret),
        manager_->IdentityKeySigningCallback(),
        base::BindOnce(&StateMachine::OnEnclaveResponse,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void SendPINRenewalRequest(std::string token) {
    state_ = State::kRenewingPIN;
    enclave::Transact(
        manager_->network_context_factory_, enclave::GetEnclaveIdentity(),
        std::move(token), std::nullopt,
        BuildPINRenewalRequest(
            std::move(*cert_xml_), std::move(*sig_xml_),
            GetCurrentWrappedSecretForUser(user_).second,
            base::as_byte_span(user_->wrapped_pin().wrapped_pin())),
        manager_->IdentityKeySigningCallback(),
        base::BindOnce(&StateMachine::OnEnclaveResponse,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void DoWrappingPINAndSecret(Event event) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (std::holds_alternative<Failure>(event)) {
      state_ = State::kStop;
      return;
    }

    cbor::Value response =
        std::move(std::get_if<EnclaveResponse>(&event)->value());
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

    recovery_key_store_wrap_response_ = ParseRecoveryKeyStoreWrapResponse(
        hashed_pin_->metadata, recovery_key_store_wrap_response);
    if (!recovery_key_store_wrap_response_) {
      FIDO_LOG(ERROR)
          << "Failed to translate response into an UpdateVaultProto";
      state_ = State::kStop;
      return;
    }

    wrapping_response_ = std::move(response);

    state_ = State::kWaitingForRecoveryKeyStore;
    recovery_key_store_request_ =
        manager_->recovery_key_store_conn_->UpdateRecoveryKeyStore(
            *primary_account_info_, *recovery_key_store_wrap_response_->vault,
            base::BindOnce(
                [](base::WeakPtr<StateMachine> machine,
                   trusted_vault::RecoveryKeyStoreStatus status) {
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
    CHECK(std::holds_alternative<trusted_vault::RecoveryKeyStoreStatus>(event))
        << ToString(event);

    const auto* status =
        std::get_if<trusted_vault::RecoveryKeyStoreStatus>(&event);
    if (*status != trusted_vault::RecoveryKeyStoreStatus::kSuccess) {
      if (is_pin_renewal_) {
        base::UmaHistogramEnumeration(kPinRenewalFailureHistogram,
                                      PinRenewalFailureCause::kRKSUpload);
      }
      FIDO_LOG(ERROR) << "Failed to upload to recovery key store";
      state_ = State::kStop;
      return;
    }

    const bool updating_pin_member = is_pin_update_ || is_pin_renewal_;
    if (!updating_pin_member && !is_set_pin_) {
      CHECK(wrapped_pin_proto_->wrapped_pin().empty());
      wrapped_pin_proto_->set_wrapped_pin(BuildWrappedPIN(
          *hashed_pin_, ToSizedSpan<32>(wrapped_pin_proto_->claim_key()),
          *recovery_key_store_wrap_response_,
          store_keys_args_for_joining_->keys.back()));
    }
    const std::string& vault_public_key =
        recovery_key_store_wrap_response_->vault->application_keys()[0]
            .asymmetric_key_pair()
            .public_key();
    const auto secure_box_pub_key =
        trusted_vault::SecureBoxPublicKey::CreateByImport(
            base::as_byte_span(vault_public_key));

    std::string wrapped_pin_proto_serialized =
        wrapped_pin_proto_->SerializeAsString();
    *user_->mutable_wrapped_pin() = std::move(*wrapped_pin_proto_);
    // If changing the PIN, there must be a previous PIN member public key.
    // If enrolling with a PIN, it's possible Chrome is replacing an existing
    // PIN that cannot be used, in which case we also need to set the previous
    // PIN member public key.
    CHECK(!updating_pin_member || action_->pin_public_key);

    state_ = (updating_pin_member || is_set_pin_)
                 ? State::kJoiningUpdatedPINToDomain
                 : State::kJoiningPINToDomain;
    std::optional<trusted_vault::MemberKeysSource> member_keys_source =
        std::move(member_keys_source_);
    // If changing, renewing, or setting a PIN then `member_keys_source` will
    // have been populated by the enclave. Otherwise a new PIN is being set and
    // `store_keys_args_for_joining_` will contain the security domain secret,
    // which is sufficient for calculating the member keys.
    CHECK_EQ(member_keys_source.has_value(),
             updating_pin_member || is_set_pin_);
    if (!member_keys_source) {
      member_keys_source = trusted_vault::GetTrustedVaultKeysWithVersions(
          store_keys_args_for_joining_->keys,
          store_keys_args_for_joining_->last_key_version);
    }
    join_request_ = manager_->trusted_vault_conn_->RegisterAuthenticationFactor(
        *primary_account_info_, std::move(*member_keys_source),
        *secure_box_pub_key,
        trusted_vault::GpmPinMetadata(
            action_->pin_public_key,
            trusted_vault::UsableRecoveryPinMetadata(
                std::move(wrapped_pin_proto_serialized),
                /*expiry=*/base::Time())),
        base::BindOnce(&StateMachine::OnJoinedSecurityDomain,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void DoJoiningPINToDomain(Event event) {
    CHECK(std::holds_alternative<JoinStatus>(event)) << ToString(event);

    wrapped_pin_proto_.reset();

    const auto& join_status = std::get_if<JoinStatus>(&event)->value();
    const trusted_vault::TrustedVaultRegistrationStatus status =
        join_status.first;
    const int key_version = join_status.second;

    if (status != trusted_vault::TrustedVaultRegistrationStatus::kSuccess) {
      state_ = State::kStop;
      return;
    }

    if (is_set_pin_) {
      // If adding a PIN to an existing account, then we're done.
      success_ = true;
      state_ = State::kStop;
      return;
    }

    store_keys_args_for_joining_->last_key_version = key_version;

    if (!StoreWrappedSecrets(
            user_, GetNewSecretsToStore(*user_, *store_keys_args_for_joining_),
            base::span_from_ref(wrapping_response_->GetArray()[1]))) {
      FIDO_LOG(ERROR) << "Secret wrapping resulted in malformed response: "
                      << cbor::DiagnosticWriter::Write(*wrapping_response_);
      state_ = State::kStop;
      return;
    }

    user_->set_last_refreshed_pin_epoch_secs(
        base::Time::Now().InSecondsFSinceUnixEpoch());

    JoinSecurityDomain();
  }

  void DoJoiningUpdatedPINToDomain(Event event) {
    CHECK(std::holds_alternative<JoinStatus>(event)) << ToString(event);

    wrapped_pin_proto_.reset();

    const auto& join_status = std::get_if<JoinStatus>(&event)->value();
    const trusted_vault::TrustedVaultRegistrationStatus status =
        join_status.first;

    state_ = State::kStop;
    success_ =
        status == trusted_vault::TrustedVaultRegistrationStatus::kSuccess;
    if (!success_) {
      if (is_pin_renewal_) {
        base::UmaHistogramEnumeration(kPinRenewalFailureHistogram,
                                      PinRenewalFailureCause::kJoiningToDomain);
      }
      return;
    }

    user_->set_last_refreshed_pin_epoch_secs(
        base::Time::Now().InSecondsFSinceUnixEpoch());
    manager_->WriteState(&local_state_);
  }

#if BUILDFLAG(IS_MAC)
  void DoJoiningICloudKeychainToDomain(Event event) {
    CHECK(std::holds_alternative<JoinStatus>(event)) << ToString(event);
    const auto& join_status = std::get_if<JoinStatus>(&event)->value();
    const trusted_vault::TrustedVaultRegistrationStatus status =
        join_status.first;
    FIDO_LOG(EVENT) << "iCloud recovery key registration status: "
                    << TrustedVaultRegistrationStatusToString(status);
    state_ = State::kNextAction;
  }
#endif  // BUILDFLAG(IS_MAC)

  void DoSettingPIN(Event event) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    state_ = State::kStop;
    if (std::holds_alternative<Failure>(event)) {
      return;
    }

    cbor::Value response =
        std::move(std::get_if<EnclaveResponse>(&event)->value());
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

    UploadVaultAndMemberFromResponse(hashed_pin_->metadata,
                                     response.GetArray()[1]);
  }

  void DoSettingPINWithSingleCommand(Event event) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    state_ = State::kStop;
    if (std::holds_alternative<Failure>(event)) {
      return;
    }

    cbor::Value response =
        std::move(std::get_if<EnclaveResponse>(&event)->value());
    if (!IsAllOk(response, 1)) {
      FIDO_LOG(ERROR) << "PIN change resulted in error response: "
                      << cbor::DiagnosticWriter::Write(response);
      return;
    }

    std::optional<std::string> wrapped_pin = ParseWrappedPinFromCbor(response);
    if (!wrapped_pin) {
      return;
    }
    wrapped_pin_proto_->set_wrapped_pin(std::move(*wrapped_pin));

    UploadVaultAndMemberFromResponse(hashed_pin_->metadata,
                                     response.GetArray()[0]);
  }

  void DoRenewingPIN(Event event) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    state_ = State::kStop;
    if (std::holds_alternative<Failure>(event)) {
      base::UmaHistogramEnumeration(kPinRenewalFailureHistogram,
                                    PinRenewalFailureCause::kEnclaveRequest1);
      return;
    }

    cbor::Value response =
        std::move(std::get_if<EnclaveResponse>(&event)->value());
    std::optional<device::enclave::RequestError> error =
        GetRequestError(response, 0u);
    if (error) {
      switch (*error) {
        case device::enclave::RequestError::kCohortNotYetDeprecated:
          base::UmaHistogramEnumeration(
              kPinRenewalFailureHistogram,
              PinRenewalFailureCause::kCohortNotYetDeprecated);
          // This is the usual expected result of a PIN renewal.
          FIDO_LOG(EVENT) << "Not renewing PIN because the enclave reports the "
                             "cohort is not yet deprecated";
          user_->set_last_refreshed_pin_epoch_secs(
              base::Time::Now().InSecondsFSinceUnixEpoch());
          manager_->WriteState(&local_state_);
          return;
        case device::enclave::RequestError::kRecoveryKeyStoreDowngrade:
          base::UmaHistogramEnumeration(
              kPinRenewalFailureHistogram,
              PinRenewalFailureCause::kRecoveryKeyStoreDowngrade);
          FIDO_LOG(ERROR) << "Not renewing PIN because it would result in "
                             "downgrading the recovery store";
          return;
        default:
          // `IsAllOk` below catches other errors the enclave may return that
          // the client does not know about.
          break;
      }
    }
    if (!IsAllOk(response, 1)) {
      base::UmaHistogramEnumeration(kPinRenewalFailureHistogram,
                                    PinRenewalFailureCause::kEnclaveRequest2);
      FIDO_LOG(ERROR) << "PIN renewal resulted in error response: "
                      << cbor::DiagnosticWriter::Write(response);
      return;
    }

    // The PIN hash and claim keys haven't changed...
    wrapped_pin_proto_ =
        std::make_unique<EnclaveLocalState::WrappedPIN>(user_->wrapped_pin());
    if (base::FeatureList::IsEnabled(device::kWebAuthnWrapCohortData)) {
      // ...but the wrapped PIN may contain new Vault cohort details so we need
      // to update that.
      std::optional<std::string> wrapped_pin =
          ParseWrappedPinFromCbor(response);
      if (!wrapped_pin) {
        return;
      }
      wrapped_pin_proto_->set_wrapped_pin(std::move(*wrapped_pin));
    }

    UploadVaultAndMemberFromResponse(
        PinMetadata::FromProto(*wrapped_pin_proto_), response.GetArray()[0]);
  }

  void DoWaitingForEnclaveTokenForUnregister(Event event) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    access_token_fetcher_.reset();
    if (std::holds_alternative<Failure>(event)) {
      FIDO_LOG(ERROR) << "Failed to get access token for enclave";
      state_ = State::kStop;
      return;
    }

    state_ = State::kUnregistering;
    std::string token = std::move(std::get_if<AccessToken>(&event)->value());
    enclave::Transact(manager_->network_context_factory_,
                      enclave::GetEnclaveIdentity(), std::move(token),
                      /*reauthentication_token=*/std::nullopt,
                      BuildUnregisterMessage(user_->device_id()),
                      enclave::SigningCallback(),
                      base::BindOnce(&StateMachine::OnEnclaveResponse,
                                     weak_ptr_factory_.GetWeakPtr()));
  }

  void DoUnregistering(Event event) {
    state_ = State::kStop;
    if (std::holds_alternative<Failure>(event)) {
      return;
    }

    cbor::Value response =
        std::move(std::get_if<EnclaveResponse>(&event)->value());
    if (!IsAllOk(response, 1)) {
      FIDO_LOG(ERROR) << "Unregister request resulted in error response: "
                      << cbor::DiagnosticWriter::Write(response);
      return;
    }

    success_ = true;
  }

  // Start the process of uploading a Vault, and inserting it into the security
  // domain, based on an enclave response. The `response` value should be an
  // element from an enclave's response array. I.e. including the "ok" wrapping.
  // It's assumed that `IsAllOk` has been checked and that the response is not
  // an error. The `vault_` and `member_keys_source_` fields will be updated on
  // success.
  bool UploadVaultAndMemberFromResponse(const PinMetadata& pin_metadata,
                                        const cbor::Value& response) {
    const cbor::Value& response_value =
        response.GetMap()
            .find(cbor::Value(enclave::kResponseSuccessKey))
            ->second;
    if (!response_value.is_map()) {
      if (is_pin_renewal_) {
        base::UmaHistogramEnumeration(
            kPinRenewalFailureHistogram,
            PinRenewalFailureCause::kEnclaveResponse1);
      }
      FIDO_LOG(ERROR) << "response was not a map";
      return false;
    }
    const int32_t key_version = GetCurrentWrappedSecretForUser(user_).first;
    std::optional<std::pair<EnclaveRecoveryKeyStoreWrapResponse,
                            trusted_vault::MemberKeysSource>>
        result = ParseVaultAndMemberResponse(key_version, pin_metadata,
                                             response_value.GetMap());
    if (!result) {
      if (is_pin_renewal_) {
        base::UmaHistogramEnumeration(
            kPinRenewalFailureHistogram,
            PinRenewalFailureCause::kEnclaveResponse2);
      }
      return false;
    }
    std::tie(recovery_key_store_wrap_response_, member_keys_source_) =
        std::move(*result);

    state_ = State::kWaitingForRecoveryKeyStore;
    recovery_key_store_request_ =
        manager_->recovery_key_store_conn_->UpdateRecoveryKeyStore(
            *primary_account_info_, *recovery_key_store_wrap_response_->vault,
            base::BindOnce(
                [](base::WeakPtr<StateMachine> machine,
                   trusted_vault::RecoveryKeyStoreStatus status) {
                  if (!machine) {
                    return;
                  }
                  machine->Process(status);
                },
                weak_ptr_factory_.GetWeakPtr()));
    return true;
  }

  void JoinSecurityDomain() {
    state_ = State::kJoiningDomain;
    const auto secure_box_pub_key =
        trusted_vault::SecureBoxPublicKey::CreateByImport(
            base::as_byte_span(user_->member_public_key()));
    join_request_ = manager_->trusted_vault_conn_->RegisterAuthenticationFactor(
        *primary_account_info_,
        trusted_vault::GetTrustedVaultKeysWithVersions(
            store_keys_args_for_joining_->keys,
            store_keys_args_for_joining_->last_key_version),
        *secure_box_pub_key, trusted_vault::LocalPhysicalDevice(),
        base::BindOnce(&StateMachine::OnJoinedSecurityDomain,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void GetAccessTokenInternal() {
    access_token_fetcher_ =
        std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
            signin::OAuthConsumerId::kEnclaveManager,
            manager_->identity_manager_,
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

  void OnEnclaveResponse(
      base::expected<cbor::Value, enclave::TransactError> response) {
    if (!response.has_value()) {
      Process(Failure());
    } else {
      Process(EnclaveResponse(std::move(response.value())));
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

#if BUILDFLAG(IS_MAC)
  void JoinICloudKeychainToDomain(
      std::unique_ptr<trusted_vault::ICloudRecoveryKey> icloud_recovery_key) {
    std::vector<trusted_vault::TrustedVaultKeyAndVersion> member_keys_source =
        trusted_vault::GetTrustedVaultKeysWithVersions(
            {manager_->secret_}, manager_->secret_version_);
    join_request_ = manager_->trusted_vault_conn_->RegisterAuthenticationFactor(
        *primary_account_info_, std::move(member_keys_source),
        icloud_recovery_key->key()->public_key(),
        trusted_vault::ICloudKeychain(),
        base::BindOnce(&StateMachine::OnJoinedSecurityDomain,
                       weak_ptr_factory_.GetWeakPtr()));
  }
#endif  // BUILDFLAG(IS_MAC)

  // Constructed a wrapped version of the hashed PIN that will be part of the
  // virtual member metadata. This inner CBOR structure contains everything that
  // the enclave would need when processing a PIN and is authenticated (and
  // encrypted) by the security domain secret.
  static std::string BuildWrappedPIN(
      const HashedPIN& hashed_pin,
      base::span<const uint8_t, 32> claim_key,
      const EnclaveRecoveryKeyStoreWrapResponse& vault_details,
      base::span<const uint8_t> security_domain_secret) {
    cbor::Value::MapValue map;
    map.emplace(1, base::span<const uint8_t>(hashed_pin.hashed));
    if (base::FeatureList::IsEnabled(device::kWebAuthnSendPinGeneration)) {
      map.emplace(2, 0);  // Generation number.
    }
    map.emplace(3, claim_key);
    map.emplace(4, base::as_byte_span(
                       vault_details.vault->vault_parameters().counter_id()));
    // The vault handle in the wrapped PIN doesn't include the first byte,
    // which is the type of the vault entry.
    map.emplace(5, base::as_byte_span(
                       vault_details.vault->vault_parameters().vault_handle())
                       .subspan<1>());
    if (base::FeatureList::IsEnabled(device::kWebAuthnWrapCohortData)) {
      map.emplace(6, vault_details.cert_xml_serial_number);
      map.emplace(7, vault_details.cohort_public_key);
    }
    const std::vector<uint8_t> cbor_bytes =
        cbor::Writer::Write(cbor::Value(std::move(map))).value();
    return VecToString(EncryptWrappedPIN(security_domain_secret, cbor_bytes));
  }

  void DownloadRecoveryKeyStoreKeys() {
    state_ = State::kDownloadingRecoveryKeyStoreKeys;
    cert_xml_loader_ = FetchURL(
        manager_->url_loader_factory_.get(),
        GetUrl(device::enclave::kCertXmlUrlFeature),
        base::BindOnce(&StateMachine::FetchComplete,
                       weak_ptr_factory_.GetWeakPtr(), FetchedFile::kCertFile));
    sig_xml_loader_ = FetchURL(
        manager_->url_loader_factory_.get(),
        GetUrl(device::enclave::kSigXmlUrlFeature),
        base::BindOnce(&StateMachine::FetchComplete,
                       weak_ptr_factory_.GetWeakPtr(), FetchedFile::kSigFile));
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
  std::unique_ptr<trusted_vault::TrustedVaultConnection::Request>
      download_account_state_request_;
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;
  std::unique_ptr<network::SimpleURLLoader> cert_xml_loader_;
  std::unique_ptr<network::SimpleURLLoader> sig_xml_loader_;
  std::unique_ptr<network::SimpleURLLoader> upload_loader_;
  std::optional<std::string> cert_xml_;
  std::optional<std::string> sig_xml_;
  std::unique_ptr<HashedPIN> hashed_pin_;
  std::optional<EnclaveRecoveryKeyStoreWrapResponse>
      recovery_key_store_wrap_response_;
  std::unique_ptr<trusted_vault::RecoveryKeyStoreConnection::Request>
      recovery_key_store_request_;
  std::optional<cbor::Value> wrapping_response_;
  // True if a PIN is being hashed in order to add to an existing account.
  bool is_set_pin_ = false;
  // True if a PIN is being hashed in order to change it, rather than to set
  // a new PIN on an account.
  bool is_pin_update_ = false;
  // True if the GPM PIN is being renewed without knowing or changing it.
  bool is_pin_renewal_ = false;
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
          std::make_unique<IdentityObserver>(identity_manager_, this)) {
  // Automatically load the enclave state shortly after startup so that any
  // renewals will be considered without the user having to do something to
  // trigger a WebAuthn operation.
  LoadAfterDelay(base::Minutes(4), base::DoNothing());
  // Also consider renewing the PIN every day, for users who keep Chrome open
  // for long periods.
  renewal_timer_.Start(FROM_HERE, base::Hours(24),
                       base::BindRepeating(&EnclaveManager::ConsiderPinRenewal,
                                           weak_ptr_factory_.GetWeakPtr()));
}

EnclaveManager::~EnclaveManager() = default;

EnclaveManager* EnclaveManager::GetEnclaveManager() {
  return this;
}

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

void EnclaveManager::LoadAfterDelay(base::TimeDelta delay,
                                    base::OnceClosure closure) {
  load_timer_.Start(
      FROM_HERE, delay,
      base::BindOnce(&EnclaveManager::Load, weak_ptr_factory_.GetWeakPtr(),
                     std::move(closure)));
}

void EnclaveManager::Load(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (is_loaded()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(closure));
    return;
  }

  load_duration_timer_ = std::make_unique<base::ElapsedTimer>();

  load_callbacks_.emplace_back(std::move(closure));
  load_callbacks_.emplace_back(
      base::BindOnce(&EnclaveManager::NotifyObserversThatStateUpdated,
                     weak_ptr_factory_.GetWeakPtr()));
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

std::unique_ptr<EnclaveManager::StoreKeysLock>
EnclaveManager::GetStoreKeysLock() {
  store_keys_lock_depth_++;
  return std::make_unique<StoreKeysLock>(GetWeakPtr());
}

bool EnclaveManager::AddDeviceToAccount(
    std::optional<trusted_vault::GpmPinMetadata> pin_metadata,
    EnclaveManager::Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(has_pending_keys());

  std::unique_ptr<EnclaveLocalState::WrappedPIN> wrapped_pin;
  if (pin_metadata.has_value() && pin_metadata->usable_pin_metadata) {
    wrapped_pin = std::make_unique<EnclaveLocalState::WrappedPIN>();
    if (!wrapped_pin->ParseFromString(
            pin_metadata->usable_pin_metadata->wrapped_pin) ||
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
    std::optional<std::string> previous_pin_public_key,
    EnclaveManager::Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(has_pending_keys());

  auto action = std::make_unique<PendingAction>();
  action->pin_public_key = std::move(previous_pin_public_key);
  action->callback = std::move(callback);
  action->store_keys_args = std::move(pending_keys_);
  action->pin = std::move(pin);
  pending_actions_.emplace_back(std::move(action));
  Act();
}

void EnclaveManager::SetPIN(std::string pin,
                            std::string rapt,
                            EnclaveManager::Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(user_->registered());

  auto action = std::make_unique<PendingAction>();
  action->callback = std::move(callback);
  action->set_pin = std::move(pin);
  action->rapt = std::move(rapt);
  pending_actions_.emplace_back(std::move(action));
  Act();
}

void EnclaveManager::ChangePIN(std::string updated_pin,
                               std::string rapt,
                               EnclaveManager::Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(user_->registered());

  auto action = std::make_unique<PendingAction>();
  action->callback = std::move(callback);
  action->updated_pin = std::move(updated_pin);
  action->rapt = std::move(rapt);
  pending_actions_.emplace_back(std::move(action));
  Act();
}

void EnclaveManager::RenewPIN(EnclaveManager::Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(user_->registered());
  CHECK(user_->has_wrapped_pin());

  auto action = std::make_unique<PendingAction>();
  action->callback = std::move(callback);
  action->renew_pin = true;
  pending_actions_.emplace_back(std::move(action));
  Act();
}

#if BUILDFLAG(IS_MAC)
void EnclaveManager::AddICloudRecoveryKey(
    std::unique_ptr<trusted_vault::ICloudRecoveryKey> icloud_recovery_key,
    EnclaveManager::Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(user_->registered());
  CHECK(!secret_.empty())
      << "AddICloudRecoveryKey must be called immediately after registration "
         "and before discarding the security domain secret";
  auto action = std::make_unique<PendingAction>();
  action->callback = std::move(callback);
  action->icloud_recovery_key = std::move(icloud_recovery_key);
  pending_actions_.emplace_back(std::move(action));
  Act();
}
#endif  // BUILDFLAG(IS_MAC)

void EnclaveManager::Unenroll(EnclaveManager::Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto action = std::make_unique<PendingAction>();
  action->callback =
      base::BindOnce(&EnclaveManager::UnregisterComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));
  action->unregister = true;

  if (!user_ || !is_registered()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(action->callback), true));
    return;
  }

  pending_actions_.emplace_back(std::move(action));
  Act();
}

bool EnclaveManager::ConsiderSecurityDomainState(
    const trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult&
        state,
    EnclaveManager::Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(user_);
  bool ret = is_ready();

  if (IsSecurityDomainReset(state)) {
    ClearRegistration();
    FIDO_LOG(EVENT) << "The security domain has been reset.";
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
    return false;
  }

  if (ret && state.gpm_pin_metadata.has_value() &&
      state.gpm_pin_metadata->usable_pin_metadata) {
    const auto& metadata = *state.gpm_pin_metadata;
    auto wrapped_pin = std::make_unique<EnclaveLocalState::WrappedPIN>();
    if (wrapped_pin->ParseFromString(
            metadata.usable_pin_metadata->wrapped_pin) &&
        !CheckPINInvariants(*wrapped_pin).has_value()) {
      if (metadata.public_key.has_value() &&
          (!user_->has_wrapped_pin() ||
           user_->wrapped_pin().wrapped_pin() != wrapped_pin->wrapped_pin())) {
        std::unique_ptr<PendingAction> action =
            std::make_unique<PendingAction>();
        action->callback = std::move(callback);
        action->update_wrapped_pin = true;
        action->wrapped_pin = std::move(wrapped_pin);
        action->pin_public_key = *metadata.public_key;
        FIDO_LOG(EVENT) << "The GPM PIN has been updated";
        pending_actions_.emplace_back(std::move(action));
        Act();
      }
    } else {
      FIDO_LOG(ERROR) << "Wrapped PIN from security domain update is invalid: "
                      << base::HexEncode(base::as_byte_span(
                             metadata.usable_pin_metadata->wrapped_pin));
    }
  }

  return ret;
}

void EnclaveManager::GetIdentityKeyForSignature(
    base::OnceCallback<void(
        scoped_refptr<unexportable_keys::RefCountedUnexportableSigningKey>)>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!user_ || user_->wrapped_identity_private_key().empty()) {
    std::move(callback).Run(nullptr);
    return;
  }

  if (identity_key_) {
    std::move(callback).Run(identity_key_);
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
        enclave_manager->identity_key_ = base::MakeRefCounted<
            unexportable_keys::RefCountedUnexportableSigningKey>(
            std::move(key), unexportable_keys::UnexportableKeyId());
        std::move(callback).Run(enclave_manager->identity_key_);
      },
      weak_ptr_factory_.GetWeakPtr(), primary_account_info_->account_id,
      std::move(callback));

  // Retrieve the key on a non-UI thread, and post a task back to the current
  // thread that invokes `key_callback` with the obtained key.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(
          [](std::string wrapped_identity_private_key)
              -> std::unique_ptr<crypto::UnexportableSigningKey> {
            std::unique_ptr<crypto::UnexportableKeyProvider> provider =
                GetWebAuthnUnexportableKeyProvider();
            if (!provider) {
              return nullptr;
            }
            return provider->FromWrappedSigningKeySlowly(
                ToVector(wrapped_identity_private_key));
          },
          user_->wrapped_identity_private_key()),
      std::move(key_callback));
}

enclave::SigningCallback EnclaveManager::IdentityKeySigningCallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!user_->wrapped_identity_private_key().empty());
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
                            key->key().IsHardwareBacked()
                                ? enclave::ClientKeyType::kHardware
                                : enclave::ClientKeyType::kSoftware;
                        return std::move(client_signature);
                      },
                      std::move(device_id), std::move(message_to_be_signed),
                      key),
                  std::move(result_callback));
            },
            enclave_manager->user_->device_id(),
            std::move(message_to_be_signed), std::move(result_callback));

        enclave_manager->GetIdentityKeyForSignature(
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
    FIDO_LOG(ERROR) << "Attempted a UV signature but no key is available";
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

  auto user_verifying_key_provider =
      GetUserVerifyingKeyProviderForSigning(std::move(options));
  if (!user_verifying_key_provider) {
    FIDO_LOG(ERROR)
        << "Attempted a UV signature but UV key provider is unavailable";
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
         base::expected<std::unique_ptr<crypto::UserVerifyingSigningKey>,
                        crypto::UserVerifyingKeyCreationError> maybe_key) {
        if (!enclave_manager ||
            enclave_manager->primary_account_info_->account_id != account_id) {
          FIDO_LOG(ERROR) << "Primary user no longer available for UV key "
                             "signature generation";
          std::move(callback).Run(nullptr);
          return;
        }
        if (!maybe_key.has_value()) {
          FIDO_LOG(ERROR) << "UV key retrieval failed with error "
                          << static_cast<int>(maybe_key.error());
          enclave_manager->ClearRegistration();
          std::move(callback).Run(nullptr);
          return;
        }
        enclave_manager->user_verifying_key_ =
            base::MakeRefCounted<crypto::RefCountedUserVerifyingSigningKey>(
                std::move(maybe_key.value()));
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
                      [](std::string device_id, const bool is_hardware_backed,
                         base::OnceCallback<void(
                             std::optional<enclave::ClientSignature>)>
                             result_callback,
                         base::expected<std::vector<uint8_t>,
                                        crypto::UserVerifyingKeySigningError>
                             maybe_signature) {
                        if (!maybe_signature.has_value()) {
                          FIDO_LOG(ERROR)
                              << "UV key signature failed with error "
                              << static_cast<int>(maybe_signature.error());
                          std::move(result_callback).Run(std::nullopt);
                          return;
                        }
                        enclave::ClientSignature client_signature;
                        client_signature.device_id = ToVector(device_id);
                        client_signature.signature =
                            std::move(maybe_signature.value());
                        client_signature.key_type =
                            is_hardware_backed
                                ? enclave::ClientKeyType::kUserVerified
                                : enclave::ClientKeyType::kSoftwareUserVerified;
                        std::move(result_callback)
                            .Run(std::move(client_signature));
                      },
                      std::move(device_id),
                      uv_signing_key->key().IsHardwareBacked(),
                      std::move(result_callback)));
            },
            enclave_manager->user_->device_id(),
            std::move(message_to_be_signed), std::move(result_callback));

        enclave_manager->GetUserVerifyingKeyForSignature(
            std::move(options), std::move(signing_callback));
      },
      std::move(options), weak_ptr_factory_.GetWeakPtr());
}

std::pair<std::unique_ptr<EnclaveManager::UvKeyCreationLock>,
          device::enclave::UVKeyCreationCallback>
EnclaveManager::UserVerifyingKeyCreationCallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(user_->deferred_uv_key_creation());
  CHECK(user_->registered());
  return {
      TakeUvKeyCreationLock(),
      base::BindOnce(
          [](base::WeakPtr<EnclaveManager> enclave_manager,
             CoreAccountId account_id,
             base::OnceCallback<void(base::span<const uint8_t>)>
                 public_key_callback) {
            if (!enclave_manager) {
              std::move(public_key_callback).Run(std::vector<uint8_t>());
              return;
            }
            // Unregister the device with the enclave if there are any errors
            // from this point, because UV key creation is a necessary step to
            // have a usable state.
            //
            // The key provider is only used for creating a new key, not for
            // signing, so passing empty options here is ok.
            auto key_provider =
                GetUserVerifyingKeyProviderForCreateAndDeleteOnly();
            if (!key_provider) {
              enclave_manager->OnDeferredUvKeyCreationFailure();
              std::move(public_key_callback).Run(std::vector<uint8_t>());
              return;
            }
            key_provider->GenerateUserVerifyingSigningKey(
                device::enclave::kSigningAlgorithms,
                base::BindOnce(
                    [](base::WeakPtr<EnclaveManager> enclave_manager,
                       base::OnceCallback<void(base::span<const uint8_t>)>
                           public_key_callback,
                       CoreAccountId account_id,
                       base::expected<
                           std::unique_ptr<crypto::UserVerifyingSigningKey>,
                           crypto::UserVerifyingKeyCreationError>
                           maybe_uv_key) {
                      if (!enclave_manager ||
                          enclave_manager->primary_account_info_->account_id !=
                              account_id) {
                        FIDO_LOG(ERROR)
                            << "Primary user no longer available for "
                               "deferred UV key creation";
                        std::move(public_key_callback)
                            .Run(std::vector<uint8_t>());
                        return;
                      }
                      if (!maybe_uv_key.has_value()) {
                        FIDO_LOG(ERROR)
                            << "Failed deferred UV key creation with error "
                            << static_cast<int>(maybe_uv_key.error());
                        // If the user cancelled the verification, they should
                        // get a chance to try again on a future request.
                        // Otherwise the device is unregistered so they can
                        // attempt recovery later.
                        if (maybe_uv_key.error() !=
                            crypto::UserVerifyingKeyCreationError::
                                kUserCancellation) {
                          enclave_manager->OnDeferredUvKeyCreationFailure();
                        }
                        std::move(public_key_callback)
                            .Run(std::vector<uint8_t>());
                        return;
                      }
                      enclave_manager->user_verifying_key_ =
                          base::MakeRefCounted<
                              crypto::RefCountedUserVerifyingSigningKey>(
                              std::move(maybe_uv_key.value()));
                      const std::vector<uint8_t> uv_public_key =
                          enclave_manager->user_verifying_key_->key()
                              .GetPublicKey();
                      const std::string uv_public_key_str =
                          VecToString(uv_public_key);

                      auto* local_state =
                          StateForUser(enclave_manager->local_state_.get(),
                                       *enclave_manager->primary_account_info_);
                      local_state->set_uv_public_key(uv_public_key_str);
                      local_state->set_wrapped_uv_private_key(
                          UserVerifyingLabelToString(
                              enclave_manager->user_verifying_key_->key()
                                  .GetKeyLabel()));
                      local_state->set_deferred_uv_key_creation(false);
                      enclave_manager->WriteState(
                          enclave_manager->local_state_.get());
                      enclave_manager->OnDeferredUvKeyCreationSuccess();

                      std::move(public_key_callback).Run(uv_public_key);
                    },
                    enclave_manager, std::move(public_key_callback),
                    std::move(account_id)));
          },
          weak_ptr_factory_.GetWeakPtr(), primary_account_info_->account_id)};
}

void EnclaveManager::OnDeferredUvKeyCreationFailure() {
  ClearRegistration();
  deferred_uv_key_creation_successful_ = false;
}

void EnclaveManager::OnDeferredUvKeyCreationSuccess() {
  deferred_uv_key_creation_successful_ = true;
}

std::unique_ptr<EnclaveManager::UvKeyCreationLock>
EnclaveManager::TakeUvKeyCreationLock() {
  CHECK(!deferred_uv_key_creation_in_progress_);
  deferred_uv_key_creation_in_progress_ = true;
  return std::make_unique<UvKeyCreationLockImpl>(
      (base::BindOnce(&EnclaveManager::OnUvKeyCreationLockReleased,
                      weak_ptr_factory_.GetWeakPtr())));
}

void EnclaveManager::OnUvKeyCreationLockReleased() {
  CHECK(deferred_uv_key_creation_in_progress_);
  deferred_uv_key_creation_in_progress_ = false;

  // If the success bit hasn't been set, it means a transaction was destroyed
  // before attempting UV key creation. By passing `true` to the pending
  // transactions, the next one can attempt to create one.
  bool success = deferred_uv_key_creation_successful_.has_value()
                     ? *deferred_uv_key_creation_successful_
                     : true;
  if (!pending_uv_key_requests_.empty()) {
    std::vector<base::OnceCallback<void(bool)>> callbacks;
    pending_uv_key_requests_.swap(callbacks);

    for (auto& callback : callbacks) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), success));
    }
  }
}

void EnclaveManager::AddPendingUvRequest(
    base::OnceCallback<void(bool)> callback) {
  CHECK(deferred_uv_key_creation_in_progress_);
  pending_uv_key_requests_.emplace_back(std::move(callback));
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

void EnclaveManager::SetWrappedPINDataForTesting(
    std::vector<uint8_t> wrapped_pin_data) {
  CHECK(has_wrapped_pin());
  const_cast<webauthn_pb::EnclaveLocalState_User&>(*user_)
      .mutable_wrapped_pin()
      ->set_wrapped_pin(VecToString(wrapped_pin_data));
}

EnclaveManager::UvKeyState EnclaveManager::uv_key_state(
    bool platform_has_biometrics) const {
  CHECK(is_ready());
#if BUILDFLAG(IS_WIN)
  if (user_->deferred_uv_key_creation()) {
    return UvKeyState::kUsesSystemUIDeferredCreation;
  }
#endif
  if (user_->wrapped_uv_private_key().empty()) {
    return UvKeyState::kNone;
  }
#if BUILDFLAG(IS_MAC)
  if (platform_has_biometrics) {
    // Chrome will display an LAAuthenticationView with a Touch ID prompt.
    return UvKeyState::kUsesChromeUI;
  }
  // Delegate prompting the user for their screen lock to macOS.
  return UvKeyState::kUsesSystemUI;
#else
  return UvKeyState::kUsesSystemUI;
#endif
}

void EnclaveManager::CheckGpmPinAvailability(
    GpmPinAvailabilityCallback callback) {
  CoreAccountInfo account_info =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  download_account_state_request_ =
      trusted_vault_conn_->DownloadAuthenticationFactorsRegistrationState(
          account_info,
          base::BindOnce(&EnclaveManager::OnCheckGpmPinAvailabilityResult,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
          base::DoNothing());
}

// static
void EnclaveManager::AreUserVerifyingKeysSupported(Callback callback) {
  if (base::FeatureList::IsEnabled(
          device::kWebAuthnUseInsecureSoftwareUnexportableKeys)) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
    return;
  }
#if BUILDFLAG(IS_CHROMEOS)
  // ChromeOS doesn't have HW-backed UV keys, but uses a software provider.
  std::move(callback).Run(true);
#else
  crypto::AreUserVerifyingKeysSupported(
      MakeUserVerifyingKeyConfig(/*options=*/{}), std::move(callback));
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

void EnclaveManager::StorePendingKeys(const GaiaId& gaia_id,
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

void EnclaveManager::StoreKeys(const GaiaId& gaia_id,
                               std::vector<std::vector<uint8_t>> keys,
                               int last_key_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (base::FeatureList::IsEnabled(device::kWebAuthnOpportunisticRetrieval)) {
    if (store_keys_lock_depth_) {
      webauthn::metrics::RecordGPMRecoveryEvent(
          webauthn::metrics::WebAuthenticationGPMRecoveryEvent::
              kStoreKeysFromExplicitFlowStarted);
      StorePendingKeys(gaia_id, std::move(keys), last_key_version);
    } else {
      webauthn::metrics::RecordGPMRecoveryEvent(
          webauthn::metrics::WebAuthenticationGPMRecoveryEvent::
              kStoreKeysFromOpportunisticFlowStarted);
      // TODO(crbug.com/450851888): Refactor the logic related to storing the
      // keys from the out of context retrieval.
      StoreKeysFromOutOfContextRetrieval(gaia_id, std::move(keys),
                                         last_key_version);
    }
  } else {
    // Use the old implementation:
    StorePendingKeys(gaia_id, std::move(keys), last_key_version);
  }
}

void EnclaveManager::StoreKeysFromOutOfContextRetrieval(
    const GaiaId& gaia_id,
    std::vector<std::vector<uint8_t>> keys,
    int last_key_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!store_keys_lock_depth_);

  auto pending_keys = std::make_unique<StoreKeysArgs>();
  pending_keys->gaia_id = gaia_id;
  pending_keys->keys = std::move(keys);
  pending_keys->last_key_version = last_key_version;

  if (is_registered()) {
    FIDO_LOG(EVENT) << "Redundant opportunistic keys provided for version "
                    << last_key_version;
    NotifyObserversAboutOutOfContextRecoveryOutcome(
        OutOfContextRecoveryOutcome::
            kStoreKeysFromOpportunisticFlowIgnoredRedundant);
    return;
  }

  FIDO_LOG(EVENT) << "Opportunistic keys provided";
  // These keys were provided opportunistically so that a MagicArch flow can
  // be avoided in the future. However, as an invariant, we only register with
  // the enclave if we can serve requests, which means having a form of local
  // user verification (either system UV or GPM PIN).
  AreUserVerifyingKeysSupported(
      base::BindOnce(&EnclaveManager::OpportunisticStoreKeysUVCheckComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(pending_keys)));
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
  aead.Init(base::as_byte_span(wrapped_pin->claim_key()));
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

EnclaveLocalState& EnclaveManager::local_state_for_testing() {
  return *local_state_;
}

void EnclaveManager::ClearCachedKeysForTesting() {
  user_verifying_key_ = nullptr;
  identity_key_ = nullptr;
}

void EnclaveManager::ResetForTesting() {
  store_keys_count_ = 0;
  user_verifying_key_ = nullptr;
  identity_key_ = nullptr;
  secret_.clear();
  secret_version_ = -1;
  pending_actions_.clear();
  load_callbacks_.clear();
  state_machine_.reset();
  pending_keys_.reset();
  currently_writing_ = false;
  pending_write_ = std::nullopt;
  identity_observer_.reset();
  primary_account_info_.reset();
  user_ = nullptr;
  local_state_.reset();
  loading_ = false;
}

void EnclaveManager::ClearRegistrationForTesting() {
  ClearRegistration();
}

// static
void EnclaveManager::EnableInvariantChecksForTesting(bool enabled) {
  g_invariant_override_ = !enabled;
}

void EnclaveManager::ConsiderPinRenewalForTesting() {
  ConsiderPinRenewal();
}

unsigned EnclaveManager::renewal_checks_for_testing() const {
  return renewal_checks_;
}

unsigned EnclaveManager::renewal_attempts_for_testing() const {
  LOG(ERROR) << __func__ << " " << renewal_attempts_;
  return renewal_attempts_;
}

// static
std::string EnclaveManager::MakeWrappedPINForTesting(
    base::span<const uint8_t> security_domain_secret,
    std::string_view pin) {
  std::unique_ptr<HashedPIN> hashed = HashPINSlowly(pin);
  std::unique_ptr<EnclaveLocalState::WrappedPIN> wrapped_pin =
      hashed->ToWrappedPIN();
  const uint8_t kFakeCounterId[8] = {};
  const uint8_t kFakeVaultHandle[16] = {};
  const uint8_t kFakeCohortPublicKey[16] = {};
  const int32_t kFakeSerialNumber = 1;

  cbor::Value::MapValue map;
  map.emplace(1, base::span<const uint8_t>(hashed->hashed));
  // 2 used to correspond to the generation.
  map.emplace(3, ToSizedSpan<32>(wrapped_pin->claim_key()));
  map.emplace(4, base::span<const uint8_t>(kFakeCounterId));
  map.emplace(5, base::span<const uint8_t>(kFakeVaultHandle));
  map.emplace(6, base::span<const uint8_t>(kFakeCohortPublicKey));
  map.emplace(7, kFakeSerialNumber);
  const std::vector<uint8_t> cbor_bytes =
      cbor::Writer::Write(cbor::Value(std::move(map))).value();
  wrapped_pin->set_wrapped_pin(
      VecToString(EncryptWrappedPIN(security_domain_secret, cbor_bytes)));
  return wrapped_pin->SerializeAsString();
}

// static
std::vector<uint8_t> EnclaveManager::EncryptWrappedPIN(
    base::span<const uint8_t> security_domain_secret,
    base::span<const uint8_t> cbor_bytes) {
  // This is "KeychainApplicationKey:chrome:GPM PIN data wrapping key".
  static constexpr uint8_t kKeyPurposePinDataKey[] = {
      0x4b, 0x65, 0x79, 0x63, 0x68, 0x61, 0x69, 0x6e, 0x41, 0x70, 0x70,
      0x6c, 0x69, 0x63, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x4b, 0x65, 0x79,
      0x3a, 0x63, 0x68, 0x72, 0x6f, 0x6d, 0x65, 0x3a, 0x47, 0x50, 0x4d,
      0x20, 0x50, 0x49, 0x4e, 0x20, 0x64, 0x61, 0x74, 0x61, 0x20, 0x77,
      0x72, 0x61, 0x70, 0x70, 0x69, 0x6e, 0x67, 0x20, 0x6b, 0x65, 0x79};
  const std::array<uint8_t, 32> derived_key = crypto::HkdfSha256<32>(
      security_domain_secret, /*salt=*/base::span<const uint8_t>(),
      kKeyPurposePinDataKey);
  crypto::Aead aead(crypto::Aead::AeadAlgorithm::AES_256_GCM);
  aead.Init(derived_key);
  uint8_t nonce[12];
  crypto::RandBytes(nonce);
  std::vector<uint8_t> wrapped_pin = aead.Seal(
      cbor_bytes, nonce, /*additional_data=*/base::span<const uint8_t>());
  wrapped_pin.insert(wrapped_pin.begin(), std::begin(nonce), std::end(nonce));
  return wrapped_pin;
}

void EnclaveManager::Act() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!local_state_) {
    if (loading_) {
      return;
    }

    loading_ = true;

    if (!encryptor_.has_value()) {
      g_browser_process->os_crypt_async()->GetInstance(base::BindOnce(
          &EnclaveManager::OnOsCryptReady, weak_ptr_factory_.GetWeakPtr()));
      return;
    }

    base::OnceCallback<void(std::optional<std::string>)> decryption_callback =
        base::BindOnce(
            [](base::WeakPtr<EnclaveManager> manager,
               std::optional<std::string> contents) {
              if (!manager) {
                return;
              }
              std::string decrypted;
              if (!contents.has_value() ||
                  !manager->encryptor_->DecryptString(*contents, &decrypted)) {
                manager->LoadComplete(std::nullopt);
                return;
              }
              manager->LoadComplete(std::move(decrypted));
            },
            weak_ptr_factory_.GetWeakPtr());

    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::USER_BLOCKING, base::MayBlock()},
        base::BindOnce(
            [](base::FilePath path) -> std::optional<std::string> {
              std::string contents;
              if (!base::ReadFileToString(path, &contents)) {
                return std::nullopt;
              }

              return std::move(contents);
            },
            file_path_),
        std::move(decryption_callback));
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

  if (load_duration_timer_) {
    base::UmaHistogramTimes("WebAuthentication.EnclaveLoadDuration",
                            load_duration_timer_->Elapsed());
    load_duration_timer_.reset();
  }

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
  identity_key_.reset();

  const signin::AccountsInCookieJarInfo in_jar =
      identity_manager_->GetAccountsInCookieJar();
  if (in_jar.AreAccountsFresh()) {
    // If the user has signed out of any non-primary accounts, erase their
    // enclave state.
    const base::flat_set<GaiaId> gaia_ids_in_cookie_jar =
        base::STLSetUnion<base::flat_set<GaiaId>>(
            GetGaiaIDs(in_jar.GetPotentiallyInvalidSignedInAccounts()),
            GetGaiaIDs(in_jar.GetSignedOutAccounts()));
    const base::flat_set<GaiaId> gaia_ids_in_state =
        GetGaiaIDs(local_state_->users());
    base::flat_set<GaiaId> to_remove =
        base::STLSetDifference<base::flat_set<GaiaId>>(gaia_ids_in_state,
                                                       gaia_ids_in_cookie_jar);
    if (primary_account_info_) {
      to_remove.erase(primary_account_info_->gaia);
    }
    // A `StateMachine` can also mutate the enclave state. Thus if we're about
    // to mutate it ourselves, confirm that any `StateMachine` is about to be
    // stopped and thus cannot overwrite these changes.
    CHECK(need_to_stop);
    for (const auto& gaia_id : to_remove) {
      CHECK(local_state_->mutable_users()->erase(gaia_id.ToString()));
    }
    WriteState(local_state_.get());
  }

  if (need_to_stop && !is_post_load) {
    CancelAllActions();
    Stopped();
  }

  ConsiderPinRenewal();
}

void EnclaveManager::Stopped() {
  state_machine_.reset();
  Act();
  NotifyObserversThatStateUpdated();
}

void EnclaveManager::NotifyObserversThatStateUpdated() {
  for (Observer& observer : observer_list_) {
    observer.OnStateUpdated();
  }
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
      crypto::SHA256Hash(base::as_byte_span(serialized));
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
  CHECK(encryptor_.has_value());

  currently_writing_ = true;

  std::string encrypted;
  if (!encryptor_->EncryptString(serialized, &encrypted)) {
    WriteStateComplete(false);
    return;
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(
          [](base::FilePath path, std::string encrypted) -> bool {
            return base::ImportantFileWriter::WriteFileAtomically(path,
                                                                  encrypted);
          },
          file_path_, std::move(encrypted)),
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
  identity_key_.reset();

  // Delete keys from the platform as a cleanup. Failures are ignored because
  // there is nothing to be done in that case.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(
          [](std::vector<uint8_t> wrapped_identity_private_key) {
            std::unique_ptr<crypto::UnexportableKeyProvider> provider =
                GetWebAuthnUnexportableKeyProvider();
            if (crypto::StatefulUnexportableKeyProvider* stateful_provider =
                    provider ? provider->AsStatefulUnexportableKeyProvider()
                             : nullptr) {
              stateful_provider->DeleteSigningKeySlowly(
                  wrapped_identity_private_key);
            }
          },
          ToVector(user_->wrapped_identity_private_key())));
  if (!user_->wrapped_uv_private_key().empty()) {
    // The key provider is only used to delete, not sign, so passing empty
    // options here is ok.
    if (auto user_verifying_key_provider =
            GetUserVerifyingKeyProviderForCreateAndDeleteOnly()) {
      auto key_label =
          UserVerifyingKeyLabelFromString(user_->wrapped_uv_private_key());
      CHECK(key_label);

      user_verifying_key_provider->DeleteUserVerifyingKey(std::move(*key_label),
                                                          base::DoNothing());
    }
  }

  user_ = nullptr;  // Prevent dangling raw_ptr error on next line.
  CHECK(local_state_->mutable_users()->erase(
      primary_account_info_->gaia.ToString()));
  user_ = CreateStateForUser(local_state_.get(), *primary_account_info_);
  WriteState(local_state_.get());

  CancelAllActions();
}

void EnclaveManager::UnregisterComplete(EnclaveManager::Callback callback,
                                        bool success) {
  if (success) {
    ClearRegistration();
  }
  std::move(callback).Run(success);
}

void EnclaveManager::SetSecret(int32_t key_version,
                               base::span<const uint8_t> secret) {
  secret_version_ = key_version;
  secret_ = std::vector<uint8_t>(secret.begin(), secret.end());
}

// A list of PIN-renewal events that are reported to UMA. Do not renumber
// as the values are persisted.
enum class PinRenewalEvent {
  kConsidered = 0,
  kNothingToRenew = 1,
  kConcurrentRenewal = 2,
  kNotYetTime = 3,
  kStarted = 4,
  kSuccess = 5,
  kFailure = 6,

  kMaxValue = kFailure,
};

static const char kPinRenewalHistogram[] = "WebAuthentication.PinRenewalEvent";

void EnclaveManager::ConsiderPinRenewal() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::UmaHistogramEnumeration(kPinRenewalHistogram,
                                PinRenewalEvent::kConsidered);

  renewal_checks_++;
  if (!user_ || !is_ready() || !user_->has_wrapped_pin()) {
    base::UmaHistogramEnumeration(kPinRenewalHistogram,
                                  PinRenewalEvent::kNothingToRenew);
    return;
  }

  if (is_renewing_) {
    base::UmaHistogramEnumeration(kPinRenewalHistogram,
                                  PinRenewalEvent::kConcurrentRenewal);
    return;
  }

  const auto now = base::Time::Now();
  const base::Time last_refreshed = base::Time::FromSecondsSinceUnixEpoch(
      user_->last_refreshed_pin_epoch_secs());
  if (last_refreshed > now || now - last_refreshed > base::Days(kRefreshDays)) {
    FIDO_LOG(EVENT) << "Renewing GPM PIN based on time since last renewal";
    renewal_attempts_++;
    is_renewing_ = true;
    base::UmaHistogramEnumeration(kPinRenewalHistogram,
                                  PinRenewalEvent::kStarted);
    RenewPIN(base::BindOnce(&EnclaveManager::OnRenewalComplete,
                            weak_ptr_factory_.GetWeakPtr()));
  } else {
    base::UmaHistogramEnumeration(kPinRenewalHistogram,
                                  PinRenewalEvent::kNotYetTime);
  }
}

void EnclaveManager::OnRenewalComplete(bool success) {
  base::UmaHistogramEnumeration(
      kPinRenewalHistogram,
      success ? PinRenewalEvent::kSuccess : PinRenewalEvent::kFailure);

  is_renewing_ = false;
}

bool EnclaveManager::IsSecurityDomainReset(
    const trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult&
        state) {
  // If the local state indicates that the user has joined the security domain,
  // but the security domain is not initialized or does not match the key
  // version, assume the security domain has been reset by another client.
  return user_->joined() &&
         state.state !=
             trusted_vault::
                 DownloadAuthenticationFactorsRegistrationStateResult::State::
                     kError &&
         (!state.key_version.has_value() ||
          user_->wrapped_security_domain_secrets().find(*state.key_version) ==
              user_->wrapped_security_domain_secrets().end());
}

void EnclaveManager::OnOsCryptReady(os_crypt_async::Encryptor encryptor) {
  CHECK(!encryptor_.has_value());
  encryptor_.emplace(std::move(encryptor));
  loading_ = false;
  Act();
}

void EnclaveManager::OnCheckGpmPinAvailabilityResult(
    base::OnceCallback<void(GpmPinAvailability)> callback,
    trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
        result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  download_account_state_request_.reset();
  auto pin_availability = GpmPinAvailability::kGpmPinUnset;
  if (result.gpm_pin_metadata) {
    pin_availability = result.gpm_pin_metadata->usable_pin_metadata
                           ? GpmPinAvailability::kGpmPinSetAndUsable
                           : GpmPinAvailability::kGpmPinSetButNotUsable;
  }
  // Calling the callback after fetching the GPM PIN info:
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), pin_availability));
}

void EnclaveManager::OpportunisticStoreKeysUVCheckComplete(
    std::unique_ptr<StoreKeysArgs> pending_keys,
    bool can_make_uv_keys) {
  FIDO_LOG(EVENT) << "Opportunistic keys UV key result: " << can_make_uv_keys;
  if (!can_make_uv_keys) {
    // Without local UV we can store keys only if the GPM pin is available and
    // usable.
    CheckGpmPinAvailability(base::BindOnce(
        &EnclaveManager::OpportunisticStoreKeysGpmPinCheckComplete,
        weak_ptr_factory_.GetWeakPtr(), std::move(pending_keys)));
    return;
  }
  OpportunisticStoreKeys(std::move(pending_keys));
}

void EnclaveManager::OpportunisticStoreKeysGpmPinCheckComplete(
    std::unique_ptr<StoreKeysArgs> pending_keys,
    GpmPinAvailability gpm_pin_availability) {
  if (gpm_pin_availability != GpmPinAvailability::kGpmPinSetAndUsable) {
    NotifyObserversAboutOutOfContextRecoveryOutcome(
        OutOfContextRecoveryOutcome::
            kStoreKeysFromOpportunisticFlowIgnoredNoUV);
    return;
  }
  OpportunisticStoreKeys(std::move(pending_keys));
}

void EnclaveManager::OpportunisticStoreKeys(
    std::unique_ptr<StoreKeysArgs> pending_keys) {
  pending_keys_ = std::move(pending_keys);
  store_keys_count_++;
  AddDeviceToAccount(
      /*pin_metadata=*/std::nullopt,
      base::BindOnce(&EnclaveManager::OpportunisticStoreKeysAddComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EnclaveManager::OpportunisticStoreKeysAddComplete(bool success) {
  FIDO_LOG(EVENT) << "Opportunistic keys device add result: " << success;
  auto outcome =
      success
          ? OutOfContextRecoveryOutcome::
                kStoreKeysFromOpportunisticFlowSucceeded
          : OutOfContextRecoveryOutcome::kStoreKeysFromOpportunisticFlowFailed;
  NotifyObserversAboutOutOfContextRecoveryOutcome(outcome);
}

void EnclaveManager::NotifyObserversAboutOutOfContextRecoveryOutcome(
    OutOfContextRecoveryOutcome outcome) {
  webauthn::metrics::RecordGPMRecoveryEvent(
      ToWebAuthenticationGPMRecoveryEvent(outcome));
  for (Observer& observer : observer_list_) {
    observer.OnOutOfContextRecoveryCompletion(outcome);
  }
}

base::WeakPtr<EnclaveManager> EnclaveManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
