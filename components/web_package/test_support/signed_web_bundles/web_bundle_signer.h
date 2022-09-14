// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_TEST_SUPPORT_SIGNED_WEB_BUNDLES_WEB_BUNDLE_SIGNER_H_
#define COMPONENTS_WEB_PACKAGE_TEST_SUPPORT_SIGNED_WEB_BUNDLES_WEB_BUNDLE_SIGNER_H_

#include <vector>

#include "base/containers/span.h"
#include "components/cbor/values.h"

namespace web_package {

// A class for use in tests to create signed web bundles. It can also be used to
// create wrongly signed bundles by passing `ErrorForTesting` != `kNoError`.
// Since this class is only intended for use in tests, error handling is
// implemented in the form of CHECKs. Use this class in conjunction with the
// `WebBundleBuilder` class to create signed web bundles.
class WebBundleSigner {
 public:
  enum class ErrorForTesting {
    kNoError,
    kInvalidSignatureLength,
    kInvalidPublicKeyLength,
    kWrongSignatureStackEntryAttributeName,
    kNoPublicKeySignatureStackEntryAttribute,
    kAdditionalSignatureStackEntryAttribute,
    kAdditionalSignatureStackEntryElement,
  };

  struct KeyPair {
    static KeyPair CreateRandom();

    KeyPair(base::span<const uint8_t> public_key,
            base::span<const uint8_t> private_key);
    KeyPair(const KeyPair& other);
    ~KeyPair();

    std::vector<uint8_t> public_key;
    std::vector<uint8_t> private_key;
  };

  // Creates an integrity block with the given signature stack entries.
  static cbor::Value CreateIntegrityBlock(
      const cbor::Value::ArrayValue& signature_stack);

  // Signs an unsigned bundle with the given key pairs, in order. I.e. the first
  // key pair will sign the unsigned bundle, the second key pair will sign the
  // bundle signed with the first key pair, and so on.
  static std::vector<uint8_t> SignBundle(
      base::span<const uint8_t> unsigned_bundle,
      const std::vector<KeyPair>& key_pairs,
      ErrorForTesting error_for_testing = ErrorForTesting::kNoError);

  // Creates a signature stack entry for the given public key and signature.
  static cbor::Value CreateSignatureStackEntry(
      base::span<const uint8_t> public_key,
      std::vector<uint8_t> signature,
      ErrorForTesting error_for_testing = ErrorForTesting::kNoError);

 private:
  static cbor::Value CreateSignatureStackEntryAttributes(
      std::vector<uint8_t> public_key,
      ErrorForTesting error_for_testing = ErrorForTesting::kNoError);

  static cbor::Value CreateIntegrityBlockForBundle(
      base::span<const uint8_t> unsigned_bundle,
      const std::vector<KeyPair>& key_pairs,
      ErrorForTesting error_for_testing = ErrorForTesting::kNoError);
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_TEST_SUPPORT_SIGNED_WEB_BUNDLES_WEB_BUNDLE_SIGNER_H_
