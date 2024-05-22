// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_actions.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "ui/base/ui_base_features.h"

class BrowserActionsTest : public BrowserWithTestWindowTest {
 public:
  BrowserActionsTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/std::vector<
            base::test::FeatureRef>{features::kSidePanelPinning,
                                    features::kToolbarPinning},
        /*disabled_features=*/{});
  }

  BrowserActionsTest(const BrowserActionsTest&) = delete;
  BrowserActionsTest& operator=(const BrowserActionsTest&) = delete;
  ~BrowserActionsTest() override {}

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(BrowserActionsTest, DidCreateBrowserActions) {
  BrowserActions* browser_actions = browser()->browser_actions();
  auto& action_manager = actions::ActionManager::GetForTesting();

  std::vector<actions::ActionId> browser_action_ids = {
      kActionNewIncognitoWindow, kActionPrint,    kActionClearBrowsingData,
      kActionTaskManager,        kActionDevTools, kActionSendTabToSelf};

  ASSERT_NE(browser_actions->root_action_item(), nullptr);

  for (actions::ActionId action_id : browser_action_ids) {
    EXPECT_NE(action_manager.FindAction(action_id), nullptr);
  }
}

TEST_F(BrowserActionsTest, CheckBrowserActionsEnabledState) {
  BrowserActions* browser_actions = browser()->browser_actions();
  auto& action_manager = actions::ActionManager::GetForTesting();

  ASSERT_NE(browser_actions->root_action_item(), nullptr);

  EXPECT_EQ(action_manager.FindAction(kActionNewIncognitoWindow)->GetEnabled(),
            true);
  EXPECT_EQ(action_manager.FindAction(kActionClearBrowsingData)->GetEnabled(),
            true);
  EXPECT_EQ(action_manager.FindAction(kActionTaskManager)->GetEnabled(), true);
  EXPECT_EQ(action_manager.FindAction(kActionDevTools)->GetEnabled(), true);
  EXPECT_EQ(action_manager.FindAction(kActionPrint)->GetEnabled(),
            chrome::CanPrint(browser()));
  EXPECT_EQ(action_manager.FindAction(kActionSendTabToSelf)->GetEnabled(),
            chrome::CanSendTabToSelf(browser()));
}
