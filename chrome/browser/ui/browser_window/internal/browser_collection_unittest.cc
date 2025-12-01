// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/browser_collection.h"

#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestBrowserCollection : public BrowserCollection {
 public:
  TestBrowserCollection() = default;
  ~TestBrowserCollection() override = default;

  // BrowserCollection:
  BrowserVector GetBrowsers(Order order) override { return browsers_; }

  void SetBrowsers(BrowserVector browsers) { browsers_ = std::move(browsers); }

  void NotifyBrowserClosed(BrowserWindowInterface* browser) {
    for (auto& observer : observers()) {
      observer.OnBrowserClosed(browser);
    }
  }

 private:
  BrowserVector browsers_;
};

}  // namespace

using BrowserCollectionTest = testing::Test;

TEST_F(BrowserCollectionTest, ForEachIteratesOverAllBrowsers) {
  TestBrowserCollection collection;
  MockBrowserWindowInterface browser1;
  MockBrowserWindowInterface browser2;
  collection.SetBrowsers({&browser1, &browser2});

  std::vector<BrowserWindowInterface*> visited;
  collection.ForEach(
      [&](BrowserWindowInterface* browser) {
        visited.push_back(browser);
        return true;
      },
      BrowserCollection::Order::kCreation);

  EXPECT_EQ(visited.size(), 2u);
  EXPECT_EQ(visited[0], &browser1);
  EXPECT_EQ(visited[1], &browser2);
}

TEST_F(BrowserCollectionTest, ForEachStopsWhenCallbackReturnsFalse) {
  TestBrowserCollection collection;
  MockBrowserWindowInterface browser1;
  MockBrowserWindowInterface browser2;
  collection.SetBrowsers({&browser1, &browser2});

  std::vector<BrowserWindowInterface*> visited;
  collection.ForEach(
      [&](BrowserWindowInterface* browser) {
        visited.push_back(browser);
        return false;
      },
      BrowserCollection::Order::kCreation);

  EXPECT_EQ(visited.size(), 1u);
  EXPECT_EQ(visited[0], &browser1);
}

TEST_F(BrowserCollectionTest, ForEachResilientToBrowserDestruction) {
  TestBrowserCollection collection;
  MockBrowserWindowInterface browser1;
  MockBrowserWindowInterface browser2;
  MockBrowserWindowInterface browser3;
  collection.SetBrowsers({&browser1, &browser2, &browser3});

  std::vector<BrowserWindowInterface*> visited;
  collection.ForEach(
      [&](BrowserWindowInterface* browser) {
        visited.push_back(browser);
        if (browser == &browser1) {
          // Destroy browser2 mid-iteration.
          collection.NotifyBrowserClosed(&browser2);
        }
        return true;
      },
      BrowserCollection::Order::kCreation);

  // Should visit browser1, skip browser2 (because it was closed), and visit
  // browser3.
  EXPECT_EQ(visited.size(), 2u);
  EXPECT_EQ(visited[0], &browser1);
  EXPECT_EQ(visited[1], &browser3);
}
