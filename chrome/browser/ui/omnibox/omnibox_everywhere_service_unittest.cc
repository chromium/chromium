// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere_service.h"

#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
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

TEST_F(OmniboxEverywhereServiceTest,
       ScreenshotDisclosureWithoutUIManagerCancels) {
  TestingProfile profile;
  OmniboxEverywhereService* service =
      OmniboxEverywhereServiceFactory::GetForProfile(&profile);
  ASSERT_TRUE(service);

  base::test::TestFuture<void> accepted_future;
  base::test::TestFuture<void> cancelled_future;
  service->ShowScreenshotDisclosureDialog(accepted_future.GetCallback(),
                                          cancelled_future.GetCallback());

  EXPECT_FALSE(accepted_future.IsReady());
  EXPECT_TRUE(cancelled_future.Wait());
  EXPECT_FALSE(
      omnibox_everywhere::prefs::IsScreenshotDisclosureAccepted(&profile));
}

TEST_F(OmniboxEverywhereServiceTest,
       ScreenshotDisclosureWithoutUIManagerHandlesNullCallbacks) {
  TestingProfile profile;
  OmniboxEverywhereService* service =
      OmniboxEverywhereServiceFactory::GetForProfile(&profile);
  ASSERT_TRUE(service);

  // Should not crash when called with null callbacks.
  service->ShowScreenshotDisclosureDialog(base::NullCallback(),
                                          base::NullCallback());
  EXPECT_FALSE(
      omnibox_everywhere::prefs::IsScreenshotDisclosureAccepted(&profile));
}

TEST_F(OmniboxEverywhereServiceTest, OnScreenshotDisclosureAccepted) {
  TestingProfile profile;
  OmniboxEverywhereService* service =
      OmniboxEverywhereServiceFactory::GetForProfile(&profile);
  ASSERT_TRUE(service);

  EXPECT_FALSE(
      omnibox_everywhere::prefs::IsScreenshotDisclosureAccepted(&profile));
  base::test::TestFuture<void> accepted_future;
  service->OnScreenshotDisclosureAcceptedForTesting(
      accepted_future.GetCallback());
  EXPECT_TRUE(accepted_future.Wait());
  EXPECT_TRUE(
      omnibox_everywhere::prefs::IsScreenshotDisclosureAccepted(&profile));
}

TEST_F(OmniboxEverywhereServiceTest, ProfileAccessorReturnsProfile) {
  TestingProfile profile;
  OmniboxEverywhereService* service =
      OmniboxEverywhereServiceFactory::GetForProfile(&profile);
  ASSERT_TRUE(service);
  EXPECT_EQ(&profile, service->profile());
}
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
