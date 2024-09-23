// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_INTEGRITY_BLOCK_DATA_MATCHER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_INTEGRITY_BLOCK_DATA_MATCHER_H_

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_integrity_block_data.h"
#include "components/web_package/signed_web_bundles/ecdsa_p256_public_key.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_stack_entry.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace web_app::test {

namespace internal {

inline auto SignatureInfoMatcher(
    const web_package::Ed25519PublicKey& public_key) {
  using Ed25519Info = web_package::SignedWebBundleSignatureInfoEd25519;
  return VariantWith<Ed25519Info>(
      testing::Property(&Ed25519Info::public_key, testing::Eq(public_key)));
}

inline auto SignatureInfoMatcher(
    const web_package::EcdsaP256PublicKey& public_key) {
  using EcdsaP256SHA256Info =
      web_package::SignedWebBundleSignatureInfoEcdsaP256SHA256;
  return testing::VariantWith<EcdsaP256SHA256Info>(testing::Property(
      &EcdsaP256SHA256Info::public_key, testing::Eq(public_key)));
}

}  // namespace internal

inline auto IntegrityBlockDataPublicKeysAre(const auto&... public_key) {
  return testing::Optional(testing::Property(
      &IsolatedWebAppIntegrityBlockData::signatures,
      testing::ElementsAre(internal::SignatureInfoMatcher(public_key)...)));
}

}  // namespace web_app::test

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_INTEGRITY_BLOCK_DATA_MATCHER_H_
