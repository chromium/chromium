// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/browser_binding/fake_browser_bound_key.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace payments {

FakeBrowserBoundKey::FakeBrowserBoundKey(
    std::vector<uint8_t> identifier,
    std::vector<uint8_t> public_key_as_cose_key,
    std::vector<uint8_t> signature,
    int32_t algorithm_identifier,
    std::vector<uint8_t> expected_client_data,
    bool is_new)
    : identifier_(std::move(identifier)),
      public_key_as_cose_key_(std::move(public_key_as_cose_key)),
      signature_(std::move(signature)),
      algorithm_identifier_(algorithm_identifier),
      expected_client_data_(std::move(expected_client_data)),
      is_new_(is_new) {}

FakeBrowserBoundKey::FakeBrowserBoundKey(const FakeBrowserBoundKey& other)
    : identifier_(other.identifier_),
      public_key_as_cose_key_(other.public_key_as_cose_key_),
      signature_(other.signature_),
      algorithm_identifier_(other.algorithm_identifier_),
      expected_client_data_(other.expected_client_data_),
      is_new_(other.is_new_) {}

FakeBrowserBoundKey& FakeBrowserBoundKey::operator=(
    const FakeBrowserBoundKey& other) {
  identifier_ = other.identifier_;
  public_key_as_cose_key_ = other.public_key_as_cose_key_;
  signature_ = other.signature_;
  expected_client_data_ = other.expected_client_data_;
  is_new_ = other.is_new_;
  return *this;
}

FakeBrowserBoundKey::~FakeBrowserBoundKey() = default;

std::vector<uint8_t> FakeBrowserBoundKey::GetIdentifier() const {
  return identifier_;
}

std::vector<uint8_t> FakeBrowserBoundKey::Sign(
    const std::vector<uint8_t>& client_data) {
  if (client_data == expected_client_data_) {
    return signature_;
  }
  return {};
}

std::vector<uint8_t> FakeBrowserBoundKey::GetPublicKeyAsCoseKey() const {
  return public_key_as_cose_key_;
}

}  // namespace payments
