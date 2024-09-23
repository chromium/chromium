// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/token_binding_helper.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/types/expected.h"
#include "components/signin/public/base/hybrid_encryption_key.h"
#include "components/signin/public/base/session_binding_utils.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_loader.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "crypto/signature_verifier.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_urls.h"
#include "url/gurl.h"

namespace {

constexpr std::string_view kTokenBindingNamespace = "TokenBinding";

unexportable_keys::BackgroundTaskPriority kTokenBindingPriority =
    unexportable_keys::BackgroundTaskPriority::kBestEffort;

base::expected<std::string, TokenBindingHelper::Error> CreateAssertionToken(
    const std::string& header_and_payload,
    crypto::SignatureVerifier::SignatureAlgorithm algorithm,
    unexportable_keys::ServiceErrorOr<std::vector<uint8_t>> signature) {
  if (!signature.has_value()) {
    return base::unexpected(TokenBindingHelper::Error::kSignAssertionFailure);
  }

  std::optional<std::string> signed_assertion =
      signin::AppendSignatureToHeaderAndPayload(header_and_payload, algorithm,
                                                *signature);
  if (!signed_assertion.has_value()) {
    return base::unexpected(TokenBindingHelper::Error::kAppendSignatureFailure);
  }

  return *signed_assertion;
}

// A helper to record a histogram value before running `callback`.
// Also reorders callback parameters for chaining with `CreateAssertionToken()`.
void RunCallbackAndRecordMetrics(
    TokenBindingHelper::GenerateAssertionCallback callback,
    std::optional<HybridEncryptionKey> ephemeral_key,
    base::expected<std::string, TokenBindingHelper::Error>
        assertion_token_or_error) {
  base::UmaHistogramEnumeration("Signin.TokenBinding.GenerateAssertionResult",
                                assertion_token_or_error.error_or(
                                    TokenBindingHelper::kNoErrorForMetrics));
  std::move(callback).Run(
      std::move(assertion_token_or_error).value_or(std::string()),
      std::move(ephemeral_key));
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
    binding_keys_.erase(account_id);
    return;
  }

  binding_keys_.insert_or_assign(
      account_id, BindingKeyData(std::vector<uint8_t>(
                      wrapped_binding_key.begin(), wrapped_binding_key.end())));
}

bool TokenBindingHelper::HasBindingKey(const CoreAccountId& account_id) const {
  return base::Contains(binding_keys_, account_id);
}

void TokenBindingHelper::ClearAllKeys() {
  binding_keys_.clear();
}

void TokenBindingHelper::GenerateBindingKeyAssertion(
    const CoreAccountId& account_id,
    std::string_view challenge,
    const GURL& destination_url,
    GenerateAssertionCallback callback) {
  CHECK(callback);
  auto it = binding_keys_.find(account_id);
  if (it == binding_keys_.end()) {
    RunCallbackAndRecordMetrics(std::move(callback), std::nullopt,
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
      std::string(challenge), destination_url, std::move(callback)));
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
    const GURL& destination_url,
    GenerateAssertionCallback callback,
    unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>
        binding_key) {
  if (!binding_key.has_value()) {
    RunCallbackAndRecordMetrics(std::move(callback), std::nullopt,
                                base::unexpected(Error::kLoadKeyFailure));
    return;
  }

  HybridEncryptionKey ephemeral_key;
  crypto::SignatureVerifier::SignatureAlgorithm algorithm =
      *unexportable_key_service_->GetAlgorithm(*binding_key);
  std::optional<std::string> header_and_payload =
      signin::CreateKeyAssertionHeaderAndPayload(
          algorithm,
          *unexportable_key_service_->GetSubjectPublicKeyInfo(*binding_key),
          GaiaUrls::GetInstance()->oauth2_chrome_client_id(), challenge,
          destination_url, kTokenBindingNamespace, &ephemeral_key);

  if (!header_and_payload.has_value()) {
    RunCallbackAndRecordMetrics(
        std::move(callback), std::nullopt,
        base::unexpected(Error::kCreateAssertionFaiure));
    return;
  }

  unexportable_key_service_->SignSlowlyAsync(
      *binding_key, base::as_bytes(base::make_span(*header_and_payload)),
      kTokenBindingPriority,
      base::BindOnce(&CreateAssertionToken, *header_and_payload, algorithm)
          .Then(base::BindOnce(&RunCallbackAndRecordMetrics,
                               std::move(callback), std::move(ephemeral_key))));
}
