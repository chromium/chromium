// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/browser_command/browser_command_handler.h"

#include <memory>

#include "base/run_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/webui/resources/js/browser_command/browser_command.mojom.h"

using browser_command::mojom::ClickInfo;
using browser_command::mojom::ClickInfoPtr;
using browser_command::mojom::Command;
using browser_command::mojom::CommandHandler;

namespace {

std::vector<Command> supported_commands = {
    Command::kUnknownCommand,  // Included for SupportedCommands test
    Command::kOpenSafetyCheck,
    Command::kOpenSafeBrowsingEnhancedProtectionSettings,
    Command::kOpenFeedbackForm,
    Command::kOpenPrivacyGuide,
    Command::kStartTabGroupTutorial,
    Command::kOpenPasswordManager,
    Command::kNoOpCommand,
    Command::kOpenPerformanceSettings,
    Command::kOpenNTPAndStartCustomizeChromeTutorial,
    Command::kStartPasswordManagerTutorial,
    Command::kStartSavedTabGroupTutorial,
    Command::kOpenAISettings,
    Command::kOpenSafetyCheckFromWhatsNew,
    Command::kOpenPaymentsSettings,
    Command::KOpenHistorySearchSettings,
    Command::kShowCustomizeChromeToolbar,
};

// Callback used for testing
// BrowserCommandHandler::CanExecuteCommand().
void CanExecuteCommandCallback(base::OnceClosure quit_closure,
                               bool* can_show_out,
                               bool can_show) {
  *can_show_out = can_show;
  std::move(quit_closure).Run();
}

// Callback used for testing BrowserCommandHandler::ExecuteCommand().
void ExecuteCommandCallback(base::OnceClosure quit_closure,
                            bool* command_executed_out,
                            bool command_executed) {
  *command_executed_out = command_executed;
  std::move(quit_closure).Run();
}

}  // namespace

class BrowserCommandHandlerTest : public InProcessBrowserTest {
 public:
  BrowserCommandHandlerTest() = default;
  ~BrowserCommandHandlerTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kToolbarPinning);
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    command_handler_ = std::make_unique<BrowserCommandHandler>(
        mojo::PendingReceiver<CommandHandler>(), browser()->profile(),
        supported_commands);
  }

  bool CanExecuteCommand(Command command_id) {
    base::RunLoop run_loop;
    bool can_show = false;
    command_handler_->CanExecuteCommand(
        command_id, base::BindOnce(&CanExecuteCommandCallback,
                                   run_loop.QuitClosure(), &can_show));
    run_loop.Run();
    return can_show;
  }

  bool ExecuteCommand(Command command_id, ClickInfoPtr click_info) {
    base::RunLoop run_loop;
    bool command_executed = false;
    command_handler_->ExecuteCommand(
        command_id, std::move(click_info),
        base::BindOnce(&ExecuteCommandCallback, run_loop.QuitClosure(),
                       &command_executed));
    run_loop.Run();
    return command_executed;
  }

 protected:
  std::unique_ptr<BrowserCommandHandler> command_handler_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(BrowserCommandHandlerTest,
                       ShowCustomizeChromeToolbarCommand) {
  // Verify customize chrome is not showing in the side panel.
  std::optional<SidePanelEntryId> current_entry =
      browser()->GetFeatures().side_panel_ui()->GetCurrentEntryId();
  EXPECT_FALSE(current_entry.has_value());

  // If the active tab supports customize chrome it should
  // allow running commands.
  EXPECT_TRUE(CanExecuteCommand(Command::kShowCustomizeChromeToolbar));

  // Show customize chrome toolbar command calls show customize chrome toolbar.
  ClickInfoPtr info = ClickInfo::New();
  EXPECT_TRUE(
      ExecuteCommand(Command::kShowCustomizeChromeToolbar, std::move(info)));

  // Verify customize chrome is showing in the side panel.
  current_entry = browser()->GetFeatures().side_panel_ui()->GetCurrentEntryId();
  EXPECT_TRUE(current_entry.has_value());
  EXPECT_EQ(SidePanelEntryId::kCustomizeChrome, current_entry.value());
}
