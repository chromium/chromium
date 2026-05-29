// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/token_binding_helper.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/process/process.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "components/signin/internal/identity_manager/oauth2_upgrade_token_flow.h"
#include "components/signin/public/base/binding_key_registration_token_helper.h"
#include "components/signin/public/base/binding_key_registration_token_result.h"
#include "components/signin/public/base/session_binding_utils.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_loader.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "crypto/signature_verifier.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "url/gurl.h"

namespace {

constexpr std::string_view kTokenBindingNamespace = "TokenBinding";

constexpr unexportable_keys::BackgroundTaskPriority kTokenBindingPriority =
    unexportable_keys::BackgroundTaskPriority::kBestEffort;

base::expected<std::string, TokenBindingHelper::Error> CreateAssertionToken(
    const std::string& header_and_payload,
    crypto::SignatureVerifier::SignatureAlgorithm algorithm,
    const std::vector<uint8_t>& pubkey,
    unexportable_keys::ServiceErrorOr<std::vector<uint8_t>> signature) {
  if (!signature.has_value()) {
    return base::unexpected(TokenBindingHelper::Error::kSignAssertionFailure);
  }

  std::optional<std::string> signed_assertion =
      signin::AppendSignatureToHeaderAndPayload(header_and_payload, algorithm,
                                                pubkey, *signature);
  if (!signed_assertion.has_value()) {
    return base::unexpected(TokenBindingHelper::Error::kAppendSignatureFailure);
  }

  return *signed_assertion;
}

// A helper to record a histogram value before running `callback`.
void RunCallbackAndRecordMetrics(
    TokenBindingHelper::GenerateAssertionCallback callback,
    base::expected<std::string, TokenBindingHelper::Error>
        assertion_token_or_error) {
  base::UmaHistogramEnumeration("Signin.TokenBinding.GenerateAssertionResult",
                                assertion_token_or_error.error_or(
                                    TokenBindingHelper::kNoErrorForMetrics));
  std::move(callback).Run(
      std::move(assertion_token_or_error).value_or(std::string()));
}

}  // namespace

TokenBindingHelper::TokenBindingHelper(
    unexportable_keys::UnexportableKeyService& unexportable_key_service)
    : unexportable_key_service_(unexportable_key_service) {}

TokenBindingHelper::~TokenBindingHelper() = default;

void TokenBindingHelper::SetBindingKey(
    const CoreAccountId& account_id,
    base::span<const uint8_t> wrapped_binding_key) {
  if (wrapped_binding_key.empty()) {
    // No need in storing an empty key, just remove the entry if any.
    auto it = binding_keys_.find(account_id);
    if (it == binding_keys_.end()) {
      return;
    }

    binding_keys_.erase(it);
    if (binding_keys_.empty()) {
      // Make sure that any new binding key registration starts using a new key
      // after the latest binding key is removed.
      registration_token_helper_.reset();
    }
    return;
  }

  binding_keys_.insert_or_assign(
      account_id, BindingKeyData(std::vector<uint8_t>(
                      wrapped_binding_key.begin(), wrapped_binding_key.end())));
}

bool TokenBindingHelper::HasBindingKey(const CoreAccountId& account_id) const {
  return binding_keys_.contains(account_id);
}

void TokenBindingHelper::ClearAllKeys() {
  binding_keys_.clear();
  registration_token_helper_.reset();
}

void TokenBindingHelper::MaybeInitializeRegistrationTokenHelper(
    base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
        supported_algorithms) {
  if (registration_token_helper_) {
    return;
  }
  std::vector<uint8_t> wrapped_binding_key_to_reuse;
  if (!binding_keys_.empty()) {
    // All bound tokens are supposed to use the same key, so we're taking an
    // arbitrary key.
    wrapped_binding_key_to_reuse = binding_keys_.begin()->second.wrapped_key;
  }
  if (!wrapped_binding_key_to_reuse.empty()) {
    // Ignore the value of `supported_algorithms` in favor of an existing
    // binding key.
    registration_token_helper_ =
        std::make_unique<signin::BindingKeyRegistrationTokenHelper>(
            *unexportable_key_service_,
            std::move(wrapped_binding_key_to_reuse));
  } else {
    registration_token_helper_ =
        std::make_unique<signin::BindingKeyRegistrationTokenHelper>(
            *unexportable_key_service_, base::ToVector(supported_algorithms));
  }
}

void TokenBindingHelper::OnAllCredentialsLoaded(bool has_refresh_tokens) {
  if (!has_refresh_tokens) {
    return;
  }

  MaybeInitializeRegistrationTokenHelper({
      crypto::SignatureVerifier::ECDSA_SHA256,
      crypto::SignatureVerifier::RSA_PKCS1_SHA256,
  });
  CHECK(registration_token_helper_);
  registration_token_helper_->CreateKeyLoaderIfNeeded();
}

bool TokenBindingHelper::IsRegistrationKeyReady() const {
  return registration_token_helper_ &&
         registration_token_helper_->IsRegistrationKeyReady();
}

void TokenBindingHelper::GenerateBindingKeyRegistrationToken(
    base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
        supported_algorithms,
    std::string_view auth_code,
    base::OnceCallback<void(
        std::optional<signin::BindingKeyRegistrationTokenResult>)> callback) {
  MaybeInitializeRegistrationTokenHelper(supported_algorithms);
  CHECK(registration_token_helper_);

  registration_token_helper_->GenerateForTokenBinding(
      GaiaUrls::GetInstance()->oauth2_chrome_client_id(), auth_code,
      GURL("https://accounts.google.com/accountmanager"), std::move(callback));
}

void TokenBindingHelper::GenerateBindingKeyAssertion(
    const CoreAccountId& account_id,
    std::string_view challenge,
    std::string_view ephemeral_public_key,
    const GURL& destination_url,
    GenerateAssertionCallback callback) {
  CHECK(callback);
  auto it = binding_keys_.find(account_id);
  if (it == binding_keys_.end()) {
    RunCallbackAndRecordMetrics(std::move(callback),
                                base::unexpected(Error::kKeyNotFound));
    return;
  }

  auto& binding_key_data = it->second;
  if (!binding_key_data.key_loader) {
    binding_key_data.key_loader =
        unexportable_keys::UnexportableKeyLoader::CreateFromWrappedKey(
            *unexportable_key_service_, binding_key_data.wrapped_key,
            kTokenBindingPriority);
  }

  // `base::Unretained(this)` is safe because `this` owns the
  // `UnexportableKeyLoader`.
  binding_key_data.key_loader->InvokeCallbackAfterKeyLoaded(base::BindOnce(
      &TokenBindingHelper::SignAssertionToken, base::Unretained(this),
      std::string(challenge), std::string(ephemeral_public_key),
      destination_url, std::move(callback)));
}

void TokenBindingHelper::StartGarbageCollection(
    absl::flat_hash_set<std::vector<uint8_t>> known_wrapped_keys_in_db) {
  unexportable_key_service_->GetAllKeysForGarbageCollectionSlowlyAsync(
      unexportable_keys::BackgroundTaskPriority::kBestEffort,
      base::BindOnce(&TokenBindingHelper::OnGetAllKeysForGarbageCollection,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(known_wrapped_keys_in_db)));
}

std::vector<uint8_t> TokenBindingHelper::GetWrappedBindingKey(
    const CoreAccountId& account_id) const {
  auto it = binding_keys_.find(account_id);
  if (it == binding_keys_.end()) {
    return {};
  }

  return it->second.wrapped_key;
}

size_t TokenBindingHelper::GetBoundTokenCount() const {
  return binding_keys_.size();
}

bool TokenBindingHelper::AreAllBindingKeysSame() const {
  return std::ranges::all_of(binding_keys_, [this](const auto& kv_pair) {
    return kv_pair.second.wrapped_key ==
           binding_keys_.begin()->second.wrapped_key;
  });
}

void TokenBindingHelper::CopyBindingKeyFromAnotherTokenService(
    base::span<const uint8_t> wrapped_binding_key) {
  CHECK(!wrapped_binding_key.empty());
  // This will force a load of the `wrapped_binding_key` into the
  // `unexportable_key_service_`. In stateful implementations like on macOS,
  // this will furthermore ensure that the key representation on disk will be
  // duplicated with metadata corresponding to `unexportable_key_service_`. This
  // in turn will ensure that this key will not be deleted by garbage collection
  // if the source token service no longer needs this key, but
  // `unexportable_key_service_` still does.
  unexportable_key_service_->FromWrappedSigningKeySlowlyAsync(
      // TODO(crbug.com/455538352): Implement metrics.
      wrapped_binding_key, kTokenBindingPriority, base::DoNothing());
}

void TokenBindingHelper::PerformTokenBindingUpgrade(
    const CoreAccountId& account_id,
    std::string_view refresh_token,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::string_view device_id,
    std::string_view challenge,
    base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
        supported_algorithms) {
  CHECK(base::FeatureList::IsEnabled(
      switches::kEnableChromeRefreshTokenBindingUpgrade));
  CHECK_NE(refresh_token, GaiaConstants::kInvalidRefreshToken);
  std::unique_ptr<signin::OAuth2UpgradeTokenFlow>& upgrade_flow =
      upgrade_flows_[account_id];
  if (upgrade_flow != nullptr) {
    // Do nothing if an upgrade is already in progress for this account.
    return;
  }

  // `base::Unretained()` is safe because `this` owns `upgrade_flow`.
  upgrade_flow = std::make_unique<signin::OAuth2UpgradeTokenFlow>(
      std::string(refresh_token),
      switches::kRefreshTokenBindingUpgradeType.Get(), std::string(device_id),
      std::move(url_loader_factory),
      base::BindOnce(&TokenBindingHelper::OnUpgradeTokenFinished,
                     base::Unretained(this), account_id));

  // `base::Unretained()` is safe because `this` owns
  // `registration_token_helper_`.
  GenerateBindingKeyRegistrationToken(
      supported_algorithms, challenge,
      base::BindOnce(&TokenBindingHelper::OnUpgradeRegistrationTokenGenerated,
                     base::Unretained(this), account_id));
}

void TokenBindingHelper::SetSaveBindingKeyCallback(
    SaveBindingKeyCallback callback) {
  CHECK(!save_binding_key_callback_);
  CHECK(callback);
  save_binding_key_callback_ = std::move(callback);
}

void TokenBindingHelper::OnUpgradeRegistrationTokenGenerated(
    const CoreAccountId& account_id,
    std::optional<signin::BindingKeyRegistrationTokenResult> result) {
  CHECK(base::FeatureList::IsEnabled(
      switches::kEnableChromeRefreshTokenBindingUpgrade));

  auto it = upgrade_flows_.find(account_id);
  CHECK(it != upgrade_flows_.end());
  std::unique_ptr<signin::OAuth2UpgradeTokenFlow>& upgrade_flow = it->second;

  if (!result.has_value()) {
    upgrade_flow->AbortWithError(
        signin::OAuth2UpgradeTokenFlowResult::kTokenGenerationFailure);
    return;
  }

  CHECK(save_binding_key_callback_);
  SaveBindingKeyResult save_key_result =
      save_binding_key_callback_.Run(account_id, upgrade_flow->refresh_token(),
                                     std::move(result->wrapped_binding_key));
  base::UmaHistogramEnumeration(
      "Signin.TokenBinding.UpgradeSaveBindingKeyResult", save_key_result);
  if (save_key_result != SaveBindingKeyResult::kSuccess) {
    upgrade_flow->AbortWithError(
        signin::OAuth2UpgradeTokenFlowResult::kFailedToSaveBindingKey);
    return;
  }

  upgrade_flow->StartWithRegistrationToken(
      std::move(result->registration_token));
}

void TokenBindingHelper::OnUpgradeTokenFinished(
    const CoreAccountId& account_id) {
  CHECK(base::FeatureList::IsEnabled(
      switches::kEnableChromeRefreshTokenBindingUpgrade));

  size_t removed_count = upgrade_flows_.erase(account_id);
  CHECK_EQ(removed_count, 1U);
}

base::WeakPtr<TokenBindingHelper> TokenBindingHelper::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

TokenBindingHelper::BindingKeyData::BindingKeyData(
    std::vector<uint8_t> in_wrapped_key)
    : wrapped_key(in_wrapped_key) {}
TokenBindingHelper::BindingKeyData::BindingKeyData(BindingKeyData&& other) =
    default;
TokenBindingHelper::BindingKeyData&
TokenBindingHelper::BindingKeyData::operator=(BindingKeyData&& other) = default;
TokenBindingHelper::BindingKeyData::~BindingKeyData() = default;

void TokenBindingHelper::SignAssertionToken(
    std::string_view challenge,
    std::string_view ephemeral_public_key,
    const GURL& destination_url,
    GenerateAssertionCallback callback,
    unexportable_keys::ServiceErrorOr<
        unexportable_keys::UnexportableSigningKeyId> binding_key) {
  if (!binding_key.has_value()) {
    RunCallbackAndRecordMetrics(std::move(callback),
                                base::unexpected(Error::kLoadKeyFailure));
    return;
  }

  crypto::SignatureVerifier::SignatureAlgorithm algorithm =
      *unexportable_key_service_->GetAlgorithm(*binding_key);
  std::vector<uint8_t> pubkey =
      *unexportable_key_service_->GetSubjectPublicKeyInfo(*binding_key);
  std::optional<std::string> header_and_payload =
      signin::CreateKeyAssertionHeaderAndPayload(
          algorithm, pubkey, GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
          challenge, destination_url, kTokenBindingNamespace,
          ephemeral_public_key);

  if (!header_and_payload.has_value()) {
    RunCallbackAndRecordMetrics(
        std::move(callback), base::unexpected(Error::kCreateAssertionFaiure));
    return;
  }

  unexportable_key_service_->SignSlowlyAsync(
      *binding_key, base::as_byte_span(*header_and_payload),
      kTokenBindingPriority,
      base::BindOnce(&CreateAssertionToken, *header_and_payload, algorithm,
                     std::move(pubkey))
          .Then(base::BindOnce(&RunCallbackAndRecordMetrics,
                               std::move(callback))));
}

void TokenBindingHelper::OnGetAllKeysForGarbageCollection(
    absl::flat_hash_set<std::vector<uint8_t>> known_wrapped_keys_in_db,
    unexportable_keys::ServiceErrorOr<
        std::vector<unexportable_keys::UnexportableKeyId>>
        all_key_ids_or_error) {
  if (!all_key_ids_or_error.has_value() || all_key_ids_or_error->empty()) {
    return;
  }

  std::vector<unexportable_keys::UnexportableKeyId>& all_key_ids =
      *all_key_ids_or_error;

  static constexpr std::string_view kGarbageCollectionHistogramPrefix =
      "Crypto.UnexportableKeys.GarbageCollection.RefreshTokenBinding.";

  const size_t key_count = all_key_ids.size();
  base::UmaHistogramCounts100(
      base::StrCat({kGarbageCollectionHistogramPrefix, "TotalKeyCount"}),
      key_count);

  // Construct a set of all wrapped keys that are still used.
  absl::flat_hash_set<std::vector<uint8_t>> known_wrapped_keys =
      std::move(known_wrapped_keys_in_db);

  for (const auto& [_, binding_key_data] : binding_keys_) {
    known_wrapped_keys.insert(binding_key_data.wrapped_key);
  }

  // Filter out keys from the response that are still used or were generated
  // after the current Chrome session started.
  std::erase_if(all_key_ids, [&](unexportable_keys::UnexportableKeyId key_id) {
    unexportable_keys::ServiceErrorOr<std::vector<uint8_t>> wrapped_key =
        unexportable_key_service_->GetWrappedKey(key_id);
    return !wrapped_key.has_value() ||
           known_wrapped_keys.contains(*wrapped_key) ||
           unexportable_key_service_->GetCreationTime(key_id).value_or(
               base::Time::Now()) >= base::Process::Current().CreationTime();
  });

  base::UmaHistogramCounts100(
      base::StrCat({kGarbageCollectionHistogramPrefix, "UsedKeyCount"}),
      key_count - all_key_ids.size());

  base::UmaHistogramCounts100(
      base::StrCat({kGarbageCollectionHistogramPrefix, "ObsoleteKeyCount"}),
      all_key_ids.size());

  unexportable_key_service_->DeleteKeysSlowlyAsync(
      all_key_ids, unexportable_keys::BackgroundTaskPriority::kBestEffort,
      base::BindOnce([](unexportable_keys::ServiceErrorOr<size_t> result) {
        base::UmaHistogramCounts100(
            base::StrCat({kGarbageCollectionHistogramPrefix,
                          "ObsoleteKeyDeletionCount"}),
            result.value_or(0));
      }));
}
