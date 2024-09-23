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
  BrowserActionsTest() = default;

 private:
  base::test::ScopedFeatureList feature_list_{features::kToolbarPinning};
};

TEST_F(BrowserActionsTest, DidCreateBrowserActions) {
  BrowserActions* browser_actions = browser()->browser_actions();
  auto& action_manager = actions::ActionManager::GetForTesting();

  std::vector<actions::ActionId> browser_action_ids = {
      kActionNewIncognitoWindow, kActionPrint,
      kActionClearBrowsingData,  kActionTaskManager,
      kActionDevTools,           kActionSendTabToSelf,
      kActionQrCodeGenerator,    kActionShowAddressesBubbleOrPage};

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
  EXPECT_EQ(action_manager.FindAction(kActionQrCodeGenerator)->GetEnabled(),
            false);
  EXPECT_EQ(
      action_manager.FindAction(kActionShowAddressesBubbleOrPage)->GetEnabled(),
      true);
}

TEST_F(BrowserActionsTest, GetCleanTitleAndTooltipText) {
  // \u2026 is the unicode hex value for a horizontal ellipsis.
  const std::u16string expected = u"Print";
  std::u16string input = u"&Print\u2026";
  std::u16string output = BrowserActions::GetCleanTitleAndTooltipText(input);
  EXPECT_EQ(output, expected);

  std::u16string input_middle_amp = u"Pri&nt\u2026";
  std::u16string output_middle_amp =
      BrowserActions::GetCleanTitleAndTooltipText(input_middle_amp);
  EXPECT_EQ(output_middle_amp, expected);

  std::u16string input_ellipsis_text = u"&Print...";
  std::u16string output_ellipsis_text =
      BrowserActions::GetCleanTitleAndTooltipText(input_ellipsis_text);
  EXPECT_EQ(output_ellipsis_text, expected);
}
