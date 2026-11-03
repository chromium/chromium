// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/profiles/profile_view_utils.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/subscription_eligibility/ai_subscription_tier_names.h"
#include "components/subscription_eligibility/subscription_eligibility_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

class ProfileViewUtilsTest : public testing::Test {
 public:
  ProfileViewUtilsTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~ProfileViewUtilsTest() override = default;

  // testing::Test:
  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    profile_ = testing_profile_manager()->CreateTestingProfile("profile");
    Test::SetUp();
  }

  TestingProfileManager* testing_profile_manager() {
    return &testing_profile_manager_;
  }

  TestingProfile* profile() { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager testing_profile_manager_;
  raw_ptr<TestingProfile> profile_;
};

// Assert the utility correctly reports whether the OTR affordance may be shown
// when the incognito policy is applied.
TEST_F(ProfileViewUtilsTest, IsOpenLinkOTREnabled_RespectsIncognitoPolicy) {
  const GURL test_url("https://www.foo.com/");
  IncognitoModePrefs::SetAvailability(
      profile()->GetPrefs(), policy::IncognitoModeAvailability::kDisabled);
  EXPECT_FALSE(IsOpenLinkOTREnabled(profile(), test_url));

  IncognitoModePrefs::SetAvailability(
      profile()->GetPrefs(), policy::IncognitoModeAvailability::kEnabled);
  EXPECT_TRUE(IsOpenLinkOTREnabled(profile(), test_url));
}

// Assert the utility correctly reports the OTR affordance may not be shown for
// a OTR source profile.
TEST_F(ProfileViewUtilsTest, IsOpenLinkOTREnabled_DisabledForOTRProfile) {
  const GURL test_url("https://www.foo.com/");
  EXPECT_TRUE(IsOpenLinkOTREnabled(profile(), test_url));

  TestingProfile::Builder incognito_builder;
  TestingProfile* incognito_profile =
      incognito_builder.BuildIncognito(profile());
  EXPECT_FALSE(IsOpenLinkOTREnabled(incognito_profile, test_url));
}

TEST_F(ProfileViewUtilsTest, LinearGradientDefaultRingSpecs) {
  ui::ColorProvider color_provider;

  // Avatars <= 24px (kSmallAvatarThresholdDip) get a 2px ring
  // (kAvatarRingThicknessSmallDip).
  // Total size = avatar_size + 2 * (gap_width (2px) + ring_thickness (2px))
  //            = avatar_size + 8px.
  for (int size : {16, 20, 24}) {
    EXPECT_EQ(GetAvatarRingThickness(size), 2);

    ui::ImageModel avatar =
        ui::ImageModel::FromImage(gfx::test::CreateImage(size, size));
    gfx::ImageSkia ring_avatar =
        AddLinearGradientRingToAvatar(avatar, color_provider, size);
    EXPECT_EQ(ring_avatar.size(), gfx::Size(size + 8, size + 8));
  }

  // Avatars > 24px (kSmallAvatarThresholdDip) get a 3px ring
  // (kAvatarRingThicknessDip).
  // Total size = avatar_size + 2 * (gap_width (2px) + ring_thickness (3px))
  //            = avatar_size + 10px.
  for (int size : {25, 60, 74}) {
    EXPECT_EQ(GetAvatarRingThickness(size), 3);

    ui::ImageModel avatar =
        ui::ImageModel::FromImage(gfx::test::CreateImage(size, size));
    gfx::ImageSkia ring_avatar =
        AddLinearGradientRingToAvatar(avatar, color_provider, size);
    EXPECT_EQ(ring_avatar.size(), gfx::Size(size + 10, size + 10));
  }

  // Verify explicit ring_thickness override is respected.
  {
    constexpr int kAvatarSize = 20;
    constexpr int kCustomThickness = 5;
    ui::ImageModel avatar = ui::ImageModel::FromImage(
        gfx::test::CreateImage(kAvatarSize, kAvatarSize));
    gfx::ImageSkia ring_avatar =
        AddLinearGradientRingToAvatar(avatar, color_provider, kAvatarSize,
                                      kAvatarRingGapDip, kCustomThickness);
    // Total size = 20 + 2 * (gap (2px) + thickness (5px)) = 34px.
    EXPECT_EQ(ring_avatar.size(), gfx::Size(34, 34));
  }
}

class ProfileViewUtilsAvatarRingTest : public ProfileViewUtilsTest {
 protected:
  void SetTier(int tier) {
    profile()->GetPrefs()->SetInteger(
        subscription_eligibility::prefs::kAiSubscriptionTier, tier);
  }
};

TEST_F(ProfileViewUtilsAvatarRingTest, HiddenWhenRingFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(switches::kEnableAiSubscriptionAvatarRing);

  SetTier(1);
  EXPECT_FALSE(GetAvatarAiDecorationForTier(1).show_decoration);
  EXPECT_FALSE(GetAvatarAiDecoration(profile()).show_decoration);
}

// The presentation itself, including the overrides, is tested in the component:
// the avatar decoration is the component's presentation of the tier.
TEST_F(ProfileViewUtilsAvatarRingTest, UsesAiSubscriptionTierDisplayInfo) {
  base::test::ScopedFeatureList feature_list(
      switches::kEnableAiSubscriptionAvatarRing);

  for (int tier : {0, 1, 2, 4}) {
    EXPECT_EQ(GetAvatarAiDecorationForTier(tier),
              subscription_eligibility::GetAiSubscriptionTierDisplayInfo(tier))
        << "tier " << tier;
  }

  EXPECT_FALSE(GetAvatarAiDecoration(nullptr).show_decoration);
  SetTier(2);
  EXPECT_EQ(GetAvatarAiDecoration(profile()),
            subscription_eligibility::GetAiSubscriptionTierDisplayInfo(2));
  EXPECT_TRUE(GetAvatarAiDecoration(profile()).show_decoration);
  SetTier(0);
  EXPECT_FALSE(GetAvatarAiDecoration(profile()).show_decoration);
}
