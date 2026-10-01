// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/unexportable_key_utils.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/webauthn/enclave_manager.h"
#include "crypto/apple/fake_keychain_v2.h"
#include "crypto/apple/scoped_fake_keychain_v2.h"
#include "crypto/unexportable_key.h"
#include "device/fido/enclave/constants.h"
#include "device/fido/public/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Parameterized on whether `kWebAuthnSoftwareKeysWhenTpmAbsent` is enabled.
class UnexportableKeyUtilsMacTest : public testing::TestWithParam<bool> {
 public:
  UnexportableKeyUtilsMacTest() {
    scoped_feature_list_.InitWithFeatureState(
        device::kWebAuthnSoftwareKeysWhenTpmAbsent, GetParam());
    ResetUnexportableKeyProviderTypeMetricForTesting();
  }

 protected:
  bool SoftwareKeysFallbackEnabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, UnexportableKeyUtilsMacTest, testing::Bool());

// The hardware provider is used regardless of the flag state when a Secure
// Enclave is available.
TEST_P(UnexportableKeyUtilsMacTest, SecureEnclaveAvailable) {
  crypto::apple::ScopedFakeKeychainV2 fake_keychain(
      EnclaveManager::kEnclaveKeysKeychainAccessGroup);
  fake_keychain.keychain()->set_secure_enclave_available(true);

  base::HistogramTester histogram_tester;
  std::unique_ptr<crypto::UnexportableKeyProvider> provider =
      GetWebAuthnUnexportableKeyProvider();
  ASSERT_TRUE(provider);
  EXPECT_TRUE(provider->AsStatefulUnexportableKeyProvider());
  EXPECT_NE(provider->SelectAlgorithm(device::enclave::kSigningAlgorithms),
            std::nullopt);
  histogram_tester.ExpectUniqueSample(kUnexportableKeyProviderTypeHistogram,
                                      UnexportableKeyProviderType::kHardware,
                                      1);

  // The histogram is only recorded once per process.
  ASSERT_TRUE(GetWebAuthnUnexportableKeyProvider());
  histogram_tester.ExpectUniqueSample(kUnexportableKeyProviderTypeHistogram,
                                      UnexportableKeyProviderType::kHardware,
                                      1);

  std::unique_ptr<crypto::UnexportableSigningKey> key =
      provider->GenerateSigningKeySlowly(device::enclave::kSigningAlgorithms);
  ASSERT_TRUE(key);
  EXPECT_TRUE(key->IsHardwareBacked());
}

TEST_P(UnexportableKeyUtilsMacTest, SecureEnclaveUnavailable) {
  crypto::apple::ScopedFakeKeychainV2 fake_keychain(
      EnclaveManager::kEnclaveKeysKeychainAccessGroup);
  fake_keychain.keychain()->set_secure_enclave_available(false);

  base::HistogramTester histogram_tester;
  std::unique_ptr<crypto::UnexportableKeyProvider> provider =
      GetWebAuthnUnexportableKeyProvider();

  if (!SoftwareKeysFallbackEnabled()) {
    EXPECT_FALSE(provider);
    histogram_tester.ExpectUniqueSample(kUnexportableKeyProviderTypeHistogram,
                                        UnexportableKeyProviderType::kNone, 1);
    return;
  }

  ASSERT_TRUE(provider);
  // Software keys are stateless, unlike Keychain-backed keys.
  EXPECT_FALSE(provider->AsStatefulUnexportableKeyProvider());
  EXPECT_NE(provider->SelectAlgorithm(device::enclave::kSigningAlgorithms),
            std::nullopt);
  histogram_tester.ExpectUniqueSample(
      kUnexportableKeyProviderTypeHistogram,
      UnexportableKeyProviderType::kSoftwareFallback, 1);

  std::unique_ptr<crypto::UnexportableSigningKey> key =
      provider->GenerateSigningKeySlowly(device::enclave::kSigningAlgorithms);
  ASSERT_TRUE(key);
  EXPECT_FALSE(key->IsHardwareBacked());
}

}  // namespace
