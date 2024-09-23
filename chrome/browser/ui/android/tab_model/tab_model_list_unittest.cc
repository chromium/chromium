// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_model_list.h"

#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class TabModelListTest : public ChromeRenderViewHostTestHarness {};
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
  tab_model.SetTabCount(1);
  EXPECT_EQ(NULL, TabModelList::GetTabModelForWebContents(contents.get()));

  TabModelList::RemoveTabModel(&tab_model);
}
