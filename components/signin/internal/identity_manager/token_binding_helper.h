// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_TOKEN_BINDING_HELPER_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_TOKEN_BINDING_HELPER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/signin/public/base/binding_key_registration_token_result.h"
#include "components/signin/public/base/session_binding_utils.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "crypto/signature_verifier.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace unexportable_keys {
class UnexportableKeyService;
class UnexportableKeyLoader;
}  // namespace unexportable_keys

namespace signin {
class BindingKeyRegistrationTokenHelper;
class OAuth2UpgradeTokenFlow;
}  // namespace signin

namespace network {
class SharedURLLoaderFactory;
}

class GURL;

struct CoreAccountId;

// `TokenBindingHelper` manages in-memory cache of refresh token binding keys
// and provides an asynchronous method of creating a binding key assertion.
//
// Keys needs to be loaded into the helper on every startup.
class TokenBindingHelper {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(Error)
  enum class Error {
    // Reserved for histograms.
    // kNone = 0
    kKeyNotFound = 1,
    kLoadKeyFailure = 2,
    kCreateAssertionFaiure = 3,
    kSignAssertionFailure = 4,
    kAppendSignatureFailure = 5,
    kMaxValue = kAppendSignatureFailure
  };

  static constexpr Error kNoErrorForMetrics = static_cast<Error>(0);
  // LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:TokenBindingGenerateAssertionResult)

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(SaveBindingKeyResult)
  enum class SaveBindingKeyResult {
    kSuccess = 0,
    kRefreshTokenNotFound = 1,
    kMaxValue = kRefreshTokenNotFound,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:TokenBindingUpgradeSaveBindingKeyResult)

  using GenerateAssertionCallback = base::OnceCallback<void(std::string)>;
  using SaveBindingKeyCallback = base::RepeatingCallback<SaveBindingKeyResult(
      const CoreAccountId& account_id,
      std::string_view refresh_token,
      std::vector<uint8_t> wrapped_binding_key)>;

  explicit TokenBindingHelper(
      unexportable_keys::UnexportableKeyService& unexportable_key_service);

  TokenBindingHelper(const TokenBindingHelper&) = delete;
  TokenBindingHelper& operator=(const TokenBindingHelper&) = delete;

  ~TokenBindingHelper();

  // Adds a key associated with `account_id` to the in-memory cache.
  // If `wrapped_binding_key` is empty, removes any existing key instead.
  // The key is not loaded with `unexportable_key_service_` immediately but
  // stored as a wrapped key until an attestation is requested for the first
  // time.
  void SetBindingKey(const CoreAccountId& account_id,
                     base::span<const uint8_t> wrapped_binding_key);

  // Returns whether the helper has a non-empty key associated with
  // `account_id`.
  // This method returns `true` even if the key has failed to load.
  bool HasBindingKey(const CoreAccountId& account_id) const;

  // Removes all keys from the helper.
  // To remove a key for a specific account, use `SetBindingKey()` with an empty
  // key parameter.
  void ClearAllKeys();

  // Initiates background generation or unwrapping of a binding key after all
  // credentials are loaded on startup.
  void OnAllCredentialsLoaded(bool has_refresh_tokens);

  // Returns `true` if the binding key was successfully generated or unwrapped.
  // Returns `false` if the key hasn't been created yet or if it failed to
  // create/unwrap.
  bool IsRegistrationKeyReady() const;

  // Asynchronously generates a registration token for binding a refresh token
  // to a binding key. If one of the accounts is already bound, reuses its
  // binding key. Otherwise, generates a new binding key.
  // `supported_algorithms` is ignored if an existing binding key is reused.
  // The result is returned through `callback`. Returns `std::nullopt` if the
  // generation fails.
  void GenerateBindingKeyRegistrationToken(
      base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
          supported_algorithms,
      const std::variant<signin::TokenBindingAuthCode,
                         signin::TokenBindingChallenge>& auth_code_or_challenge,
      base::OnceCallback<void(
          std::optional<signin::BindingKeyRegistrationTokenResult>)> callback);

  // Asynchronously generates a binding key assertion with a key associated with
  // `account_id`. The result is returned through `callback`. Returns an empty
  // string if the generation fails.
  // If not empty, `ephemeral_public_key` will be added to the assertion,
  // instructing the recipient to encrypt sensitive data with this key.
  void GenerateBindingKeyAssertion(const CoreAccountId& account_id,
                                   std::string_view challenge,
                                   std::string_view ephemeral_public_key,
                                   const GURL& destination_url,
                                   GenerateAssertionCallback callback);

  // Starts garbage collection of orphaned keys.
  void StartGarbageCollection(
      absl::flat_hash_set<std::vector<uint8_t>> known_wrapped_keys_in_db);

  // Returns a wrapped key associated with `account_id`. Returns an empty vector
  // if no key is found.
  std::vector<uint8_t> GetWrappedBindingKey(
      const CoreAccountId& account_id) const;

  // Returns the number of bound tokens.
  size_t GetBoundTokenCount() const;

  // Returns whether all accounts reuse the same binding key.
  // Returns `true` if empty.
  bool AreAllBindingKeysSame() const;

  // Notifies `unexportable_key_service_` about a `wrapped_binding_key` being
  // copied from another token service. This ensures that
  // `unexportable_key_service_` properly tracks a key that was created by
  // another UnexportableKeyService instance.
  //
  // Unlike `SetBindingKey()`, this method does not add `wrapped_binding_key`
  // to the in-memory cache.
  //
  // `wrapped_binding_key` must not be empty.
  void CopyBindingKeyFromAnotherTokenService(
      base::span<const uint8_t> wrapped_binding_key);

  // Initiates an opportunistic refresh token binding upgrade upon receiving a
  // challenge from the server.
  void PerformTokenBindingUpgrade(
      const CoreAccountId& account_id,
      std::string_view refresh_token,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::string_view device_id,
      std::string_view challenge,
      base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
          supported_algorithms);

  // Sets the callback to persist the binding key after a token binding upgrade.
  // Must be called exactly once.
  void SetSaveBindingKeyCallback(SaveBindingKeyCallback callback);

  base::WeakPtr<TokenBindingHelper> GetWeakPtr();

 private:
  void MaybeInitializeRegistrationTokenHelper(
      base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
          supported_algorithms);
  void OnUpgradeRegistrationTokenGenerated(
      const CoreAccountId& account_id,
      std::optional<signin::BindingKeyRegistrationTokenResult> result);
  void OnUpgradeTokenFinished(const CoreAccountId& account_id);

  struct BindingKeyData {
    explicit BindingKeyData(std::vector<uint8_t> wrapped_key);

    BindingKeyData(const BindingKeyData&) = delete;
    BindingKeyData& operator=(const BindingKeyData&) = delete;

    BindingKeyData(BindingKeyData&& other);
    BindingKeyData& operator=(BindingKeyData&& other);

    ~BindingKeyData();

    std::vector<uint8_t> wrapped_key;
    std::unique_ptr<unexportable_keys::UnexportableKeyLoader> key_loader;
  };

  void SignAssertionToken(
      std::string_view challenge,
      std::string_view ephemeral_public_key,
      const GURL& destination_url,
      GenerateAssertionCallback callback,
      unexportable_keys::ServiceErrorOr<
          unexportable_keys::UnexportableSigningKeyId> binding_key);

  void OnGetAllKeysForGarbageCollection(
      absl::flat_hash_set<std::vector<uint8_t>> known_wrapped_keys_in_db,
      unexportable_keys::ServiceErrorOr<
          std::vector<unexportable_keys::UnexportableKeyId>>
          all_key_ids_or_error);

  const raw_ref<unexportable_keys::UnexportableKeyService>
      unexportable_key_service_;
  SaveBindingKeyCallback save_binding_key_callback_;

  base::flat_map<CoreAccountId, BindingKeyData> binding_keys_;
  std::unique_ptr<signin::BindingKeyRegistrationTokenHelper>
      registration_token_helper_;
  base::flat_map<CoreAccountId, std::unique_ptr<signin::OAuth2UpgradeTokenFlow>>
      upgrade_flows_;

  base::WeakPtrFactory<TokenBindingHelper> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_TOKEN_BINDING_HELPER_H_
