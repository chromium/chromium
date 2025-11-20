// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/scoped_mock_unexportable_key_provider.h"

#include <memory>
#include <optional>

#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "components/unexportable_keys/mock_unexportable_key.h"
#include "crypto/unexportable_key.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace unexportable_keys {

namespace {

ScopedMockUnexportableKeyProvider* g_mock_provider = nullptr;

class ForwardingUnexportableKeyProvider
    : public crypto::UnexportableKeyProvider {
 public:
  explicit ForwardingUnexportableKeyProvider(
      crypto::UnexportableKeyProvider& provider)
      : provider_(provider) {}
  ~ForwardingUnexportableKeyProvider() override = default;

  // crypto::UnexportableKeyProvider:
  std::optional<crypto::SignatureVerifier::SignatureAlgorithm> SelectAlgorithm(
      base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms) override {
    return provider_->SelectAlgorithm(acceptable_algorithms);
  }

  std::unique_ptr<crypto::UnexportableSigningKey> GenerateSigningKeySlowly(
      base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms) override {
    return provider_->GenerateSigningKeySlowly(acceptable_algorithms);
  }

  std::unique_ptr<crypto::UnexportableSigningKey> FromWrappedSigningKeySlowly(
      base::span<const uint8_t> wrapped_key) override {
    return provider_->FromWrappedSigningKeySlowly(wrapped_key);
  }

  crypto::StatefulUnexportableKeyProvider* AsStatefulUnexportableKeyProvider()
      override {
    return provider_->AsStatefulUnexportableKeyProvider();
  }

 private:
  const raw_ref<crypto::UnexportableKeyProvider> provider_;
};

std::unique_ptr<crypto::UnexportableKeyProvider> GetMockKeyProvider() {
  return std::make_unique<ForwardingUnexportableKeyProvider>(
      g_mock_provider->mock());
}

std::unique_ptr<crypto::UnexportableSigningKey> GetNextGeneratedKey(
    base::queue<std::unique_ptr<crypto::UnexportableSigningKey>>&
        next_generated_keys) {
  std::unique_ptr<crypto::UnexportableSigningKey> next_generated_key;
  if (!next_generated_keys.empty()) {
    next_generated_key = std::move(next_generated_keys.front());
    next_generated_keys.pop();
  }
  return next_generated_key;
}

}  // namespace

ScopedMockUnexportableKeyProvider::ScopedMockUnexportableKeyProvider() {
  CHECK(!g_mock_provider) << "Nested providers are not allowed";
  // Store `this` in a global pointer so that all mock key providers can access
  // the `next_generated_keys_` queue.
  g_mock_provider = this;
  crypto::internal::SetUnexportableKeyProviderForTesting(&GetMockKeyProvider);

  ON_CALL(mock_provider_, SelectAlgorithm)
      .WillByDefault(
          [](base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
                 algorithms) {
            return algorithms.empty() ? std::nullopt
                                      : std::optional(algorithms[0]);
          });
  ON_CALL(mock_provider_, GenerateSigningKeySlowly).WillByDefault([this](auto) {
    return GetNextGeneratedKey(next_generated_keys_);
  });
  ON_CALL(mock_provider_, FromWrappedSigningKeySlowly)
      .WillByDefault(
          [this](auto) { return GetNextGeneratedKey(next_generated_keys_); });
}

ScopedMockUnexportableKeyProvider::~ScopedMockUnexportableKeyProvider() {
  crypto::internal::SetUnexportableKeyProviderForTesting(nullptr);
  g_mock_provider = nullptr;
}

void ScopedMockUnexportableKeyProvider::AddNextGeneratedKey(
    std::unique_ptr<crypto::UnexportableSigningKey> key) {
  next_generated_keys_.push(std::move(key));
}

}  // namespace unexportable_keys
