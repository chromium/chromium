// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sessions/session_tab_group_utils.h"

#include <memory>
#include <optional>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace session_tab_group_utils {

class SessionTabGroupUtilsTest : public testing::Test {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        {features::kTabGroupsFocusing, features::kNonGroupFocus}, {});
    tab_strip_model_ = std::make_unique<TabStripModel>(&delegate_, &profile_);
    for (int i = 0; i < 3; ++i) {
      tab_strip_model_->AppendWebContents(
          content::WebContentsTester::CreateTestWebContents(&profile_,
                                                            nullptr),
          /*foreground=*/true);
    }
    tab_strip_model_->ForgetAllOpeners();
  }

  void TearDown() override {
    tab_strip_model_->CloseAllTabs();
    tab_strip_model_.reset();
  }

  TabStripModel& tab_strip_model() { return *tab_strip_model_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  TestingProfile profile_;
  const tabs::TabModel::PreventFeatureInitializationForTesting prevent_;
  TestTabStripModelDelegate delegate_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
};

// Ephemeral groups must never be persisted to session storage; regular and
// promoted groups may be.
TEST_F(SessionTabGroupUtilsTest, PersistableTabGroup) {
  const tab_groups::TabGroupId regular_group =
      tab_strip_model().AddToNewGroup({0});
  const tab_groups::TabGroupId ephemeral_group =
      tab_strip_model().EnterFocusMode({1});

  EXPECT_TRUE(IsPersistableTabGroup(tab_strip_model(), regular_group));
  EXPECT_FALSE(IsPersistableTabGroup(tab_strip_model(), ephemeral_group));
  // A group that is not in the model cannot be persisted.
  EXPECT_FALSE(IsPersistableTabGroup(tab_strip_model(),
                                     tab_groups::TabGroupId::GenerateNew()));
  EXPECT_EQ(std::nullopt, GetPersistableFocusedGroup(tab_strip_model()));

  EXPECT_EQ(regular_group, GetPersistableTabGroupForTab(tab_strip_model(), 0));
  EXPECT_EQ(std::nullopt, GetPersistableTabGroupForTab(tab_strip_model(), 1));
  EXPECT_EQ(std::nullopt, GetPersistableTabGroupForTab(tab_strip_model(), 2));
  // An out-of-range index has no group.
  EXPECT_EQ(std::nullopt, GetPersistableTabGroupForTab(tab_strip_model(), 3));

  tab_strip_model().PromoteEphemeralTabGroup(ephemeral_group);
  EXPECT_TRUE(IsPersistableTabGroup(tab_strip_model(), ephemeral_group));
  EXPECT_EQ(ephemeral_group, GetPersistableFocusedGroup(tab_strip_model()));
  EXPECT_EQ(ephemeral_group,
            GetPersistableTabGroupForTab(tab_strip_model(), 1));
}

}  // namespace session_tab_group_utils
