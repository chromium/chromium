// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/browser_window/test/browser_event_waiter.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/base_window.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_window/test/android/browser_window_android_browsertest_base.h"
#define TestBase BrowserWindowAndroidBrowserTestBase
#else
#include "chrome/test/base/in_process_browser_test.h"
#define TestBase InProcessBrowserTest
#endif  // BUILDFLAG(IS_ANDROID)

using testing::_;

class MockBrowserCollectionObserver
    : public testing::NiceMock<BrowserCollectionObserver> {
 public:
  MOCK_METHOD(void, OnBrowserCreated, (BrowserWindowInterface * browser));
  MOCK_METHOD(void, OnBrowserClosed, (BrowserWindowInterface * browser));
  MOCK_METHOD(void, OnBrowserActivated, (BrowserWindowInterface * browser));
  MOCK_METHOD(void, OnBrowserDeactivated, (BrowserWindowInterface * browser));
};

// Runs on all desktop platforms except ChromeOS (including desktop Android),
// but not on mobile.
class GlobalBrowserCollectionTest : public TestBase {
 protected:
  // TestBase:
  void SetUpOnMainThread() override {
    TestBase::SetUpOnMainThread();

    primary_browser_ = GetLastActiveBrowserWindowInterfaceWithAnyProfile();
    primary_profile_ = primary_browser_->GetProfile();
    active_browser_ = primary_browser_;
  }

  void TearDownOnMainThread() override {
    active_browser_ = nullptr;
    primary_profile_ = nullptr;
    primary_browser_ = nullptr;

    TestBase::TearDownOnMainThread();
  }

  BrowserWindowInterface* GetPrimaryBrowser() { return primary_browser_.get(); }
  Profile* GetPrimaryProfile() { return primary_profile_.get(); }

  // TODO(crbug.com/356183782): Consider using
  // ui_test_utils::BringBrowserWindowToFront() instead.
  void ActivateBrowser(BrowserWindowInterface* browser) {
    BrowserEventWaiter browser_activated_waiter(
        BrowserEventWaiter::Event::ACTIVATED, browser);

// TODO(crbug.com/483363917): Enable this on Linux once the
// BaseWindow::Activate() behaviour is fixed.
#if !BUILDFLAG(IS_LINUX)
    BrowserEventWaiter browser_deactivated_waiter(
        BrowserEventWaiter::Event::DEACTIVATED, active_browser_);
#endif  // !BUILDFLAG(IS_LINUX)

    browser->GetWindow()->Activate();
    active_browser_ = browser;
  }

  BrowserWindowInterface* CreateAndActivateBrowser(Profile* profile) {
    BrowserWindowInterface* browser;
    {
      BrowserEventWaiter browser_created_waiter(
          BrowserEventWaiter::Event::CREATED);
      browser = CreateBrowserWindow(
          BrowserWindowCreateParams(*profile, /*from_user_gesture=*/true));
    }

    // TODO(crbug.com/477251911): Enable this on Android (if Android browsers
    // aren't already activated upon creation) once we implement activation
    // tracking in GlobalBrowserCollection for Android.
#if !BUILDFLAG(IS_ANDROID)
    ActivateBrowser(browser);
#endif  // !BUILDFLAG(IS_ANDROID)

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

  // This should be close to the last thing called. Calling Activate() after
  // this function will not do what you want.
  void CloseBrowserSynchronouslyCrossPlatform(BrowserWindowInterface* browser) {
    BrowserEventWaiter browser_closed_waiter(BrowserEventWaiter::Event::CLOSED,
                                             browser);
    if (browser == active_browser_) {
      // This avoids a dangling raw_ptr. It means that we can't use Activate()
      // after this function is called, but the windows would have been in an
      // unknown state after closing a browser anyway, since we wouldn't know
      // which other window the OS would activate.
      active_browser_ = nullptr;
    }

#if BUILDFLAG(IS_ANDROID)
    browser->GetWindow()->Close();
#else
    // TODO(crbug.com/478908209): I don't know why, but
    // `browser->GetWindow()->Close();` doesn't seem to trigger
    // GlobalBrowserCollection::OnBrowserClosed() on desktop.
    CloseBrowserSynchronously(browser);
#endif  // BUILDFLAG(IS_ANDROID)
  }

  // Returns the result of browser->IsActive() on platforms that support it.
  // Otherwise, just returns the |want| value.
  bool IsActiveIfAvailable(BrowserWindowInterface* browser, bool want) {
#if BUILDFLAG(IS_ANDROID)
    return want;
#else
    return browser->IsActive();
#endif  // BUILDFLAG(IS_ANDROID)
  }

 private:
  raw_ptr<BrowserWindowInterface> primary_browser_ = nullptr;
  raw_ptr<Profile> primary_profile_ = nullptr;
  raw_ptr<BrowserWindowInterface> active_browser_ = nullptr;
};

// TODO(crbug.com/477251911): Enable this on Android once we implement
// activation tracking in GlobalBrowserCollection for Android.
//
// TODO(crbug.com/483363917): Enable this on Linux once the
// BaseWindow::Activate() behaviour is fixed.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
#define MAYBE_TestObservationWithSingleProfile \
  DISABLED_TestObservationWithSingleProfile
#else
#define MAYBE_TestObservationWithSingleProfile TestObservationWithSingleProfile
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
IN_PROC_BROWSER_TEST_F(GlobalBrowserCollectionTest,
                       MAYBE_TestObservationWithSingleProfile) {
  // Observe GlobalBrowserCollection.
  MockBrowserCollectionObserver observer;
  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      observation{&observer};
  observation.Observe(GlobalBrowserCollection::GetInstance());

  EXPECT_TRUE(IsActiveIfAvailable(GetPrimaryBrowser(), /*want=*/true));

  // Create secondary browser and expect events.
  EXPECT_CALL(observer, OnBrowserCreated(_)).Times(1);
  BrowserWindowInterface* const secondary_browser =
      CreateAndActivateBrowser(GetPrimaryProfile());
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_FALSE(IsActiveIfAvailable(GetPrimaryBrowser(), /*want=*/false));
  EXPECT_TRUE(IsActiveIfAvailable(secondary_browser, /*want=*/true));

  // Activate primary browser and expect events.
  EXPECT_CALL(observer, OnBrowserActivated(GetPrimaryBrowser())).Times(1);
  EXPECT_CALL(observer, OnBrowserDeactivated(secondary_browser)).Times(1);
  ActivateBrowser(GetPrimaryBrowser());
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_TRUE(IsActiveIfAvailable(GetPrimaryBrowser(), /*want=*/true));
  EXPECT_FALSE(IsActiveIfAvailable(secondary_browser, /*want=*/false));

  // Reactivate secondary browser and expect events.
  EXPECT_CALL(observer, OnBrowserDeactivated(GetPrimaryBrowser())).Times(1);
  EXPECT_CALL(observer, OnBrowserActivated(secondary_browser)).Times(1);
  ActivateBrowser(secondary_browser);
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_FALSE(IsActiveIfAvailable(GetPrimaryBrowser(), /*want=*/false));
  EXPECT_TRUE(IsActiveIfAvailable(secondary_browser, /*want=*/true));

  // Close secondary browser and expect events.
  EXPECT_CALL(observer, OnBrowserClosed(secondary_browser)).Times(1);
  CloseBrowserSynchronouslyCrossPlatform(secondary_browser);
}

// TODO(crbug.com/477251911): Enable this on Android once we implement
// activation tracking in GlobalBrowserCollection for Android.
//
// TODO(crbug.com/483363917): Enable this on Linux once the
// BaseWindow::Activate() behaviour is fixed.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
#define MAYBE_TestObservationWithMultipleProfiles \
  DISABLED_TestObservationWithMultipleProfiles
#else
#define MAYBE_TestObservationWithMultipleProfiles \
  TestObservationWithMultipleProfiles
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
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

  EXPECT_TRUE(IsActiveIfAvailable(GetPrimaryBrowser(), /*want=*/true));

  // Create secondary browser and expect events.
  EXPECT_CALL(observer, OnBrowserCreated(_)).Times(1);
  BrowserWindowInterface* const secondary_browser =
      CreateAndActivateBrowserWithNewProfileIfEnabled();
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_FALSE(IsActiveIfAvailable(GetPrimaryBrowser(), /*want=*/false));
  EXPECT_TRUE(IsActiveIfAvailable(secondary_browser, /*want=*/true));

  // Activate primary browser and expect events.
  EXPECT_CALL(observer, OnBrowserActivated(GetPrimaryBrowser())).Times(1);
  EXPECT_CALL(observer, OnBrowserDeactivated(secondary_browser)).Times(1);
  ActivateBrowser(GetPrimaryBrowser());
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_TRUE(IsActiveIfAvailable(GetPrimaryBrowser(), /*want=*/true));
  EXPECT_FALSE(IsActiveIfAvailable(secondary_browser, /*want=*/false));

  // Reactivate secondary browser and expect events.
  EXPECT_CALL(observer, OnBrowserDeactivated(GetPrimaryBrowser())).Times(1);
  EXPECT_CALL(observer, OnBrowserActivated(secondary_browser)).Times(1);
  ActivateBrowser(secondary_browser);
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_FALSE(IsActiveIfAvailable(GetPrimaryBrowser(), /*want=*/false));
  EXPECT_TRUE(IsActiveIfAvailable(secondary_browser, /*want=*/true));

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

IN_PROC_BROWSER_TEST_P(GlobalBrowserCollectionTestWithOrder,
                       ForEachIteratesOverAllBrowsers) {
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

IN_PROC_BROWSER_TEST_P(GlobalBrowserCollectionTestWithOrder,
                       ForEachStopsWhenCallbackReturnsFalse) {
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

IN_PROC_BROWSER_TEST_P(GlobalBrowserCollectionTestWithOrder,
                       ForEachResilientToBrowserDestruction) {
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
    ::testing::Values(BrowserCollection::Order::kCreation
// TODO(crbug.com/477251911): Enable this on Android once we implement
// activation tracking in GlobalBrowserCollection for Android.
#if !BUILDFLAG(IS_ANDROID)
                      ,
                      BrowserCollection::Order::kActivation
#endif  // !BUILDFLAG(IS_ANDROID)
                      ),
    [](const testing::TestParamInfo<BrowserCollection::Order>& param) {
      switch (param.param) {
        case BrowserCollection::Order::kCreation:
          return "CreationOrder";
        case BrowserCollection::Order::kActivation:
          return "ActivationOrder";
      }
    });
