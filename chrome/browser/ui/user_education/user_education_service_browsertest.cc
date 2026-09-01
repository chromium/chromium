// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_education/user_education_service.h"

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using UserEducationServiceBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(UserEducationServiceBrowserTest, TestServicePresent) {
  auto* const service = UserEducationServiceFactory::GetForBrowserContext(
      browser()->GetProfile());
  ASSERT_NE(nullptr, service);
  EXPECT_NE(nullptr, service->GetFeaturePromoControllerForTesting());
  EXPECT_NE(nullptr, service->new_badge_controller());
  EXPECT_NE(nullptr, service->new_badge_registry());
  EXPECT_NE(nullptr, service->ntp_promo_controller());
  EXPECT_NE(nullptr, service->ntp_promo_registry());
  EXPECT_NE(nullptr, service->tutorial_service());
}

IN_PROC_BROWSER_TEST_F(UserEducationServiceBrowserTest,
                       TestServicePresentInIncognitoWithSomeServicesMissing) {
  auto* const incog = CreateIncognitoBrowser(browser()->GetProfile());
  auto* const service =
      UserEducationServiceFactory::GetForBrowserContext(incog->GetProfile());
  ASSERT_NE(nullptr, service);
  EXPECT_EQ(nullptr, service->GetFeaturePromoControllerForTesting());
  EXPECT_EQ(nullptr, service->new_badge_controller());
  EXPECT_EQ(nullptr, service->new_badge_registry());
  EXPECT_EQ(nullptr, service->ntp_promo_controller());
  EXPECT_EQ(nullptr, service->ntp_promo_registry());
  EXPECT_EQ(nullptr, service->tutorial_service());
}

#if !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(UserEducationServiceBrowserTest,
                       TestServicePresentInGuestWithSomeServicesMissing) {
  auto* const guest = CreateGuestBrowser();
  auto* const service =
      UserEducationServiceFactory::GetForBrowserContext(guest->GetProfile());
  ASSERT_NE(nullptr, service);
  EXPECT_EQ(nullptr, service->GetFeaturePromoControllerForTesting());
  EXPECT_EQ(nullptr, service->new_badge_controller());
  EXPECT_EQ(nullptr, service->new_badge_registry());
  EXPECT_EQ(nullptr, service->ntp_promo_controller());
  EXPECT_EQ(nullptr, service->ntp_promo_registry());
}

#endif  // BUILDFLAG(IS_CHROMEOS)
