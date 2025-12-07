// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"

#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

// Fixture that sets up 3 browsers.
class ProfileBrowserCollectionTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<BrowserCollection::Order> {
 protected:
  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // Browsers are activated in the order they are created, resulting in an
    // activation order the reverse of creation order.
    browsers_.push_back(browser());
    browsers_.push_back(CreateBrowser(GetProfile()));
    browsers_.push_back(CreateBrowser(GetProfile()));
  }
  void TearDownOnMainThread() override {
    browsers_.clear();
    InProcessBrowserTest::TearDownOnMainThread();
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

IN_PROC_BROWSER_TEST_P(ProfileBrowserCollectionTest,
                       ForEachIteratesOverAllBrowsers) {
  std::vector<BrowserWindowInterface*> visited;
  ProfileBrowserCollection::GetForProfile(GetProfile())
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

IN_PROC_BROWSER_TEST_P(ProfileBrowserCollectionTest,
                       ForEachStopsWhenCallbackReturnsFalse) {
  std::vector<BrowserWindowInterface*> visited;
  ProfileBrowserCollection::GetForProfile(GetProfile())
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

IN_PROC_BROWSER_TEST_P(ProfileBrowserCollectionTest,
                       ForEachResilientToBrowserDestruction) {
  std::vector<BrowserWindowInterface*> visited;
  ProfileBrowserCollection::GetForProfile(GetProfile())
      ->ForEach(
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
