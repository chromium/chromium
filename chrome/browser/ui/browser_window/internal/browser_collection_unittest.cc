// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/browser_collection.h"

#include <algorithm>

#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestBrowserCollection : public BrowserCollection {
 public:
  TestBrowserCollection() = default;
  ~TestBrowserCollection() override = default;

  // BrowserCollection:
  bool IsEmpty() const override { return browsers_.empty(); }
  size_t GetSize() const override { return browsers_.size(); }
  BrowserVector GetBrowsers(Order order) override {
    if (order == Order::kActivation) {
      BrowserVector reversed = browsers_;
      std::reverse(reversed.begin(), reversed.end());
      return reversed;
    }
    return browsers_;
  }

  void SetBrowsers(BrowserVector browsers) { browsers_ = std::move(browsers); }

  void NotifyBrowserClosed(BrowserWindowInterface* browser) {
    for (auto& observer : observers()) {
      observer.OnBrowserClosed(browser);
    }
  }

  void NotifyBrowserCreated(BrowserWindowInterface* browser) {
    browsers_.push_back(browser);
    for (auto& observer : observers()) {
      observer.OnBrowserCreated(browser);
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
  EXPECT_FALSE(collection.IsEmpty());
  EXPECT_EQ(collection.GetSize(), 2u);

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
  EXPECT_FALSE(collection.IsEmpty());
  EXPECT_EQ(collection.GetSize(), 2u);

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
  EXPECT_FALSE(collection.IsEmpty());
  EXPECT_EQ(collection.GetSize(), 3u);

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

TEST_F(BrowserCollectionTest, ForEachEnumeratesNewBrowsers) {
  TestBrowserCollection collection;
  MockBrowserWindowInterface browser1;
  MockBrowserWindowInterface browser2;
  collection.SetBrowsers({&browser1});
  EXPECT_FALSE(collection.IsEmpty());
  EXPECT_EQ(collection.GetSize(), 1u);

  std::vector<BrowserWindowInterface*> visited;
  collection.ForEach(
      [&](BrowserWindowInterface* browser) {
        visited.push_back(browser);
        if (browser == &browser1) {
          // Create browser2 mid-iteration.
          collection.NotifyBrowserCreated(&browser2);
        }
        return true;
      },
      BrowserCollection::Order::kCreation,
      /*enumerate_new_browsers=*/true);

  // Should visit browser1 and browser2.
  EXPECT_EQ(visited.size(), 2u);
  EXPECT_EQ(visited[0], &browser1);
  EXPECT_EQ(visited[1], &browser2);
}

TEST_F(BrowserCollectionTest, ForEachEnumeratesNewBrowsersActivationOrder) {
  TestBrowserCollection collection;
  MockBrowserWindowInterface browser1;
  MockBrowserWindowInterface browser2;
  MockBrowserWindowInterface browser3;
  collection.SetBrowsers({&browser1, &browser2});
  EXPECT_FALSE(collection.IsEmpty());
  EXPECT_EQ(collection.GetSize(), 2u);

  std::vector<BrowserWindowInterface*> visited;
  collection.ForEach(
      [&](BrowserWindowInterface* browser) {
        visited.push_back(browser);
        if (browser == &browser1) {
          // Create browser3 mid-iteration.
          collection.NotifyBrowserCreated(&browser3);
        }
        return true;
      },
      BrowserCollection::Order::kActivation,
      /*enumerate_new_browsers=*/true);

  // Since kActivation order reverses the browsers in the test collection,
  // we should visit browser2, then browser1, and finally the newly created
  // browser3.
  EXPECT_EQ(visited.size(), 3u);
  EXPECT_EQ(visited[0], &browser2);
  EXPECT_EQ(visited[1], &browser1);
  EXPECT_EQ(visited[2], &browser3);
}

TEST_F(BrowserCollectionTest, ForEachDoesNotEnumerateNewBrowsersByDefault) {
  TestBrowserCollection collection;
  MockBrowserWindowInterface browser1;
  MockBrowserWindowInterface browser2;
  collection.SetBrowsers({&browser1});
  EXPECT_FALSE(collection.IsEmpty());
  EXPECT_EQ(collection.GetSize(), 1u);

  std::vector<BrowserWindowInterface*> visited;
  collection.ForEach(
      [&](BrowserWindowInterface* browser) {
        visited.push_back(browser);
        if (browser == &browser1) {
          // Create browser2 mid-iteration.
          collection.NotifyBrowserCreated(&browser2);
        }
        return true;
      },
      BrowserCollection::Order::kCreation);

  // Should visit browser1 but NOT browser2.
  EXPECT_EQ(visited.size(), 1u);
  EXPECT_EQ(visited[0], &browser1);
}
