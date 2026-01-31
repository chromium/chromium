// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"

#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

using testing::_;

class MockBrowserCollectionObserver
    : public testing::NiceMock<BrowserCollectionObserver> {
 public:
  MOCK_METHOD(void, OnBrowserCreated, (BrowserWindowInterface * browser));
  MOCK_METHOD(void, OnBrowserClosed, (BrowserWindowInterface * browser));
  MOCK_METHOD(void, OnBrowserActivated, (BrowserWindowInterface * browser));
  MOCK_METHOD(void, OnBrowserDeactivated, (BrowserWindowInterface * browser));
};

class GlobalBrowserCollectionTest : public PlatformBrowserTest {
 public:
#if BUILDFLAG(IS_CHROMEOS)
  // This makes multi-profile work on ChromeOS.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        ash::switches::kIgnoreUserProfileMappingForTests);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

 protected:
  BrowserWindowInterface* GetPrimaryBrowser() {
#if BUILDFLAG(IS_ANDROID)
    // TODO(crbug.com/477251911): Add implementation for Android.
    return nullptr;
#else
    return browser();
#endif  // BUILDFLAG(IS_ANDROID)
  }

  Profile* GetPrimaryProfile() { return GetPrimaryBrowser()->GetProfile(); }

  BrowserWindowInterface* CreateAndActivateBrowser(Profile* profile) {
    BrowserWindowInterface* browser = CreateBrowserWindow(
        BrowserWindowCreateParams(*profile, /*from_user_gesture=*/true));
#if BUILDFLAG(IS_ANDROID)
    // TODO(crbug.com/477251911): Add |browser| activation on Android.
#else
    browser->GetBrowserForMigrationOnly()->DidBecomeActive();
#endif  // BUILDFLAG(IS_ANDROID)
    return browser;
  }

  // Creates a new Profile and an associated browser. Reuses the default test
  // profile if multi-profile is not supported (eg. on Android at the moment).
  BrowserWindowInterface* CreateAndActivateBrowserWithNewProfileIfEnabled() {
    Profile* new_profile = GetPrimaryProfile();

    if (profiles::IsMultipleProfilesEnabled()) {
      ProfileManager* const profile_manager =
          g_browser_process->profile_manager();
      new_profile = &profiles::testing::CreateProfileSync(
          profile_manager, profile_manager->GenerateNextProfileDirectoryPath());
    }

    return CreateAndActivateBrowser(new_profile);
  }

  // TODO(crbug.com/356183782): Consider rewriting this test as an interactive
  // ui test and using ui_test_utils::BringBrowserWindowToFront() instead.
  void ActivatePrimaryBrowser(BrowserWindowInterface* const secondary_browser) {
#if BUILDFLAG(IS_ANDROID)
    // TODO(crbug.com/477251911): Add implementation for Android.
#else
    browser()->DidBecomeActive();
    secondary_browser->GetBrowserForMigrationOnly()->DidBecomeInactive();
#endif  // BUILDFLAG(IS_ANDROID)
  }

  void ActivateSecondaryBrowser(
      BrowserWindowInterface* const secondary_browser) {
#if BUILDFLAG(IS_ANDROID)
    // TODO(crbug.com/477251911): Add implementation for Android.
#else
    secondary_browser->GetBrowserForMigrationOnly()->DidBecomeActive();
    browser()->DidBecomeInactive();
#endif  // BUILDFLAG(IS_ANDROID)
  }

  void CloseBrowserSynchronouslyCrossPlatform(BrowserWindowInterface* browser) {
#if BUILDFLAG(IS_ANDROID)
    // TODO(crbug.com/477251911): Add implementation for Android.
#else
    CloseBrowserSynchronously(browser);
#endif  // BUILDFLAG(IS_ANDROID)
  }
};

// TODO(crbug.com/477251911): Make this test work on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_TestObservationWithSingleProfile \
  DISABLED_TestObservationWithSingleProfile
#else
#define MAYBE_TestObservationWithSingleProfile TestObservationWithSingleProfile
#endif  // BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(GlobalBrowserCollectionTest,
                       MAYBE_TestObservationWithSingleProfile) {
  // Observe GlobalBrowserCollection.
  MockBrowserCollectionObserver observer;
  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      observation{&observer};
  observation.Observe(GlobalBrowserCollection::GetInstance());

  // Create secondary browser and expect events.
  EXPECT_CALL(observer, OnBrowserCreated(_)).Times(1);
  BrowserWindowInterface* const secondary_browser =
      CreateAndActivateBrowser(GetPrimaryProfile());
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Start with secondary browser active (and primary browser inactive).
  ActivateSecondaryBrowser(secondary_browser);

  // Activate primary browser and expect events.
  EXPECT_CALL(observer, OnBrowserActivated(GetPrimaryBrowser())).Times(1);
  EXPECT_CALL(observer, OnBrowserDeactivated(secondary_browser)).Times(1);
  ActivatePrimaryBrowser(secondary_browser);
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Reactivate secondary browser and expect events.
  EXPECT_CALL(observer, OnBrowserDeactivated(GetPrimaryBrowser())).Times(1);
  EXPECT_CALL(observer, OnBrowserActivated(secondary_browser)).Times(1);
  ActivateSecondaryBrowser(secondary_browser);
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Close secondary browser and expect events.
  EXPECT_CALL(observer, OnBrowserClosed(secondary_browser)).Times(1);
  CloseBrowserSynchronouslyCrossPlatform(secondary_browser);
}

// TODO(crbug.com/477251911): Make this test work on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_TestObservationWithMultipleProfiles \
  DISABLED_TestObservationWithMultipleProfiles
#else
#define MAYBE_TestObservationWithMultipleProfiles \
  TestObservationWithMultipleProfiles
#endif  // BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(GlobalBrowserCollectionTest,
                       MAYBE_TestObservationWithMultipleProfiles) {
  // This test is only relevant on platforms that support multi-profile.
  if (!profiles::IsMultipleProfilesEnabled()) {
    GTEST_SKIP() << "Multiple profiles disabled";
  }

  // Observe GlobalBrowserCollection.
  MockBrowserCollectionObserver observer;
  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      observation{&observer};
  observation.Observe(GlobalBrowserCollection::GetInstance());

  // Create secondary browser and expect events.
  EXPECT_CALL(observer, OnBrowserCreated(_)).Times(1);
  BrowserWindowInterface* const secondary_browser =
      CreateAndActivateBrowserWithNewProfileIfEnabled();
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Start with secondary browser active (and primary browser inactive).
  ActivateSecondaryBrowser(secondary_browser);

  // Activate primary browser and expect events.
  EXPECT_CALL(observer, OnBrowserActivated(GetPrimaryBrowser())).Times(1);
  EXPECT_CALL(observer, OnBrowserDeactivated(secondary_browser)).Times(1);
  ActivatePrimaryBrowser(secondary_browser);
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Reactivate secondary browser and expect events.
  EXPECT_CALL(observer, OnBrowserDeactivated(GetPrimaryBrowser())).Times(1);
  EXPECT_CALL(observer, OnBrowserActivated(secondary_browser)).Times(1);
  ActivateSecondaryBrowser(secondary_browser);
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Close secondary browser and expect events.
  EXPECT_CALL(observer, OnBrowserClosed(secondary_browser)).Times(1);
  CloseBrowserSynchronouslyCrossPlatform(secondary_browser);
}

// Fixture that sets up 3 browsers.
class GlobalBrowserCollectionTestWithOrder
    : public GlobalBrowserCollectionTest,
      public testing::WithParamInterface<BrowserCollection::Order> {
 protected:
  // GlobalBrowserCollectionTest:
  void SetUpOnMainThread() override {
    GlobalBrowserCollectionTest::SetUpOnMainThread();
    // Browsers are activated in the order they are created, resulting in an
    // activation order the reverse of creation order.
    browsers_.push_back(GetPrimaryBrowser());
    browsers_.push_back(CreateAndActivateBrowserWithNewProfileIfEnabled());
    browsers_.push_back(CreateAndActivateBrowserWithNewProfileIfEnabled());

    const auto* global_colection = GlobalBrowserCollection::GetInstance();
    EXPECT_FALSE(global_colection->IsEmpty());
    EXPECT_EQ(global_colection->GetSize(), 3u);
  }
  void TearDownOnMainThread() override {
    browsers_.clear();
    GlobalBrowserCollectionTest::TearDownOnMainThread();
  }

  BrowserWindowInterface* GetBrowser(int index) { return browsers_.at(index); }

  BrowserWindowInterface* GetAndClearBrowser(int index) {
    BrowserWindowInterface* tmp = browsers_.at(index);
    browsers_.at(index) = nullptr;
    return tmp;
  }

 private:
  // Browser instances in creation order.
  std::vector<raw_ptr<BrowserWindowInterface>> browsers_;
};

// TODO(crbug.com/477251911): Make this test work on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ForEachIteratesOverAllBrowsers \
  DISABLED_ForEachIteratesOverAllBrowsers
#else
#define MAYBE_ForEachIteratesOverAllBrowsers ForEachIteratesOverAllBrowsers
#endif  // BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_P(GlobalBrowserCollectionTestWithOrder,
                       MAYBE_ForEachIteratesOverAllBrowsers) {
  std::vector<BrowserWindowInterface*> visited;
  GlobalBrowserCollection::GetInstance()->ForEach(
      [&](BrowserWindowInterface* b) {
        visited.push_back(b);
        return true;
      },
      GetParam());

  EXPECT_EQ(visited.size(), 3u);
  if (GetParam() == BrowserCollection::Order::kCreation) {
    EXPECT_EQ(visited[0], GetBrowser(0));
    EXPECT_EQ(visited[1], GetBrowser(1));
    EXPECT_EQ(visited[2], GetBrowser(2));
  } else {
    EXPECT_EQ(visited[0], GetBrowser(2));
    EXPECT_EQ(visited[1], GetBrowser(1));
    EXPECT_EQ(visited[2], GetBrowser(0));
  }
}

// TODO(crbug.com/477251911): Make this test work on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ForEachStopsWhenCallbackReturnsFalse \
  DISABLED_ForEachStopsWhenCallbackReturnsFalse
#else
#define MAYBE_ForEachStopsWhenCallbackReturnsFalse \
  ForEachStopsWhenCallbackReturnsFalse
#endif  // BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_P(GlobalBrowserCollectionTestWithOrder,
                       MAYBE_ForEachStopsWhenCallbackReturnsFalse) {
  std::vector<BrowserWindowInterface*> visited;
  GlobalBrowserCollection::GetInstance()->ForEach(
      [&](BrowserWindowInterface* b) {
        visited.push_back(b);
        return false;
      },
      GetParam());

  EXPECT_EQ(visited.size(), 1u);
  if (GetParam() == BrowserCollection::Order::kCreation) {
    EXPECT_EQ(visited[0], GetBrowser(0));
  } else {
    EXPECT_EQ(visited[0], GetBrowser(2));
  }
}

// TODO(crbug.com/477251911): Make this test work on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ForEachResilientToBrowserDestruction \
  DISABLED_ForEachResilientToBrowserDestruction
#else
#define MAYBE_ForEachResilientToBrowserDestruction \
  ForEachResilientToBrowserDestruction
#endif  // BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_P(GlobalBrowserCollectionTestWithOrder,
                       MAYBE_ForEachResilientToBrowserDestruction) {
  std::vector<BrowserWindowInterface*> visited;
  GlobalBrowserCollection::GetInstance()->ForEach(
      [&](BrowserWindowInterface* b) {
        visited.push_back(b);
        if (visited.size() == 1) {
          // Close the second browser mid-iteration.
          CloseBrowserSynchronouslyCrossPlatform(GetAndClearBrowser(1));
        }
        return true;
      },
      GetParam());

  // Should visit browser 0, skip browser 1 (because it was closed), and visit
  // browser 2.
  EXPECT_EQ(visited.size(), 2u);
  if (GetParam() == BrowserCollection::Order::kCreation) {
    EXPECT_EQ(visited[0], GetBrowser(0));
    EXPECT_EQ(visited[1], GetBrowser(2));
  } else {
    EXPECT_EQ(visited[0], GetBrowser(2));
    EXPECT_EQ(visited[1], GetBrowser(0));
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    GlobalBrowserCollectionTestWithOrder,
    ::testing::Values(BrowserCollection::Order::kCreation,
                      BrowserCollection::Order::kActivation),
    [](const testing::TestParamInfo<BrowserCollection::Order>& param) {
      switch (param.param) {
        case BrowserCollection::Order::kCreation:
          return "CreationOrder";
        case BrowserCollection::Order::kActivation:
          return "ActivationOrder";
      }
    });
