// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_list.h"

#include <memory>
#include <set>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "ui/base/mojom/window_show_state.mojom.h"

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
  native_params.initial_show_state = ui::mojom::WindowShowState::kMinimized;
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
  native_params.initial_show_state = ui::mojom::WindowShowState::kInactive;
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

// Class that tries to observe all pre-existing and newly created browsers.
// Ensures that for each browser there is a single OnBrowserAdded/Removed call
// or it already existed in BrowserList.
class BrowserObserverChild : public BrowserListObserver, TabStripModelObserver {
 public:
  explicit BrowserObserverChild(Browser* created_for_browser)
      : created_for_browser_(created_for_browser) {
    BrowserList* browser_list = BrowserList::GetInstance();
    for (Browser* browser : *browser_list) {
      EXPECT_FALSE(base::Contains(observed_browsers_, browser));
      observed_browsers_.insert(browser);
      browser->tab_strip_model()->AddObserver(this);
    }
    EXPECT_TRUE(base::Contains(observed_browsers_, created_for_browser_));
    browser_list->AddObserver(this);
  }

  ~BrowserObserverChild() override {
    BrowserList* browser_list = BrowserList::GetInstance();
    for (Browser* browser : *browser_list) {
      EXPECT_TRUE(base::Contains(observed_browsers_, browser));
      observed_browsers_.erase(browser);
      browser->tab_strip_model()->RemoveObserver(this);
    }
    browser_list->RemoveObserver(this);
  }

  void OnBrowserAdded(Browser* browser) override {
    EXPECT_NE(browser, created_for_browser_);
    EXPECT_FALSE(base::Contains(observed_browsers_, browser));
    observed_browsers_.insert(browser);
    browser->tab_strip_model()->AddObserver(this);
  }

  void OnBrowserRemoved(Browser* browser) override {
    browser->tab_strip_model()->RemoveObserver(this);
    EXPECT_TRUE(base::Contains(observed_browsers_, browser));
    observed_browsers_.erase(browser);
  }

 private:
  std::set<raw_ptr<Browser, SetExperimental>> observed_browsers_;
  raw_ptr<Browser, DanglingUntriaged> created_for_browser_;
};

// Class that creates BrowserObserverChild when a Browser is created;
class BrowserObserverParent : public BrowserListObserver {
 public:
  BrowserObserverParent() { BrowserList::GetInstance()->AddObserver(this); }

  void OnBrowserAdded(Browser* browser) override {
    if (!child_observer_) {
      child_observer_ = std::make_unique<BrowserObserverChild>(browser);
    }
  }

  ~BrowserObserverParent() override {
    BrowserList::GetInstance()->RemoveObserver(this);
  }

 protected:
  std::unique_ptr<BrowserObserverChild> child_observer_;
};

TEST_F(BrowserListUnitTest, ObserverAddedInFlight) {
  BrowserObserverParent parent_observer;

  const BrowserList* browser_list = BrowserList::GetInstance();
  EXPECT_EQ(1U, browser_list->size());

  // Adding second browser should not trigger double-observation.
  Browser::CreateParams native_params(profile(), true);
  std::unique_ptr<Browser> browser2(
      CreateBrowserWithTestWindowForParams(native_params));
  EXPECT_EQ(2U, browser_list->size());

  // Create one more browser to trigger BrowserObserverChild::OnBrowserAdded.
  std::unique_ptr<Browser> browser3(
      CreateBrowserWithTestWindowForParams(native_params));
  EXPECT_EQ(3U, browser_list->size());
}
