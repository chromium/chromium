// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class TabModelListTest : public ChromeRenderViewHostTestHarness {};

class TestTabModel : public TabModel {
 public:
  explicit TestTabModel(Profile* profile)
      : TabModel(profile, false), tab_count_(0) {}

  int GetTabCount() const override { return tab_count_; }
  int GetActiveIndex() const override { return 0; }
  content::WebContents* GetWebContentsAt(int index) const override {
    return nullptr;
  }
  void CreateTab(TabAndroid* parent,
                 content::WebContents* web_contents) override {}
  void HandlePopupNavigation(TabAndroid* parent,
                             NavigateParams* params) override {}
  content::WebContents* CreateNewTabForDevTools(const GURL& url) override {
    return nullptr;
  }
  bool IsSessionRestoreInProgress() const override { return false; }
  bool IsCurrentModel() const override { return false; }
  TabAndroid* GetTabAt(int index) const override { return nullptr; }
  void SetActiveIndex(int index) override {}
  void CloseTabAt(int index) override {}
  void AddObserver(TabModelObserver* observer) override {}
  void RemoveObserver(TabModelObserver* observer) override {}

  // A fake value for the current number of tabs.
  int tab_count_;
};
}  // namespace

// Regression test for http://crbug.com/432685.
TEST_F(TabModelListTest, TestGetTabModelForWebContents) {
  TestTabModel tab_model(profile());
  TabModelList::AddTabModel(&tab_model);

  std::unique_ptr<content::WebContents> contents(CreateTestWebContents());

  // Should not crash when there are no tabs.
  EXPECT_EQ(NULL, TabModelList::GetTabModelForWebContents(contents.get()));

  // Should not crash when there is an uninitialized tab, i.e. when
  // TabModel::GetTabAt returns NULL.
  tab_model.tab_count_ = 1;
  EXPECT_EQ(NULL, TabModelList::GetTabModelForWebContents(contents.get()));

  TabModelList::RemoveTabModel(&tab_model);
}
