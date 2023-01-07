// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/signature_provider.h"

#include <utility>

#include "crypto/rsa_private_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

// Param: 1-based key version to be tested.
typedef ::testing::TestWithParam<int> SignatureProviderWithValidKeyIndexTest;

void CheckSignatureForDomain(const SignatureProvider::SigningKey* signing_key,
                             const std::string& domain,
                             bool expected_success) {
  std::string signature;
  bool success = signing_key->GetSignatureForDomain(domain, &signature);
  ASSERT_EQ(expected_success, success);
  EXPECT_NE(expected_success, signature.empty());
}

TEST_P(SignatureProviderWithValidKeyIndexTest, Test) {
  SignatureProvider provider;

  provider.set_current_key_version(GetParam());
  const SignatureProvider::SigningKey* signing_key = provider.GetCurrentKey();
  ASSERT_EQ(provider.GetKeyByVersion(GetParam()), signing_key);
  ASSERT_TRUE(signing_key);

  EXPECT_FALSE(signing_key->public_key().empty());

  CheckSignatureForDomain(signing_key, SignatureProvider::kTestDomain1, true);
  CheckSignatureForDomain(signing_key, SignatureProvider::kTestDomain2, true);
  CheckSignatureForDomain(signing_key, SignatureProvider::kTestDomain3, true);
  CheckSignatureForDomain(signing_key, "some-random-domain.com", false);

  std::string signature;
  EXPECT_TRUE(signing_key->Sign("some-string", &signature));
  EXPECT_FALSE(signature.empty());
}

INSTANTIATE_TEST_SUITE_P(All,
                         SignatureProviderWithValidKeyIndexTest,
                         testing::ValuesIn({1, 2}));

// Param: 1-based key version to be tested.
typedef ::testing::TestWithParam<int> SignatureProviderWithInvalidKeyIndexTest;

TEST_P(SignatureProviderWithInvalidKeyIndexTest, DomainSignatures) {
  SignatureProvider provider;

  provider.set_current_key_version(GetParam());
  EXPECT_FALSE(provider.GetCurrentKey());
  EXPECT_FALSE(provider.GetKeyByVersion(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(All,
                         SignatureProviderWithInvalidKeyIndexTest,
                         testing::ValuesIn({-1, 0, 3}));

}  // namespace policy
