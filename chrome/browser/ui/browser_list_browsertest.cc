// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/mojom/window_show_state.mojom.h"

using BrowserListBrowserTest = InProcessBrowserTest;

// This tests that minimized windows get added to the active list, at the front
// the list.
IN_PROC_BROWSER_TEST_F(BrowserListBrowserTest, TestMinimized) {
  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());
  EXPECT_EQ(browser(), GetLastActiveBrowserWindowInterfaceWithAnyProfile());

  // Create a minimized browser window. It should be prepended to the active
  // list, so browser() should still be at the end of the list.
  Browser::CreateParams params(GetProfile(), true);
  params.initial_show_state = ui::mojom::WindowShowState::kMinimized;
  Browser::Create(params);
  EXPECT_EQ(2U, chrome::GetTotalBrowserCount());
  EXPECT_EQ(browser(), GetLastActiveBrowserWindowInterfaceWithAnyProfile());
}

// This tests that inactive windows do not get added to the active list.
IN_PROC_BROWSER_TEST_F(BrowserListBrowserTest, TestInactive) {
  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());
  EXPECT_EQ(browser(), GetLastActiveBrowserWindowInterfaceWithAnyProfile());

  // Create an inactive browser window. It should be prepended to
  // |BrowserList::browsers_ordered_by_activation_| so the default browser
  // should still be the last active.
  Browser::CreateParams params(GetProfile(), true);
  params.initial_show_state = ui::mojom::WindowShowState::kInactive;
  Browser::Create(params);
  EXPECT_EQ(2U, chrome::GetTotalBrowserCount());
  EXPECT_EQ(browser(), GetLastActiveBrowserWindowInterfaceWithAnyProfile());
}

// This tests if the browser list is returning the correct browser reference
// for the context provided as input.
IN_PROC_BROWSER_TEST_F(BrowserListBrowserTest,
                       TestFindBrowserWithUiElementContext) {
  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());

  BrowserWindowInterface* const last_active_browser =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();
  Browser* result = chrome::FindBrowserWithUiElementContext(
      BrowserElements::From(last_active_browser)->GetContext());
  EXPECT_EQ(last_active_browser, result);

  BrowserWindowInterface* const browser2 =
      Browser::Create(Browser::CreateParams(GetProfile(), true));
  ASSERT_EQ(2U, chrome::GetTotalBrowserCount());
  result = chrome::FindBrowserWithUiElementContext(
      BrowserElements::From(browser2)->GetContext());
  EXPECT_EQ(browser2, result);

  result = chrome::FindBrowserWithUiElementContext(
      ui::ElementContext::CreateFakeContextForTesting(100));
  EXPECT_EQ(nullptr, result);
}

// Class that tries to observe all pre-existing and newly created browsers.
// Ensures that for each browser there is a single OnBrowserCreated/Closed call
// or it already existed in GlobalBrowserCollection.
class BrowserObserverChild : public BrowserCollectionObserver,
                             TabStripModelObserver {
 public:
  explicit BrowserObserverChild(BrowserWindowInterface* created_for_browser)
      : created_for_browser_(created_for_browser) {
    GlobalBrowserCollection::GetInstance()->ForEach(
        [this](BrowserWindowInterface* browser) {
          EXPECT_FALSE(observed_browsers_.contains(browser));
          observed_browsers_.insert(browser);
          // TODO(crbug.com/452120900): TabStripModel auto-unregistered by dtor
          browser->GetTabStripModel()->AddObserver(this);
          return true;
        });
    EXPECT_TRUE(observed_browsers_.contains(created_for_browser_));
    browser_collection_observation_.Observe(
        GlobalBrowserCollection::GetInstance());
  }

  ~BrowserObserverChild() override = default;

  void OnBrowserCreated(BrowserWindowInterface* browser) override {
    // Skip browsers already tracked via ForEach in constructor.
    // GlobalBrowserCollection may re-dispatch the current OnBrowserCreated
    // event to observers added during notification dispatch.
    if (observed_browsers_.contains(browser)) {
      return;
    }
    observed_browsers_.insert(browser);
    // TODO(crbug.com/452120900): TabStripModel auto-unregistered by dtor
    browser->GetTabStripModel()->AddObserver(this);
  }

  void OnBrowserClosed(BrowserWindowInterface* browser) override {
    EXPECT_TRUE(observed_browsers_.contains(browser));
    observed_browsers_.erase(browser);
  }

 private:
  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
  std::set<raw_ptr<BrowserWindowInterface, SetExperimental>> observed_browsers_;
  raw_ptr<BrowserWindowInterface> created_for_browser_ = nullptr;
};

// Class that creates BrowserObserverChild when a Browser is created;
class BrowserObserverParent : public BrowserCollectionObserver {
 public:
  BrowserObserverParent() {
    browser_collection_observation_.Observe(
        GlobalBrowserCollection::GetInstance());
  }

  void OnBrowserCreated(BrowserWindowInterface* browser) override {
    if (!child_observer_) {
      child_observer_ = std::make_unique<BrowserObserverChild>(browser);
    }
  }

  ~BrowserObserverParent() override = default;

 protected:
  std::unique_ptr<BrowserObserverChild> child_observer_;

 private:
  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
};

IN_PROC_BROWSER_TEST_F(BrowserListBrowserTest, ObserverAddedInFlight) {
  BrowserObserverParent parent_observer;

  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());

  // Adding second browser should not trigger double-observation.
  Browser::Create(Browser::CreateParams(GetProfile(), true));
  EXPECT_EQ(2U, chrome::GetTotalBrowserCount());

  // Create one more browser to trigger BrowserObserverChild::OnBrowserCreated.
  Browser::Create(Browser::CreateParams(GetProfile(), true));
  EXPECT_EQ(3U, chrome::GetTotalBrowserCount());
}
