// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/scoped_mock_unexportable_key_provider.h"

#include <memory>
#include <optional>

#include "base/containers/span.h"
#include "components/unexportable_keys/mock_unexportable_key.h"
#include "crypto/unexportable_key.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace unexportable_keys {

namespace {

ScopedMockUnexportableKeyProvider* g_mock_provider = nullptr;

class MockUnexportableKeyProvider : public crypto::UnexportableKeyProvider {
 public:
  MockUnexportableKeyProvider();
  ~MockUnexportableKeyProvider() override;

  // crypto::UnexportableKeyProvider:
  std::optional<crypto::SignatureVerifier::SignatureAlgorithm> SelectAlgorithm(
      base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms) override;
  std::unique_ptr<crypto::UnexportableSigningKey> GenerateSigningKeySlowly(
      base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms) override;
  std::unique_ptr<crypto::UnexportableSigningKey> FromWrappedSigningKeySlowly(
      base::span<const uint8_t> wrapped_key) override;
  bool DeleteSigningKeySlowly(base::span<const uint8_t> wrapped_key) override;
};

MockUnexportableKeyProvider::MockUnexportableKeyProvider() = default;
MockUnexportableKeyProvider::~MockUnexportableKeyProvider() = default;

std::optional<crypto::SignatureVerifier::SignatureAlgorithm>
MockUnexportableKeyProvider::SelectAlgorithm(
    base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
        acceptable_algorithms) {
  if (acceptable_algorithms.empty()) {
    return std::nullopt;
  }
  return acceptable_algorithms.front();
}

std::unique_ptr<crypto::UnexportableSigningKey>
MockUnexportableKeyProvider::GenerateSigningKeySlowly(
    base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
        acceptable_algorithms) {
  return g_mock_provider->GetNextGeneratedKey();
}

std::unique_ptr<crypto::UnexportableSigningKey>
MockUnexportableKeyProvider::FromWrappedSigningKeySlowly(
    base::span<const uint8_t> wrapped_key) {
  return g_mock_provider->GetNextGeneratedKey();
}

bool MockUnexportableKeyProvider::DeleteSigningKeySlowly(
    base::span<const uint8_t> wrapped_key) {
  return true;
}

std::unique_ptr<crypto::UnexportableKeyProvider> GetMockKeyProvider() {
  return std::make_unique<MockUnexportableKeyProvider>();
}

}  // namespace

ScopedMockUnexportableKeyProvider::ScopedMockUnexportableKeyProvider() {
  CHECK(!g_mock_provider) << "Nested providers are not allowed";
  // Store `this` in a global pointer so that all mock key providers can access
  // the `next_generated_keys_` queue.
  g_mock_provider = this;
  crypto::internal::SetUnexportableKeyProviderForTesting(&GetMockKeyProvider);
}

ScopedMockUnexportableKeyProvider::~ScopedMockUnexportableKeyProvider() {
  crypto::internal::SetUnexportableKeyProviderForTesting(nullptr);
  g_mock_provider = nullptr;
}

void ScopedMockUnexportableKeyProvider::AddNextGeneratedKey(
    std::unique_ptr<crypto::UnexportableSigningKey> key) {
  next_generated_keys_.push(std::move(key));
}

std::unique_ptr<crypto::UnexportableSigningKey>
ScopedMockUnexportableKeyProvider::GetNextGeneratedKey() {
  std::unique_ptr<crypto::UnexportableSigningKey> next_generated_key;
  if (!next_generated_keys_.empty()) {
    next_generated_key = std::move(next_generated_keys_.front());
    next_generated_keys_.pop();
  }
  return next_generated_key;
}

}  // namespace unexportable_keys
