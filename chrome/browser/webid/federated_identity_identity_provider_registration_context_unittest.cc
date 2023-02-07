// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_identity_provider_registration_context.h"

#include <memory>

#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

class FederatedIdentityIdentityProviderRegistrationContextTest
    : public testing::Test {
 public:
  FederatedIdentityIdentityProviderRegistrationContextTest() {
    context_ =
        std::make_unique<FederatedIdentityIdentityProviderRegistrationContext>(
            &profile_);
  }

  void TearDown() override { context_.reset(); }

  ~FederatedIdentityIdentityProviderRegistrationContextTest() override =
      default;

  FederatedIdentityIdentityProviderRegistrationContextTest(
      FederatedIdentityIdentityProviderRegistrationContextTest&) = delete;
  FederatedIdentityIdentityProviderRegistrationContextTest& operator=(
      FederatedIdentityIdentityProviderRegistrationContextTest&) = delete;

  FederatedIdentityIdentityProviderRegistrationContext* context() {
    return context_.get();
  }
  TestingProfile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<FederatedIdentityIdentityProviderRegistrationContext>
      context_;
  TestingProfile profile_;
};

// Test the initial state of the registry.
TEST_F(FederatedIdentityIdentityProviderRegistrationContextTest,
       InitiallyEmptyRegistry) {
  EXPECT_TRUE(context()->GetRegisteredIdPs().empty());
}

// Test the state of the registry after registration.
TEST_F(FederatedIdentityIdentityProviderRegistrationContextTest, RegisterIdP) {
  const GURL configURL = GURL("https://idp.example/fedcm.json");

  context()->RegisterIdP(configURL);

  EXPECT_EQ(std::vector<GURL>{configURL}, context()->GetRegisteredIdPs());
}

// Test that registrating twice is idempotent.
TEST_F(FederatedIdentityIdentityProviderRegistrationContextTest,
       RegistrationIsIdempotent) {
  const GURL configURL = GURL("https://idp.example/fedcm.json");

  context()->RegisterIdP(configURL);
  context()->RegisterIdP(configURL);

  // Removes duplicates.
  EXPECT_EQ(std::vector<GURL>{configURL}, context()->GetRegisteredIdPs());
}

// Test that registering two IdPs preserves the order.
TEST_F(FederatedIdentityIdentityProviderRegistrationContextTest,
       RegisteringTwoIdPs) {
  const GURL configURL1 = GURL("https://idp1.example/fedcm.json");
  const GURL configURL2 = GURL("https://idp2.example/fedcm.json");

  context()->RegisterIdP(configURL1);
  context()->RegisterIdP(configURL2);

  std::vector<GURL> expected = {configURL1, configURL2};

  EXPECT_EQ(expected, context()->GetRegisteredIdPs());
}

// Registers and unregisters an IdP.
TEST_F(FederatedIdentityIdentityProviderRegistrationContextTest,
       Unregistering) {
  const GURL configURL = GURL("https://idp.example/fedcm.json");

  context()->RegisterIdP(configURL);
  EXPECT_EQ(std::vector<GURL>{configURL}, context()->GetRegisteredIdPs());

  context()->UnregisterIdP(configURL);

  EXPECT_TRUE(context()->GetRegisteredIdPs().empty());
}

// Unregistering an IdP that wasn't registered is a no-op.
TEST_F(FederatedIdentityIdentityProviderRegistrationContextTest,
       UnregisteringWithoutRegistering) {
  const GURL configURL = GURL("https://idp.example/fedcm.json");

  EXPECT_TRUE(context()->GetRegisteredIdPs().empty());

  context()->UnregisterIdP(configURL);

  EXPECT_TRUE(context()->GetRegisteredIdPs().empty());
}

// RegistersTwoIdPsUnregistersOne
TEST_F(FederatedIdentityIdentityProviderRegistrationContextTest,
       RegisteringTwoIdPsUnregistersOne) {
  const GURL configURL1 = GURL("https://idp1.example/fedcm.json");
  const GURL configURL2 = GURL("https://idp2.example/fedcm.json");

  context()->RegisterIdP(configURL1);
  context()->RegisterIdP(configURL2);
  context()->UnregisterIdP(configURL1);

  std::vector<GURL> expected = {configURL2};

  EXPECT_EQ(expected, context()->GetRegisteredIdPs());
}
