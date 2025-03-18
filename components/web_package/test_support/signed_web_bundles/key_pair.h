// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_TEST_SUPPORT_SIGNED_WEB_BUNDLES_KEY_PAIR_H_
#define COMPONENTS_WEB_PACKAGE_TEST_SUPPORT_SIGNED_WEB_BUNDLES_KEY_PAIR_H_

#include <variant>

#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/test_support/signed_web_bundles/ecdsa_p256_key_pair.h"
#include "components/web_package/test_support/signed_web_bundles/ed25519_key_pair.h"

namespace web_package::test {

using KeyPair = std::variant<Ed25519KeyPair, EcdsaP256KeyPair>;
using KeyPairs = std::vector<KeyPair>;

// Hardcoded example values for testing
Ed25519KeyPair GetDefaultEd25519KeyPair();
SignedWebBundleId GetDefaultEd25519WebBundleId();

EcdsaP256KeyPair GetDefaultEcdsaP256KeyPair();
SignedWebBundleId GetDefaultEcdsaP256WebBundleId();

}  // namespace web_package::test

#endif  // COMPONENTS_WEB_PACKAGE_TEST_SUPPORT_SIGNED_WEB_BUNDLES_KEY_PAIR_H_
