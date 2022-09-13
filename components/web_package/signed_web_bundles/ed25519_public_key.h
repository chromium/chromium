// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_ED25519_PUBLIC_KEY_H_
#define COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_ED25519_PUBLIC_KEY_H_

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/types/expected.h"

namespace web_package {

// This class wraps an Ed25519 public key. New instances must be created via the
// static `Create` function, which will validate the length of the key before
// creating a new instance. This guarantees that an instance of this class
// always contains a public key of the correct length. This makes the key safe
// to use with functions like BoringSSL's `ED25519_sign`. Note that the public
// key might still be invalid, even though it has the correct length. This will
// be checked and caught by BoringSSL when trying to use the key.
class Ed25519PublicKey {
 public:
  static constexpr size_t kLength = 32;

  // Attempts to parse the bytes as an Ed25519 public key. Returns an instance
  // of this class on success, and an error message on failure.
  static base::expected<Ed25519PublicKey, std::string> Create(
      base::span<const uint8_t> key);

  // Constructs an instance of this class from the provided bytes.
  static Ed25519PublicKey Create(base::span<const uint8_t, kLength> key);

  Ed25519PublicKey(const Ed25519PublicKey&);

  ~Ed25519PublicKey();

  const std::array<uint8_t, kLength>& bytes() const { return bytes_; }

 private:
  explicit Ed25519PublicKey(std::array<uint8_t, kLength> bytes);

  const std::array<uint8_t, kLength> bytes_;
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_ED25519_PUBLIC_KEY_H_
