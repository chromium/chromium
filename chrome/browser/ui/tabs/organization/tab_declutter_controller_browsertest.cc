// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/tab_declutter_controller.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_source.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"

class FakeTabDeclutterObserver : public TabDeclutterObserver {
 public:
  FakeTabDeclutterObserver() = default;

  void OnStaleTabsProcessed(const std::vector<tabs::TabModel*> tabs) override {
    stale_tabs_processed_count_++;
    processed_tabs_ = tabs;
  }

  void OnTriggerDeclutterUIVisibility(bool visible) override {
    trigger_declutter_ui_visibility_count_++;
    ui_visibility_ = visible;
  }

  int stale_tabs_processed_count() const { return stale_tabs_processed_count_; }

  int trigger_declutter_ui_visibility_count() const {
    return trigger_declutter_ui_visibility_count_;
  }

  const std::vector<tabs::TabModel*>& processed_tabs() const {
    return processed_tabs_;
  }

  bool ui_visibility() const { return ui_visibility_; }

 private:
  int stale_tabs_processed_count_ = 0;
  int trigger_declutter_ui_visibility_count_ = 0;
  std::vector<tabs::TabModel*> processed_tabs_;
  bool ui_visibility_;
};

class TabDeclutterControllerBrowserTest : public InProcessBrowserTest {
 public:
  TabDeclutterControllerBrowserTest() {
    feature_list_.InitWithFeatures({features::kTabstripDeclutter}, {});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // To avoid flakes when focus changes, set the active tab strip model
    // explicitly.
    resource_coordinator::GetTabLifecycleUnitSource()
        ->SetFocusedTabStripModelForTesting(browser()->tab_strip_model());

    tab_declutter_controller_ = std::make_unique<tabs::TabDeclutterController>(
        browser()->tab_strip_model(), browser()->profile());
  }

  void TearDownOnMainThread() override {
    tab_declutter_controller_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  std::unique_ptr<content::WebContents> CreateWebContents(
      base::Time last_active_time) {
    content::WebContents::CreateParams create_params(browser()->profile());
    create_params.last_active_time = last_active_time;
    create_params.initially_hidden = true;
    return content::WebContents::Create(create_params);
  }

  std::unique_ptr<tabs::TabDeclutterController> tab_declutter_controller_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabDeclutterControllerBrowserTest,
                       TestProcessInactiveTabs) {
  FakeTabDeclutterObserver fake_observer;
  tab_declutter_controller_->AddObserver(&fake_observer);

  // Add 4 out of date tabs.
  browser()->tab_strip_model()->AppendWebContents(
      CreateWebContents(base::Time::Now() - base::Days(8)), false);
  browser()->tab_strip_model()->AppendWebContents(
      CreateWebContents(base::Time::Now() - base::Days(8)), false);
  browser()->tab_strip_model()->AppendWebContents(
      CreateWebContents(base::Time::Now() - base::Days(8)), false);
  browser()->tab_strip_model()->AppendWebContents(
      CreateWebContents(base::Time::Now() - base::Days(8)), false);

  // Make one of the tabs pinned and another grouped.
  browser()->tab_strip_model()->SetTabPinned(1, true);
  browser()->tab_strip_model()->AddToNewGroup(std::vector<int>{2});

  // Tabs at index 3 and 4 should only potentially decluttered.
  tab_declutter_controller_->ProcessStaleTabs();

  EXPECT_EQ(fake_observer.stale_tabs_processed_count(), 1);
  EXPECT_EQ(fake_observer.trigger_declutter_ui_visibility_count(), 1);

  std::vector<tabs::TabModel*> expected_stale_tabs;
  expected_stale_tabs.push_back(browser()->tab_strip_model()->GetTabAtIndex(3));
  expected_stale_tabs.push_back(browser()->tab_strip_model()->GetTabAtIndex(4));

  EXPECT_EQ(fake_observer.processed_tabs(), expected_stale_tabs);
}
