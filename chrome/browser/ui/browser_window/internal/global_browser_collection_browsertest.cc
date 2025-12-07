// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"

#include "base/scoped_observation.h"
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

class MockBrowserCollectionObserver
    : public testing::NiceMock<BrowserCollectionObserver> {
 public:
  MOCK_METHOD(void, OnBrowserCreated, (BrowserWindowInterface * browser));
  MOCK_METHOD(void, OnBrowserClosed, (BrowserWindowInterface * browser));
  MOCK_METHOD(void, OnBrowserActivated, (BrowserWindowInterface * browser));
  MOCK_METHOD(void, OnBrowserDeactivated, (BrowserWindowInterface * browser));
};

class GlobalBrowserCollectionTest : public InProcessBrowserTest {
 protected:
  // Creates a new Profile and an associated browser. Reuses the default test
  // profile on ChromeOS as multi-profile is not supported.
  BrowserWindowInterface* CreateBrowserWithNewProfile() {
    Profile* new_profile = GetProfile();
#if !BUILDFLAG(IS_CHROMEOS)
    ProfileManager* const profile_manager =
        g_browser_process->profile_manager();
    new_profile = &profiles::testing::CreateProfileSync(
        profile_manager, profile_manager->GenerateNextProfileDirectoryPath());
#endif  // !BUILDFLAG(IS_CHROMEOS)
    return CreateBrowser(new_profile);
  }

  // TODO(crbug.com/356183782): Consider rewriting this test as an interactive
  // ui test and using ui_test_utils::BringBrowserWindowToFront() instead.
  void ActivatePrimaryBrowser(BrowserWindowInterface* const secondary_browser) {
    browser()->DidBecomeActive();
    secondary_browser->GetBrowserForMigrationOnly()->DidBecomeInactive();
  }

  void ActivateSecondaryBrowser(
      BrowserWindowInterface* const secondary_browser) {
    secondary_browser->GetBrowserForMigrationOnly()->DidBecomeActive();
    browser()->DidBecomeInactive();
  }
};

IN_PROC_BROWSER_TEST_F(GlobalBrowserCollectionTest,
                       TestObservationWithSingleProfile) {
  // Observe GlobalBrowserCollection.
  MockBrowserCollectionObserver observer;
  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      observation{&observer};
  observation.Observe(GlobalBrowserCollection::GetInstance());

  // Create secondary browser and expect events.
  EXPECT_CALL(observer, OnBrowserCreated(_)).Times(1);
  BrowserWindowInterface* const secondary_browser = CreateBrowser(GetProfile());
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
IN_PROC_BROWSER_TEST_F(GlobalBrowserCollectionTest,
                       TestObservationWithMultipleProfiles) {
  // Observe GlobalBrowserCollection.
  MockBrowserCollectionObserver observer;
  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      observation{&observer};
  observation.Observe(GlobalBrowserCollection::GetInstance());

  // Create secondary profile and browser and expect events.
  EXPECT_CALL(observer, OnBrowserCreated(_)).Times(1);
  BrowserWindowInterface* const secondary_browser =
      CreateBrowserWithNewProfile();
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
#endif  // !BUILDFLAG(IS_CHROMEOS)

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
    browsers_.push_back(browser());
    browsers_.push_back(CreateBrowserWithNewProfile());
    browsers_.push_back(CreateBrowserWithNewProfile());
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
          CloseBrowserSynchronously(GetAndClearBrowser(1));
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
