// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_TOKEN_BINDING_HELPER_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_TOKEN_BINDING_HELPER_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "components/signin/public/base/hybrid_encryption_key.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"

namespace unexportable_keys {
class UnexportableKeyService;
class UnexportableKeyLoader;
}  // namespace unexportable_keys

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

  using GenerateAssertionCallback =
      base::OnceCallback<void(std::string, std::optional<HybridEncryptionKey>)>;

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

  // Asynchronously generates a binding key assertion with a key associated with
  // `account_id`. The result is returned through `callback`. Returns an empty
  // string if the generation fails.
  void GenerateBindingKeyAssertion(const CoreAccountId& account_id,
                                   std::string_view challenge,
                                   const GURL& destination_url,
                                   GenerateAssertionCallback callback);

  // Returns a wrapped key associated with `account_id`. Returns an empty vector
  // if no key is found.
  std::vector<uint8_t> GetWrappedBindingKey(
      const CoreAccountId& account_id) const;

  // Returns the number of bound tokens.
  size_t GetBoundTokenCount() const;

  // Returns whether all accounts reuse the same binding key.
  // Returns `true` if empty.
  bool AreAllBindingKeysSame() const;

 private:
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
      const GURL& destination_url,
      GenerateAssertionCallback callback,
      unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>
          binding_key);

  const raw_ref<unexportable_keys::UnexportableKeyService>
      unexportable_key_service_;

  base::flat_map<CoreAccountId, BindingKeyData> binding_keys_;
};

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_TOKEN_BINDING_HELPER_H_
