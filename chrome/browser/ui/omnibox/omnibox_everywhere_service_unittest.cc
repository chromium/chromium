// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere_service.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere_service_factory.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class OmniboxEverywhereServiceTest : public testing::Test {
 public:
  OmniboxEverywhereServiceTest() {
    feature_list_.InitAndEnableFeature(omnibox::kOmniboxEverywhere);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(OmniboxEverywhereServiceTest, GetForProfile) {
  TestingProfile profile;
  OmniboxEverywhereService* service =
      OmniboxEverywhereServiceFactory::GetForProfile(&profile);
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  ASSERT_TRUE(service);
  EXPECT_FALSE(service->IsPopupVisible());
#else
  EXPECT_FALSE(service);
#endif
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
TEST_F(OmniboxEverywhereServiceTest, FrePreferenceDefaultsToFalse) {
  TestingProfile profile;
  EXPECT_FALSE(
      profile.GetPrefs()->GetBoolean(omnibox_everywhere::prefs::kFreDismissed));
  EXPECT_EQ(0, profile.GetPrefs()->GetInteger(
                   omnibox_everywhere::prefs::kFreImpressionCount));
}

TEST_F(OmniboxEverywhereServiceTest, ProfileAccessorReturnsProfile) {
  TestingProfile profile;
  OmniboxEverywhereService* service =
      OmniboxEverywhereServiceFactory::GetForProfile(&profile);
  ASSERT_TRUE(service);
  EXPECT_EQ(&profile, service->profile());
}
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
