// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/command_updater_impl.h"
#include "chrome/browser/promo_browser_command/promo_browser_command.mojom.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/new_tab_page/promo_browser_command/promo_browser_command_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"

using promo_browser_command::mojom::ClickInfo;
using promo_browser_command::mojom::ClickInfoPtr;
using promo_browser_command::mojom::Command;
using promo_browser_command::mojom::CommandHandler;

namespace {

class TestCommandHandler : public PromoBrowserCommandHandler {
 public:
  explicit TestCommandHandler(Profile* profile)
      : PromoBrowserCommandHandler(mojo::PendingReceiver<CommandHandler>(),
                                   profile) {}
  ~TestCommandHandler() override = default;

  void NavigateToURL(const GURL&, WindowOpenDisposition) override {
    // The functionality of opening a URL is removed, as it cannot be executed
    // in a unittest.
  }

  CommandUpdater* GetCommandUpdater() override {
    if (command_updater_)
      return command_updater_.get();
    return PromoBrowserCommandHandler::GetCommandUpdater();
  }

  void SetCommandUpdater(std::unique_ptr<CommandUpdater> command_updater) {
    command_updater_ = std::move(command_updater);
    // Ensure that all commands are also updated in the new |command_updater|.
    EnableCommands();
  }

  std::unique_ptr<CommandUpdater> command_updater_;
};

class MockCommandHandler : public TestCommandHandler {
 public:
  explicit MockCommandHandler(Profile* profile) : TestCommandHandler(profile) {}
  ~MockCommandHandler() override = default;

  MOCK_METHOD2(NavigateToURL, void(const GURL&, WindowOpenDisposition));
};

class MockCommandUpdater : public CommandUpdaterImpl {
 public:
  explicit MockCommandUpdater(CommandUpdaterDelegate* delegate)
      : CommandUpdaterImpl(delegate) {}
  ~MockCommandUpdater() override = default;

  MOCK_CONST_METHOD1(IsCommandEnabled, bool(int id));
  MOCK_CONST_METHOD1(SupportsCommand, bool(int id));
};

// Callback used for testing
// PromoBrowserCommandHandler::CanShowPromoWithCommand().
void CanShowPromoWithCommandCallback(base::OnceClosure quit_closure,
                                     bool* expected_can_show,
                                     bool can_show) {
  *expected_can_show = can_show;
  std::move(quit_closure).Run();
}

// Callback used for testing PromoBrowserCommandHandler::ExecuteCommand().
void ExecuteCommandCallback(base::OnceClosure quit_closure,
                            bool* expected_command_executed,
                            bool command_executed) {
  *expected_command_executed = command_executed;
  std::move(quit_closure).Run();
}

// A shorthand for conversion between ClickInfo and WindowOpenDisposition.
WindowOpenDisposition DispositionFromClick(const ClickInfo& info) {
  return ui::DispositionFromClick(info.middle_button, info.alt_key,
                                  info.ctrl_key, info.meta_key, info.shift_key);
}

}  // namespace

class PromoBrowserCommandHandlerTest : public testing::Test {
 public:
  PromoBrowserCommandHandlerTest() = default;
  ~PromoBrowserCommandHandlerTest() override = default;

  void SetUp() override {
    command_handler_ = std::make_unique<MockCommandHandler>(&profile_);
  }

  void TearDown() override { testing::Test::TearDown(); }

  MockCommandUpdater* mock_command_updater() {
    return static_cast<MockCommandUpdater*>(
        command_handler_->GetCommandUpdater());
  }

  bool CanShowPromoWithCommand(Command command_id) {
    base::RunLoop run_loop;
    bool can_show = false;
    command_handler_->CanShowPromoWithCommand(
        command_id, base::BindOnce(&CanShowPromoWithCommandCallback,
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
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<MockCommandHandler> command_handler_;
};

TEST_F(PromoBrowserCommandHandlerTest, SupportedCommands) {
  base::HistogramTester histogram_tester;

  // Mock out the command updater to test enabling and disabling commands.
  command_handler_->SetCommandUpdater(
      std::make_unique<MockCommandUpdater>(command_handler_.get()));

  // Unsupported commands do not get executed and no histogram is logged.
  EXPECT_CALL(*mock_command_updater(),
              SupportsCommand(static_cast<int>(Command::kUnknownCommand)))
      .WillOnce(testing::Return(false));

  EXPECT_FALSE(ExecuteCommand(Command::kUnknownCommand, ClickInfo::New()));
  histogram_tester.ExpectTotalCount(
      PromoBrowserCommandHandler::kPromoBrowserCommandHistogramName, 0);

  // Disabled commands do not get executed and no histogram is logged.
  EXPECT_CALL(*mock_command_updater(),
              SupportsCommand(static_cast<int>(Command::kUnknownCommand)))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*mock_command_updater(),
              IsCommandEnabled(static_cast<int>(Command::kUnknownCommand)))
      .WillOnce(testing::Return(false));

  EXPECT_FALSE(ExecuteCommand(Command::kUnknownCommand, ClickInfo::New()));
  histogram_tester.ExpectTotalCount(
      PromoBrowserCommandHandler::kPromoBrowserCommandHistogramName, 0);

  // Only supported and enabled commands get executed for which a histogram is
  // logged.
  EXPECT_CALL(*mock_command_updater(),
              SupportsCommand(static_cast<int>(Command::kUnknownCommand)))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*mock_command_updater(),
              IsCommandEnabled(static_cast<int>(Command::kUnknownCommand)))
      .WillOnce(testing::Return(true));

  EXPECT_TRUE(ExecuteCommand(Command::kUnknownCommand, ClickInfo::New()));
  histogram_tester.ExpectBucketCount(
      PromoBrowserCommandHandler::kPromoBrowserCommandHistogramName, 0, 1);
}

TEST_F(PromoBrowserCommandHandlerTest, DisableHandlingCommands) {
  base::HistogramTester histogram_tester;

  // Disabling features::kPromoBrowserCommands prevents the commands from being
  // executed.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kPromoBrowserCommands);

  // The PromoBrowserCommandHandler instance needs to be recreated for the
  // feature to take effect.
  command_handler_ = std::make_unique<MockCommandHandler>(&profile_);

  EXPECT_FALSE(ExecuteCommand(Command::kUnknownCommand, ClickInfo::New()));
  histogram_tester.ExpectTotalCount(
      PromoBrowserCommandHandler::kPromoBrowserCommandHistogramName, 0);
}

TEST_F(PromoBrowserCommandHandlerTest, CanShowOpenSafetyCheckCommandPromo) {
  EXPECT_TRUE(CanShowPromoWithCommand(Command::kOpenSafetyCheck));
}

TEST_F(PromoBrowserCommandHandlerTest, OpenSafetyCheckCommand) {
  // The OpenSafetyCheck command opens a new settings window with the Safety
  // Check, and the correct disposition.
  ClickInfoPtr info = ClickInfo::New();
  info->middle_button = true;
  info->meta_key = true;
  EXPECT_CALL(
      *command_handler_,
      NavigateToURL(GURL(chrome::GetSettingsUrl(chrome::kSafetyCheckSubPage)),
                    DispositionFromClick(*info)));
  EXPECT_TRUE(ExecuteCommand(Command::kOpenSafetyCheck, std::move(info)));
}

TEST_F(PromoBrowserCommandHandlerTest,
       CanShowSafeBrowsingEnhancedProtectionCommandPromo_NoPolicies) {
  EXPECT_TRUE(CanShowPromoWithCommand(
      Command::kOpenSafeBrowsingEnhancedProtectionSettings));
}

TEST_F(
    PromoBrowserCommandHandlerTest,
    CanShowSafeBrowsingEnhancedProtectionCommandPromo_EnhancedProtectionEnabled) {
  TestingProfile::Builder builder;
  std::unique_ptr<TestingProfile> profile = builder.Build();
  profile->GetTestingPrefService()->SetUserPref(
      prefs::kSafeBrowsingEnhanced, std::make_unique<base::Value>(true));
  command_handler_ = std::make_unique<MockCommandHandler>(profile.get());

  EXPECT_FALSE(CanShowPromoWithCommand(
      Command::kOpenSafeBrowsingEnhancedProtectionSettings));
}

TEST_F(
    PromoBrowserCommandHandlerTest,
    CanShowSafeBrowsingEnhancedProtectionCommandPromo_HasSafeBrowsingManaged_NoProtection) {
  TestingProfile::Builder builder;
  std::unique_ptr<TestingProfile> profile = builder.Build();
  profile->GetTestingPrefService()->SetManagedPref(
      prefs::kSafeBrowsingEnabled, std::make_unique<base::Value>(false));
  profile->GetTestingPrefService()->SetManagedPref(
      prefs::kSafeBrowsingEnhanced, std::make_unique<base::Value>(false));
  command_handler_ = std::make_unique<MockCommandHandler>(profile.get());

  EXPECT_FALSE(CanShowPromoWithCommand(
      Command::kOpenSafeBrowsingEnhancedProtectionSettings));
}

TEST_F(
    PromoBrowserCommandHandlerTest,
    CanShowSafeBrowsingEnhancedProtectionCommandPromo_HasSafeBrowsingManaged_StandardProtection) {
  TestingProfile::Builder builder;
  std::unique_ptr<TestingProfile> profile = builder.Build();
  profile->GetTestingPrefService()->SetManagedPref(
      prefs::kSafeBrowsingEnabled, std::make_unique<base::Value>(true));
  profile->GetTestingPrefService()->SetManagedPref(
      prefs::kSafeBrowsingEnhanced, std::make_unique<base::Value>(false));
  command_handler_ = std::make_unique<MockCommandHandler>(profile.get());

  EXPECT_FALSE(CanShowPromoWithCommand(
      Command::kOpenSafeBrowsingEnhancedProtectionSettings));
}

TEST_F(
    PromoBrowserCommandHandlerTest,
    CanShowSafeBrowsingEnhancedProtectionCommandPromo_HasSafeBrowsingManaged_EnhancedProtection) {
  TestingProfile::Builder builder;
  std::unique_ptr<TestingProfile> profile = builder.Build();
  profile->GetTestingPrefService()->SetManagedPref(
      prefs::kSafeBrowsingEnabled, std::make_unique<base::Value>(true));
  profile->GetTestingPrefService()->SetManagedPref(
      prefs::kSafeBrowsingEnhanced, std::make_unique<base::Value>(true));
  command_handler_ = std::make_unique<MockCommandHandler>(profile.get());

  EXPECT_FALSE(CanShowPromoWithCommand(
      Command::kOpenSafeBrowsingEnhancedProtectionSettings));
}

TEST_F(PromoBrowserCommandHandlerTest,
       OpenSafeBrowsingEnhancedProtectionCommand) {
  // The kOpenSafeBrowsingEnhancedProtectionSettings command opens a new
  // settings window with the Safe Browsing settings with the Enhanced
  // Protection section expanded, and the correct disposition.
  ClickInfoPtr info = ClickInfo::New();
  info->middle_button = true;
  info->meta_key = true;
  EXPECT_CALL(
      *command_handler_,
      NavigateToURL(GURL(chrome::GetSettingsUrl(
                        chrome::kSafeBrowsingEnhancedProtectionSubPage)),
                    DispositionFromClick(*info)));
  EXPECT_TRUE(ExecuteCommand(
      Command::kOpenSafeBrowsingEnhancedProtectionSettings, std::move(info)));
}
