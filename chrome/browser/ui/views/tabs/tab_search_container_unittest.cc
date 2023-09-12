// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_search_container.h"

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/tab_organization_button.h"
#include "chrome/browser/ui/views/tabs/tab_search_button.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "fake_base_tab_strip_controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"

class TabSearchContainerTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    scoped_feature_list_.InitWithFeatures(
        {features::kTabOrganization, features::kChromeRefresh2023}, {});

    auto controller = std::make_unique<FakeBaseTabStripController>();
    auto tab_strip = std::make_unique<TabStrip>(std::move(controller));
    container_before_tab_strip_ =
        std::make_unique<TabSearchContainer>(tab_strip.get(), true);
    container_after_tab_strip_ =
        std::make_unique<TabSearchContainer>(tab_strip.get(), false);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TabSearchContainer> container_before_tab_strip_;
  std::unique_ptr<TabSearchContainer> container_after_tab_strip_;
};

TEST_F(TabSearchContainerTest, ShowsOrganizationButton) {
  ASSERT_TRUE(container_before_tab_strip_->tab_search_button());
  ASSERT_TRUE(container_before_tab_strip_->tab_organization_button());
}

TEST_F(TabSearchContainerTest, OrdersButtonsCorrectly) {
  ASSERT_EQ(container_before_tab_strip_->tab_search_button(),
            container_before_tab_strip_->children()[0]);
  ASSERT_EQ(container_before_tab_strip_->tab_organization_button(),
            container_before_tab_strip_->children()[1]);

  ASSERT_EQ(container_after_tab_strip_->tab_organization_button(),
            container_after_tab_strip_->children()[0]);
  ASSERT_EQ(container_after_tab_strip_->tab_search_button(),
            container_after_tab_strip_->children()[1]);
}
