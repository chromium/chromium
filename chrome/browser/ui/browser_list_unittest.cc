// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_list.h"

#include <memory>

#include "chrome/browser/ui/browser_finder.h"
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
  EXPECT_EQ(browser(), browser_list->GetLastActive());

  // Create a minimized browser window. It should be prepended to the active
  // list, so browser() should still be at the end of the list.
  Browser::CreateParams native_params(profile(), true);
  native_params.initial_show_state = ui::SHOW_STATE_MINIMIZED;
  std::unique_ptr<Browser> browser2(
      CreateBrowserWithTestWindowForParams(native_params));
  EXPECT_EQ(2U, browser_list->size());
  EXPECT_EQ(browser(), browser_list->GetLastActive());
}

// This tests that inactive windows do not get added to the active list.
TEST_F(BrowserListUnitTest, TestInactive) {
  const BrowserList* browser_list = BrowserList::GetInstance();
  EXPECT_EQ(1U, browser_list->size());
  EXPECT_EQ(browser(), browser_list->GetLastActive());

  // Create an inactive browser window. It should be prepended to
  // |BrowserList::browsers_ordered_by_activation_| so the default browser
  // should still be the last active.
  Browser::CreateParams native_params(profile(), true);
  native_params.initial_show_state = ui::SHOW_STATE_INACTIVE;
  std::unique_ptr<Browser> browser2(
      CreateBrowserWithTestWindowForParams(native_params));
  EXPECT_EQ(2U, browser_list->size());
  EXPECT_EQ(browser(), browser_list->GetLastActive());
}

// This tests if the browser list is returning the correct browser reference
// for the context provided as input.
TEST_F(BrowserListUnitTest, TestFindBrowserWithUiElementContext) {
  const BrowserList* browser_list = BrowserList::GetInstance();
  EXPECT_EQ(1U, browser_list->size());

  Browser* result = chrome::FindBrowserWithUiElementContext(
      browser_list->get(0)->window()->GetElementContext());
  EXPECT_EQ(browser_list->get(0), result);

  Browser::CreateParams native_params(profile(), true);
  std::unique_ptr<Browser> browser2(
      CreateBrowserWithTestWindowForParams(native_params));
  auto* browser_window2 = static_cast<TestBrowserWindow*>(browser2->window());
  browser_window2->set_element_context(ui::ElementContext(2));

  result = chrome::FindBrowserWithUiElementContext(
      browser2->window()->GetElementContext());
  EXPECT_EQ(browser2.get(), result);

  result = chrome::FindBrowserWithUiElementContext(ui::ElementContext(100));
  EXPECT_EQ(nullptr, result);
}
