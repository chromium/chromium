// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/gpm_pin_service.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/webauthn/gpm_pin_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "device/fido/public/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webauthn {
namespace {

class GpmPinServiceTest : public testing::Test {
 protected:
  base::test::ScopedFeatureList feature_list_{
      device::kWebAuthnLocalGpmPasskeys};
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(GpmPinServiceTest, NullWhenFeatureDisabled) {
  base::test::ScopedFeatureList disabled;
  disabled.InitAndDisableFeature(device::kWebAuthnLocalGpmPasskeys);
  EXPECT_FALSE(GpmPinServiceFactory::GetForProfile(&profile_));
}

TEST_F(GpmPinServiceTest, NullForOffTheRecordProfile) {
  ASSERT_TRUE(GpmPinServiceFactory::GetForProfile(&profile_));
  EXPECT_FALSE(GpmPinServiceFactory::GetForProfile(
      profile_.GetPrimaryOTRProfile(/*create_if_needed=*/true)));
}

TEST_F(GpmPinServiceTest, NullForGuestProfile) {
  TestingProfile::Builder builder;
  builder.SetGuestSession();
  std::unique_ptr<TestingProfile> guest = builder.Build();
  EXPECT_FALSE(GpmPinServiceFactory::GetForProfile(guest.get()));
}

}  // namespace
}  // namespace webauthn
