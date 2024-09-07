// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_TEST_SUPPORT_SIGNED_WEB_BUNDLES_WEB_BUNDLE_SIGNER_H_
#define COMPONENTS_WEB_PACKAGE_TEST_SUPPORT_SIGNED_WEB_BUNDLES_WEB_BUNDLE_SIGNER_H_

#include <vector>

#include "base/containers/enum_set.h"
#include "base/containers/span.h"
#include "components/cbor/values.h"
#include "components/web_package/signed_web_bundles/ecdsa_p256_public_key.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/test_support/signed_web_bundles/key_pair.h"

namespace web_package::test {

// A class for use in tests to create signed web bundles. It can also be used to
// create wrongly signed bundles by passing `ErrorForTesting` != `kNoError`.
// Since this class is only intended for use in tests, error handling is
// implemented in the form of CHECKs. Use this class in conjunction with the
// `WebBundleBuilder` class to create signed web bundles.
class WebBundleSigner {
 public:
  enum class IntegrityBlockErrorForTesting {
    kMinValue = 0,
    kInvalidIntegrityBlockStructure = kMinValue,
    kInvalidVersion,
    kNoSignedWebBundleId,
    kNoAttributes,
    kEmptySignatureList,
    kMaxValue = kEmptySignatureList
  };

  enum class IntegritySignatureErrorForTesting {
    kMinValue = 0,
    kInvalidSignatureLength = kMinValue,
    kInvalidPublicKeyLength,
    kWrongSignatureStackEntryAttributeName,
    kNoPublicKeySignatureStackEntryAttribute,
    kAdditionalSignatureStackEntryAttributes,
    kAdditionalSignatureStackEntryElement,
    kWrongSignatureStackEntryAttributeNameLength,
    kMultipleValidPublicKeyAttributes,
    kSignatureStackEntryUnsupportedArrayAttribute,
    kSignatureStackEntryUnsupportedMapAttribute,
    kMaxValue = kSignatureStackEntryUnsupportedMapAttribute,
  };

  using IntegritySignatureErrorsForTesting =
      base::EnumSet<IntegritySignatureErrorForTesting,
                    IntegritySignatureErrorForTesting::kMinValue,
                    IntegritySignatureErrorForTesting::kMaxValue>;

  using IntegrityBlockErrorsForTesting =
      base::EnumSet<IntegrityBlockErrorForTesting,
                    IntegrityBlockErrorForTesting::kMinValue,
                    IntegrityBlockErrorForTesting::kMaxValue>;

  struct ErrorsForTesting {
    ErrorsForTesting(IntegrityBlockErrorsForTesting integrity_block_errors,
                     const std::vector<IntegritySignatureErrorsForTesting>&
                         signatures_errors);
    ErrorsForTesting(const ErrorsForTesting& other);
    ErrorsForTesting& operator=(const ErrorsForTesting& other);
    ~ErrorsForTesting();

    IntegrityBlockErrorsForTesting integrity_block_errors;
    std::vector<IntegritySignatureErrorsForTesting> signatures_errors;
  };

  struct IntegrityBlockAttributes {
    std::string web_bundle_id;
  };

  // Signs an unsigned bundle with the given key pairs.
  // Signatures do not depend on each other and co-exist in parallel.
  // If `ib_attributes` is not passed, signed web bundle id will be computed
  // from the first key.
  static std::vector<uint8_t> SignBundle(
      base::span<const uint8_t> unsigned_bundle,
      const KeyPairs& key_pairs,
      std::optional<IntegrityBlockAttributes> ib_attributes = {},
      ErrorsForTesting errors_for_testing = {/*integrity_block_errors=*/{},
                                             /*signatures_errors=*/{}});

  // Signs an unsigned bundle with a given key pair, automatically computing the
  // signed web bundle from it. This is a shortcut to above, more broad
  // function, mainly for convenience.
  static std::vector<uint8_t> SignBundle(
      base::span<const uint8_t> unsigned_bundle,
      const KeyPair& key_pair,
      ErrorsForTesting errors_for_testing = {/*integrity_block_errors=*/{},
                                             /*signatures_errors=*/{}});
};

}  // namespace web_package::test

#endif  // COMPONENTS_WEB_PACKAGE_TEST_SUPPORT_SIGNED_WEB_BUNDLES_WEB_BUNDLE_SIGNER_H_
