// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_list.h"

#include <memory>

#include "chrome/test/base/browser_with_test_window_test.h"

class BrowserListUnitTest : public BrowserWithTestWindowTest {
 public:
  BrowserListUnitTest() = default;
  BrowserListUnitTest(const BrowserListUnitTest&) = delete;
  BrowserListUnitTest& operator=(const BrowserListUnitTest&) = delete;
  ~BrowserListUnitTest() override = default;
};

// This tests that minimized windows get added to the active list, at the front
// the list.
TEST_F(BrowserListUnitTest, TestMinimized) {
  const BrowserList* browser_list = BrowserList::GetInstance();
  EXPECT_EQ(1U, browser_list->size());
  EXPECT_FALSE(browser_list->GetLastActive());
  Browser::CreateParams native_params(profile(), true);
  native_params.initial_show_state = ui::SHOW_STATE_MINIMIZED;
  std::unique_ptr<Browser> browser2(
      CreateBrowserWithTestWindowForParams(native_params));
  EXPECT_EQ(2U, browser_list->size());
  // Because minimized windows are added at the front of the list, browser()
  // should still be at the end of the list.
  EXPECT_EQ(browser_list->GetLastActive(), browser2.get());
}

// This tests that inactive windows do not get added to the active list.
TEST_F(BrowserListUnitTest, TestInactive) {
  const BrowserList* browser_list = BrowserList::GetInstance();
  EXPECT_EQ(1U, browser_list->size());
  EXPECT_FALSE(browser_list->GetLastActive());
  Browser::CreateParams native_params(profile(), true);
  native_params.initial_show_state = ui::SHOW_STATE_INACTIVE;
  std::unique_ptr<Browser> browser2(
      CreateBrowserWithTestWindowForParams(native_params));
  EXPECT_EQ(2U, browser_list->size());
  // Because inactive windows are not added to the list, browser()
  // should still be at the end of the list.
  EXPECT_EQ(browser_list->GetLastActive(), nullptr);
}
