// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"

#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/browser_window/test/browser_event_waiter.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/base_window.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_window/test/android/browser_window_android_browsertest_base.h"
#define TestBase BrowserWindowAndroidBrowserTestBase
#else
#include "chrome/test/base/in_process_browser_test.h"
#define TestBase InProcessBrowserTest
#endif  // BUILDFLAG(IS_ANDROID)

// Fixture that sets up 3 browsers in the main profile, and 2 others in separate
// profiles (if multi-profile is enabled).
class ProfileBrowserCollectionTest
    : public TestBase,
      public testing::WithParamInterface<BrowserCollection::Order> {
 protected:
  // TestBase:
  void SetUpOnMainThread() override {
    TestBase::SetUpOnMainThread();

    primary_browser_ = GetLastActiveBrowserWindowInterfaceWithAnyProfile();
    primary_profile_ = primary_browser_->GetProfile();

    // Browsers are activated in the order they are created, resulting in an
    // activation order the reverse of creation order.
    browsers_.push_back(GetPrimaryBrowser());
    // Ensure the initial browser is active. We need to explicitly activate the
    // browsers because some features (e.g. WebUIReloadButton) might affect the
    // window activation during creation, making the activation order
    // non-deterministic if not explicitly set.
    browsers_.back()->GetWindow()->Activate();

    // The browser created here will not show up in the lists in the test cases,
    // because it's in a different profile.
    if (profiles::IsMultipleProfilesEnabled()) {
      CreateAndActivateBrowserWithNewProfile();
    }

    browsers_.push_back(CreateAndActivateBrowser(GetPrimaryProfile()));

    // On Android, this step triggers the actual creation of the
    // ProfileBrowserCollection. It's important to do this here in the middle so
    // that it tests that we receive browsers that open both before and after
    // the ProfileBrowserCollection exists.
    const auto* profile_collection =
        ProfileBrowserCollection::GetForProfile(GetPrimaryProfile());

    // The browser created here will not show up in the lists in the test cases,
    // because it's in a different profile.
    if (profiles::IsMultipleProfilesEnabled()) {
      CreateAndActivateBrowserWithNewProfile();
    }

    browsers_.push_back(CreateAndActivateBrowser(GetPrimaryProfile()));

    EXPECT_FALSE(profile_collection->IsEmpty());
    EXPECT_EQ(profile_collection->GetSize(), 3u);
  }
  void TearDownOnMainThread() override {
    browsers_.clear();
    primary_profile_ = nullptr;
    primary_browser_ = nullptr;
    TestBase::TearDownOnMainThread();
  }

  BrowserWindowInterface* CreateAndActivateBrowser(Profile* profile) {
    BrowserWindowInterface* browser;
    {
      BrowserEventWaiter browser_created_waiter(
          BrowserEventWaiter::Event::CREATED);
      browser = CreateBrowserWindow(
          BrowserWindowCreateParams(*profile, /*from_user_gesture=*/true));
    }
    browser->GetWindow()->Activate();
    return browser;
  }

  BrowserWindowInterface* CreateAndActivateBrowserWithNewProfile() {
    ProfileManager* const profile_manager =
        g_browser_process->profile_manager();
    return CreateAndActivateBrowser(&profiles::testing::CreateProfileSync(
        profile_manager, profile_manager->GenerateNextProfileDirectoryPath()));
  }

  BrowserWindowInterface* GetPrimaryBrowser() { return primary_browser_.get(); }
  Profile* GetPrimaryProfile() { return primary_profile_.get(); }
  BrowserWindowInterface* GetBrowser(int index) { return browsers_.at(index); }

  BrowserWindowInterface* GetAndClearBrowser(int index) {
    BrowserWindowInterface* tmp = browsers_.at(index);
    browsers_.at(index) = nullptr;
    return tmp;
  }

  void CloseBrowserSynchronouslyCrossPlatform(BrowserWindowInterface* browser) {
    BrowserEventWaiter browser_closed_waiter(BrowserEventWaiter::Event::CLOSED);
#if BUILDFLAG(IS_ANDROID)
    browser->GetWindow()->Close();
#else
    // TODO(crbug.com/478908209): I don't know why, but
    // `browser->GetWindow()->Close();` doesn't seem to trigger
    // GlobalBrowserCollection::OnBrowserClosed() on desktop.
    CloseBrowserSynchronously(browser);
#endif  // BUILDFLAG(IS_ANDROID)
  }

 private:
  raw_ptr<BrowserWindowInterface> primary_browser_ = nullptr;
  raw_ptr<Profile> primary_profile_ = nullptr;

  // Browser instances in creation order.
  std::vector<raw_ptr<BrowserWindowInterface>> browsers_;
};

// TODO(crbug.com/477251911): Enable this on Android once we implement
// ProfileBrowserCollection for Android.
//
// TODO(crbug.com/483366391): Enable this on ChromeOS once the
// BaseWindow::Activate() behaviour is fixed.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ForEachIteratesOverAllBrowsers \
  DISABLED_ForEachIteratesOverAllBrowsers
#else
#define MAYBE_ForEachIteratesOverAllBrowsers ForEachIteratesOverAllBrowsers
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_P(ProfileBrowserCollectionTest,
                       MAYBE_ForEachIteratesOverAllBrowsers) {
  std::vector<BrowserWindowInterface*> visited;
  ProfileBrowserCollection::GetForProfile(GetPrimaryProfile())
      ->ForEach(
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

// TODO(crbug.com/477251911): Enable this on Android once we implement
// ProfileBrowserCollection for Android.
//
// TODO(crbug.com/483366391): Enable this on ChromeOS once the
// BaseWindow::Activate() behaviour is fixed.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ForEachStopsWhenCallbackReturnsFalse \
  DISABLED_ForEachStopsWhenCallbackReturnsFalse
#else
#define MAYBE_ForEachStopsWhenCallbackReturnsFalse \
  ForEachStopsWhenCallbackReturnsFalse
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_P(ProfileBrowserCollectionTest,
                       MAYBE_ForEachStopsWhenCallbackReturnsFalse) {
  std::vector<BrowserWindowInterface*> visited;
  ProfileBrowserCollection::GetForProfile(GetPrimaryProfile())
      ->ForEach(
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

// TODO(crbug.com/477251911): Enable this on Android once we implement
// ProfileBrowserCollection for Android.
//
// TODO(crbug.com/483366391): Enable this on ChromeOS once the
// BaseWindow::Activate() behaviour is fixed.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ForEachResilientToBrowserDestruction \
  DISABLED_ForEachResilientToBrowserDestruction
#else
#define MAYBE_ForEachResilientToBrowserDestruction \
  ForEachResilientToBrowserDestruction
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_P(ProfileBrowserCollectionTest,
                       MAYBE_ForEachResilientToBrowserDestruction) {
  std::vector<BrowserWindowInterface*> visited;
  ProfileBrowserCollection::GetForProfile(GetPrimaryProfile())
      ->ForEach(
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
    ProfileBrowserCollectionTest,
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
