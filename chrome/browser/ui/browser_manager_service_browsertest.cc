// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_manager_service.h"

#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_manager_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/test/base/testing_browser_process.h"
#endif  // !BUILDFLAG(IS_CHROMEOS)

using testing::_;

class MockBrowserCollectionObserver : public BrowserCollectionObserver {
 public:
  MOCK_METHOD(void, OnBrowserCreated, (BrowserWindowInterface * browser));
  MOCK_METHOD(void, OnBrowserClosed, (BrowserWindowInterface * browser));
  MOCK_METHOD(void, OnBrowserActivated, (BrowserWindowInterface * browser));
  MOCK_METHOD(void, OnBrowserDeactivated, (BrowserWindowInterface * browser));
};

class BrowserManagerServiceTest : public InProcessBrowserTest {
 protected:
#if !BUILDFLAG(IS_CHROMEOS)
  Profile& CreateSecondaryProfile() {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    return profiles::testing::CreateProfileSync(
        profile_manager, profile_manager->GenerateNextProfileDirectoryPath());
  }
#endif  // !BUILDFLAG(IS_CHROMEOS)

  // TODO(crbug.com/356183782): Consider rewriting this test as an interactive
  // ui test and using ui_test_utils::BringBrowserWindowToFront() instead.
  void ActivatePrimaryBrowser(Browser* const secondary_browser) {
    browser()->DidBecomeActive();
    secondary_browser->DidBecomeInactive();
  }

  void ActivateSecondaryBrowser(Browser* const secondary_browser) {
    secondary_browser->DidBecomeActive();
    browser()->DidBecomeInactive();
  }
};

IN_PROC_BROWSER_TEST_F(BrowserManagerServiceTest,
                       TestObservationWithSingleProfile) {
  // Observe primary profile.
  MockBrowserCollectionObserver observer;
  base::ScopedObservation<ProfileBrowserCollection, BrowserCollectionObserver>
      observation{&observer};
  observation.Observe(
      BrowserManagerServiceFactory::GetForProfile(GetProfile()));

  // Create secondary browser and expect events.
  EXPECT_CALL(observer, OnBrowserCreated(_)).Times(1);
  Browser* secondary_browser = CreateBrowser(GetProfile());
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Start with secondary browser active.
  ActivateSecondaryBrowser(secondary_browser);

  // Activate primary browser and expect events.
  EXPECT_CALL(observer, OnBrowserActivated(browser())).Times(1);
  EXPECT_CALL(observer, OnBrowserDeactivated(secondary_browser)).Times(1);
  ActivatePrimaryBrowser(secondary_browser);
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Reactivate secondary browser and expect events.
  EXPECT_CALL(observer, OnBrowserDeactivated(browser())).Times(1);
  EXPECT_CALL(observer, OnBrowserActivated(secondary_browser)).Times(1);
  ActivateSecondaryBrowser(secondary_browser);
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Close secondary browser and expect events.
  EXPECT_CALL(observer, OnBrowserClosed(secondary_browser)).Times(1);
  CloseBrowserSynchronously(secondary_browser);
}

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(BrowserManagerServiceTest,
                       TestObservationWithMultipleProfiles) {
  // Observe primary profile.
  MockBrowserCollectionObserver primary_observer;
  base::ScopedObservation<ProfileBrowserCollection, BrowserCollectionObserver>
      primary_observation{&primary_observer};
  primary_observation.Observe(
      BrowserManagerServiceFactory::GetForProfile(GetProfile()));

  // Create and observe secondary profile.
  Profile& secondary_profile = CreateSecondaryProfile();
  MockBrowserCollectionObserver secondary_observer;
  base::ScopedObservation<ProfileBrowserCollection, BrowserCollectionObserver>
      secondary_observation{&secondary_observer};
  secondary_observation.Observe(
      BrowserManagerServiceFactory::GetForProfile(&secondary_profile));

  // Create secondary browser and expect events.
  EXPECT_CALL(secondary_observer, OnBrowserCreated(_)).Times(1);
  Browser* secondary_browser = CreateBrowser(&secondary_profile);
  testing::Mock::VerifyAndClearExpectations(&secondary_observer);

  // Start with secondary browser active.
  ActivateSecondaryBrowser(secondary_browser);

  // Activate primary browser and expect events.
  EXPECT_CALL(primary_observer, OnBrowserActivated(browser())).Times(1);
  EXPECT_CALL(secondary_observer, OnBrowserDeactivated(secondary_browser))
      .Times(1);
  ActivatePrimaryBrowser(secondary_browser);
  testing::Mock::VerifyAndClearExpectations(&primary_observer);
  testing::Mock::VerifyAndClearExpectations(&secondary_observer);

  // Reactivate secondary browser and expect events.
  EXPECT_CALL(primary_observer, OnBrowserDeactivated(browser())).Times(1);
  EXPECT_CALL(secondary_observer, OnBrowserActivated(secondary_browser))
      .Times(1);
  ActivateSecondaryBrowser(secondary_browser);
  testing::Mock::VerifyAndClearExpectations(&primary_observer);
  testing::Mock::VerifyAndClearExpectations(&secondary_observer);

  // Close secondary browser and expect events.
  EXPECT_CALL(secondary_observer, OnBrowserClosed(secondary_browser)).Times(1);
  CloseBrowserSynchronously(secondary_browser);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)
