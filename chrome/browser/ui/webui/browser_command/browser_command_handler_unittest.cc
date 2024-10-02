// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/browser_command/browser_command_handler.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/command_updater_impl.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/user_education/tutorial_identifiers.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_education/common/help_bubble_factory_registry.h"
#include "components/user_education/common/tutorial_identifier.h"
#include "components/user_education/common/tutorial_registry.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "ui/base/window_open_disposition_utils.h"
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
};

const ui::ElementContext kTestContext1(1);

class TestCommandHandler : public BrowserCommandHandler {
 public:
  explicit TestCommandHandler(Profile* profile)
      : BrowserCommandHandler(mojo::PendingReceiver<CommandHandler>(),
                              profile,
                              supported_commands) {}
  ~TestCommandHandler() override = default;

  void NavigateToEnhancedProtectionSetting() override {
    // The functionality of opening a URL is removed, as it cannot be executed
    // in a unittest.
  }

  void NavigateToURL(const GURL&, WindowOpenDisposition) override {
    // The functionality of opening a URL is removed, as it cannot be executed
    // in a unittest.
  }

  void OpenFeedbackForm() override {
    // The functionality of opening the feedback form is removed, as it cannot
    // be executed in a unittest.
  }

  void OpenPasswordManager() override {
    // The functionality of opening the password manager is removed, as it
    // cannot be executed in a unittest.
  }

  void OpenAISettings() override {
    // The functionality of opening the AI settings is removed, as it
    // cannot be executed in a unittest.
  }

  bool TutorialServiceExists() override { return tutorial_service_exists_; }

  CommandUpdater* GetCommandUpdater() override {
    if (command_updater_) {
      return command_updater_.get();
    }
    return BrowserCommandHandler::GetCommandUpdater();
  }

  void SetCommandUpdater(std::unique_ptr<CommandUpdater> command_updater) {
    command_updater_ = std::move(command_updater);
    // Ensure that all commands are also updated in the new |command_updater|.
    EnableSupportedCommands();
  }

  void SetTutorialServiceExists(bool tutorial_service_exists) {
    tutorial_service_exists_ = tutorial_service_exists;
  }

  void SetBrowserSupportsTabGroups(bool is_supported) {
    tab_groups_feature_supported_ = is_supported;
  }

  void SetDefaultSearchProviderToGoogle(bool is_google) {
    default_search_provider_is_google_ = is_google;
  }

  void SetBrowserSupportsSavedTabGroups(bool is_supported) {
    saved_tab_groups_feature_supported_ = is_supported;
  }

 protected:
  bool BrowserSupportsTabGroups() override {
    return tab_groups_feature_supported_;
  }

  bool DefaultSearchProviderIsGoogle() override {
    return default_search_provider_is_google_;
  }

  bool BrowserSupportsSavedTabGroups() override {
    return saved_tab_groups_feature_supported_;
  }

 private:
  bool tutorial_service_exists_;
  std::unique_ptr<CommandUpdater> command_updater_;

  bool tab_groups_feature_supported_ = true;
  bool default_search_provider_is_google_ = true;
  bool saved_tab_groups_feature_supported_ = true;
};

class TestTutorialService : public user_education::TutorialService {
 public:
  TestTutorialService(
      user_education::TutorialRegistry* tutorial_registry,
      user_education::HelpBubbleFactoryRegistry* help_bubble_factory_registry)
      : user_education::TutorialService(tutorial_registry,
                                        help_bubble_factory_registry) {}
  ~TestTutorialService() override = default;
  std::u16string GetBodyIconAltText(bool is_last_step) const override {
    return std::u16string();
  }

  void StartTutorial(
      user_education::TutorialIdentifier id,
      ui::ElementContext context,
      base::OnceClosure completed_callback = base::DoNothing(),
      base::OnceClosure aborted_callback = base::DoNothing(),
      base::RepeatingClosure restart_callback = base::DoNothing()) override {
    running_id_ = id;
  }

  bool IsRunningTutorial(
      std::optional<user_education::TutorialIdentifier> id) const override {
    return id.has_value() ? id == running_id_ : running_id_.has_value();
  }

 private:
  std::optional<user_education::TutorialIdentifier> running_id_;
};

class MockTutorialService : public TestTutorialService {
 public:
  MockTutorialService(
      user_education::TutorialRegistry* tutorial_registry,
      user_education::HelpBubbleFactoryRegistry* help_bubble_factory_registry)
      : TestTutorialService(tutorial_registry, help_bubble_factory_registry) {}
  ~MockTutorialService() override = default;

  MOCK_METHOD(void,
              StartTutorial,
              (user_education::TutorialIdentifier,
               ui::ElementContext,
               base::OnceClosure,
               base::OnceClosure,
               base::RepeatingClosure));
  MOCK_METHOD(void,
              LogStartedFromWhatsNewPage,
              (user_education::TutorialIdentifier, bool));
  MOCK_CONST_METHOD1(IsRunningTutorial,
                     bool(std::optional<user_education::TutorialIdentifier>));
};

class MockCommandHandler : public TestCommandHandler {
 public:
  explicit MockCommandHandler(Profile* profile) : TestCommandHandler(profile) {}
  ~MockCommandHandler() override = default;

  MOCK_METHOD(void, StartTutorial, (StartTutorialInPage::Params params));

  MOCK_METHOD(void, NavigateToEnhancedProtectionSetting, ());

  MOCK_METHOD(void, NavigateToURL, (const GURL&, WindowOpenDisposition));

  MOCK_METHOD(void, OpenFeedbackForm, ());

  MOCK_METHOD(void, OpenPasswordManager, ());

  MOCK_METHOD(void, OpenAISettings, ());
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
// BrowserCommandHandler::CanExecuteCommand().
void CanExecuteCommandCallback(base::OnceClosure quit_closure,
                               bool* expected_can_show,
                               bool can_show) {
  *expected_can_show = can_show;
  std::move(quit_closure).Run();
}

// Callback used for testing BrowserCommandHandler::ExecuteCommand().
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

class TestingChildProfile : public TestingProfile {
 public:
  bool IsChild() const override { return true; }
};

}  // namespace

class BrowserCommandHandlerTest : public testing::Test {
 public:
  BrowserCommandHandlerTest() = default;
  ~BrowserCommandHandlerTest() override = default;

  void SetUp() override {
    command_handler_ = std::make_unique<MockCommandHandler>(&profile_);
  }

  void TearDown() override { testing::Test::TearDown(); }

  MockCommandUpdater* mock_command_updater() {
    return static_cast<MockCommandUpdater*>(
        command_handler_->GetCommandUpdater());
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
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<MockCommandHandler> command_handler_;
};

TEST_F(BrowserCommandHandlerTest, SupportedCommands) {
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
      BrowserCommandHandler::kPromoBrowserCommandHistogramName, 0);

  // Disabled commands do not get executed and no histogram is logged.
  EXPECT_CALL(*mock_command_updater(),
              SupportsCommand(static_cast<int>(Command::kUnknownCommand)))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*mock_command_updater(),
              IsCommandEnabled(static_cast<int>(Command::kUnknownCommand)))
      .WillOnce(testing::Return(false));

  EXPECT_FALSE(ExecuteCommand(Command::kUnknownCommand, ClickInfo::New()));
  histogram_tester.ExpectTotalCount(
      BrowserCommandHandler::kPromoBrowserCommandHistogramName, 0);

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
      BrowserCommandHandler::kPromoBrowserCommandHistogramName, 0, 1);
}

TEST_F(BrowserCommandHandlerTest, CanExecuteCommand_OpenSafetyCheck) {
  // By default, showing the Safety Check promo is allowed.
  EXPECT_TRUE(
      CanExecuteCommand(Command::kOpenSafeBrowsingEnhancedProtectionSettings));

  // If the browser is managed, showing the Safety Check promo is not allowed.
  TestingProfile::Builder builder;
  builder.OverridePolicyConnectorIsManagedForTesting(true);
  std::unique_ptr<TestingProfile> profile = builder.Build();
  command_handler_ = std::make_unique<MockCommandHandler>(profile.get());
  EXPECT_FALSE(CanExecuteCommand(Command::kOpenSafetyCheck));
}

TEST_F(BrowserCommandHandlerTest, OpenSafetyCheckCommand) {
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

TEST_F(BrowserCommandHandlerTest, OpenSafetyCheckFromWhatsNewCommand) {
  EXPECT_TRUE(CanExecuteCommand(Command::kOpenSafetyCheckFromWhatsNew));
  // The OpenSafetyCheck command opens a new settings window with the Safety
  // Check, and the correct disposition.
  ClickInfoPtr info = ClickInfo::New();
  info->middle_button = true;
  info->meta_key = true;
  EXPECT_CALL(
      *command_handler_,
      NavigateToURL(GURL(chrome::GetSettingsUrl(chrome::kSafetyCheckSubPage)),
                    DispositionFromClick(*info)));
  EXPECT_TRUE(
      ExecuteCommand(Command::kOpenSafetyCheckFromWhatsNew, std::move(info)));
}

TEST_F(BrowserCommandHandlerTest,
       CanShowSafeBrowsingEnhancedProtectionCommandPromo_NoPolicies) {
  EXPECT_TRUE(
      CanExecuteCommand(Command::kOpenSafeBrowsingEnhancedProtectionSettings));
}

TEST_F(
    BrowserCommandHandlerTest,
    CanShowSafeBrowsingEnhancedProtectionCommandPromo_EnhancedProtectionEnabled) {
  TestingProfile::Builder builder;
  std::unique_ptr<TestingProfile> profile = builder.Build();
  profile->GetTestingPrefService()->SetUserPref(
      prefs::kSafeBrowsingEnhanced, std::make_unique<base::Value>(true));
  command_handler_ = std::make_unique<MockCommandHandler>(profile.get());

  EXPECT_FALSE(
      CanExecuteCommand(Command::kOpenSafeBrowsingEnhancedProtectionSettings));
}

TEST_F(
    BrowserCommandHandlerTest,
    CanShowSafeBrowsingEnhancedProtectionCommandPromo_HasSafeBrowsingManaged_NoProtection) {
  TestingProfile::Builder builder;
  std::unique_ptr<TestingProfile> profile = builder.Build();
  profile->GetTestingPrefService()->SetManagedPref(
      prefs::kSafeBrowsingEnabled, std::make_unique<base::Value>(false));
  profile->GetTestingPrefService()->SetManagedPref(
      prefs::kSafeBrowsingEnhanced, std::make_unique<base::Value>(false));
  command_handler_ = std::make_unique<MockCommandHandler>(profile.get());

  EXPECT_FALSE(
      CanExecuteCommand(Command::kOpenSafeBrowsingEnhancedProtectionSettings));
}

TEST_F(
    BrowserCommandHandlerTest,
    CanShowSafeBrowsingEnhancedProtectionCommandPromo_HasSafeBrowsingManaged_StandardProtection) {
  TestingProfile::Builder builder;
  std::unique_ptr<TestingProfile> profile = builder.Build();
  profile->GetTestingPrefService()->SetManagedPref(
      prefs::kSafeBrowsingEnabled, std::make_unique<base::Value>(true));
  profile->GetTestingPrefService()->SetManagedPref(
      prefs::kSafeBrowsingEnhanced, std::make_unique<base::Value>(false));
  command_handler_ = std::make_unique<MockCommandHandler>(profile.get());

  EXPECT_FALSE(
      CanExecuteCommand(Command::kOpenSafeBrowsingEnhancedProtectionSettings));
}

TEST_F(
    BrowserCommandHandlerTest,
    CanShowSafeBrowsingEnhancedProtectionCommandPromo_HasSafeBrowsingManaged_EnhancedProtection) {
  TestingProfile::Builder builder;
  std::unique_ptr<TestingProfile> profile = builder.Build();
  profile->GetTestingPrefService()->SetManagedPref(
      prefs::kSafeBrowsingEnabled, std::make_unique<base::Value>(true));
  profile->GetTestingPrefService()->SetManagedPref(
      prefs::kSafeBrowsingEnhanced, std::make_unique<base::Value>(true));
  command_handler_ = std::make_unique<MockCommandHandler>(profile.get());

  EXPECT_FALSE(
      CanExecuteCommand(Command::kOpenSafeBrowsingEnhancedProtectionSettings));
}

TEST_F(BrowserCommandHandlerTest, OpenSafeBrowsingEnhancedProtectionCommand) {
  // The kOpenSafeBrowsingEnhancedProtectionSettings command opens a new
  // settings window with the Safe Browsing settings with the Enhanced
  // Protection section expanded, and an In-product help bubble
  ClickInfoPtr info = ClickInfo::New();
  info->middle_button = true;
  info->meta_key = true;
  EXPECT_CALL(*command_handler_, NavigateToEnhancedProtectionSetting());
  EXPECT_TRUE(ExecuteCommand(
      Command::kOpenSafeBrowsingEnhancedProtectionSettings, std::move(info)));
}

TEST_F(BrowserCommandHandlerTest, OpenFeedbackFormCommand) {
  // Open feedback form command calls open feedback form.
  ClickInfoPtr info = ClickInfo::New();
  EXPECT_CALL(*command_handler_, OpenFeedbackForm());
  EXPECT_TRUE(ExecuteCommand(Command::kOpenFeedbackForm, std::move(info)));
}

TEST_F(BrowserCommandHandlerTest, CanExecuteCommand_OpenPrivacyGuide) {
  EXPECT_TRUE(CanExecuteCommand(Command::kOpenPrivacyGuide));

  // Showing the Privacy Guide promo is not allowed if the browser is managed.
  {
    TestingProfile::Builder builder;
    builder.OverridePolicyConnectorIsManagedForTesting(true);
    std::unique_ptr<TestingProfile> profile = builder.Build();
    command_handler_ = std::make_unique<MockCommandHandler>(profile.get());
    EXPECT_FALSE(CanExecuteCommand(Command::kOpenPrivacyGuide));
  }

  // Neither is it if the profile belongs to a child.
  {
    TestingChildProfile profile;
    command_handler_ = std::make_unique<MockCommandHandler>(&profile);
    EXPECT_FALSE(CanExecuteCommand(Command::kOpenPrivacyGuide));
  }
}

TEST_F(BrowserCommandHandlerTest, OpenPrivacyGuideCommand) {
  // The OpenPrivacyGuide command opens a new settings window with the Privacy
  // Guide, and the correct disposition.
  ClickInfoPtr info = ClickInfo::New();
  info->middle_button = true;
  info->meta_key = true;
  EXPECT_CALL(
      *command_handler_,
      NavigateToURL(GURL(chrome::GetSettingsUrl(chrome::kPrivacyGuideSubPage)),
                    DispositionFromClick(*info)));
  EXPECT_TRUE(ExecuteCommand(Command::kOpenPrivacyGuide, std::move(info)));
}

TEST_F(BrowserCommandHandlerTest, StartTabGroupTutorialCommand) {
  // Command cannot be executed if the tutorial service doesn't exist.
  command_handler_->SetTutorialServiceExists(false);
  EXPECT_FALSE(CanExecuteCommand(Command::kStartTabGroupTutorial));

  // Create mock service so the command can be executed.
  auto bubble_factory_registry =
      std::make_unique<user_education::HelpBubbleFactoryRegistry>();
  user_education::TutorialRegistry registry;
  MockTutorialService service(&registry, bubble_factory_registry.get());

  // Allow command to be executed.
  command_handler_->SetTutorialServiceExists(true);

  // If the browsers Tab Strip does not support tutorials, dont run the command.
  command_handler_->SetBrowserSupportsTabGroups(false);
  EXPECT_FALSE(CanExecuteCommand(Command::kStartTabGroupTutorial));

  // If the browser supports tab groups and has a tutorial service it should
  // allow running commands.
  command_handler_->SetBrowserSupportsTabGroups(true);
  EXPECT_TRUE(CanExecuteCommand(Command::kStartTabGroupTutorial));

  // The StartTabGroupTutorial command should start the tab group tutorial.
  {
    ClickInfoPtr info = ClickInfo::New();
    EXPECT_CALL(*command_handler_, StartTutorial)
        .WillOnce([&](StartTutorialInPage::Params params) {
          EXPECT_EQ(params.tutorial_id, kTabGroupTutorialId);
        });
    EXPECT_TRUE(
        ExecuteCommand(Command::kStartTabGroupTutorial, std::move(info)));
  }
}

TEST_F(BrowserCommandHandlerTest, OpenPasswordManagerCommand) {
  // By default, opening the password manager is allowed.
  EXPECT_TRUE(CanExecuteCommand(Command::kOpenPasswordManager));
  ClickInfoPtr info = ClickInfo::New();
  info->middle_button = true;
  info->meta_key = true;
  // The OpenPassswordManager command opens a new settings window with the
  // password manager and the correct disposition.
  EXPECT_CALL(*command_handler_, OpenPasswordManager());
  EXPECT_TRUE(ExecuteCommand(Command::kOpenPasswordManager, std::move(info)));
}

TEST_F(BrowserCommandHandlerTest, OpenPerformanceSettings) {
  EXPECT_TRUE(CanExecuteCommand(Command::kOpenPerformanceSettings));

  // Confirm executing the command works.
  ClickInfoPtr info = ClickInfo::New();
  info->middle_button = true;
  info->meta_key = true;
  // The OpenPerformanceSettings command opens a new settings window with the
  // performance page open.
  EXPECT_CALL(
      *command_handler_,
      NavigateToURL(GURL(chrome::GetSettingsUrl(chrome::kPerformanceSubPage)),
                    DispositionFromClick(*info)));
  EXPECT_TRUE(
      ExecuteCommand(Command::kOpenPerformanceSettings, std::move(info)));
}

TEST_F(BrowserCommandHandlerTest,
       OpenNTPAndStartCustomizeChromeTutorialCommand) {
  // Command cannot be executed if the tutorial service doesn't exist.
  command_handler_->SetTutorialServiceExists(false);
  EXPECT_FALSE(
      CanExecuteCommand(Command::kOpenNTPAndStartCustomizeChromeTutorial));

  // Create mock service so the command can be executed.
  auto bubble_factory_registry =
      std::make_unique<user_education::HelpBubbleFactoryRegistry>();
  user_education::TutorialRegistry registry;
  MockTutorialService service(&registry, bubble_factory_registry.get());

  // Allow command to be executed.
  command_handler_->SetTutorialServiceExists(true);

  // If the search provider is not set to Google, dont run the command
  command_handler_->SetDefaultSearchProviderToGoogle(false);
  EXPECT_FALSE(
      CanExecuteCommand(Command::kOpenNTPAndStartCustomizeChromeTutorial));
  command_handler_->SetDefaultSearchProviderToGoogle(true);

  // If the browser has feature enabled and google is default search
  // provider, allow running command
  EXPECT_TRUE(
      CanExecuteCommand(Command::kOpenNTPAndStartCustomizeChromeTutorial));

  // The OpenNTPAndStartCustomizeChromeTutorialCommand command should
  // start the customize chrome tutorial.
  {
    ClickInfoPtr info = ClickInfo::New();
    EXPECT_CALL(*command_handler_, StartTutorial)
        .WillOnce([&](StartTutorialInPage::Params params) {
          EXPECT_EQ(params.tutorial_id, kSidePanelCustomizeChromeTutorialId);
        });
    EXPECT_TRUE(ExecuteCommand(Command::kOpenNTPAndStartCustomizeChromeTutorial,
                               std::move(info)));
  }
}

TEST_F(BrowserCommandHandlerTest, StartPasswordManagerTutorialCommand) {
  // Command cannot be executed if the tutorial service doesn't exist.
  command_handler_->SetTutorialServiceExists(false);
  EXPECT_FALSE(CanExecuteCommand(Command::kStartPasswordManagerTutorial));

  // Create mock service so the command can be executed.
  auto bubble_factory_registry =
      std::make_unique<user_education::HelpBubbleFactoryRegistry>();
  user_education::TutorialRegistry registry;
  MockTutorialService service(&registry, bubble_factory_registry.get());

  // Allow command to be executed.
  command_handler_->SetTutorialServiceExists(true);

  EXPECT_TRUE(CanExecuteCommand(Command::kStartPasswordManagerTutorial));

  ClickInfoPtr info = ClickInfo::New();
  EXPECT_CALL(*command_handler_, StartTutorial)
      .WillOnce([&](StartTutorialInPage::Params params) {
        EXPECT_EQ(params.tutorial_id, kPasswordManagerTutorialId);
      });
  EXPECT_TRUE(
      ExecuteCommand(Command::kStartPasswordManagerTutorial, std::move(info)));

  EXPECT_CALL(service, IsRunningTutorial).WillOnce(testing::Return(true));
  EXPECT_CALL(service, LogStartedFromWhatsNewPage)
      .WillOnce(
          [&](user_education::TutorialIdentifier tutorial_id, bool is_running) {
            EXPECT_EQ(tutorial_id, kPasswordManagerTutorialId);
            EXPECT_TRUE(is_running);
            return;
          });

  // Manually call tutorial started callback.
  command_handler_->OnTutorialStarted(kPasswordManagerTutorialId, &service);
}

TEST_F(BrowserCommandHandlerTest, StartSavedTabGroupTutorialCommand) {
  // Skip test if Tab Groups Save V2 feature flag is enabled
  if (tab_groups::IsTabGroupsSaveV2Enabled()) {
    EXPECT_FALSE(CanExecuteCommand(Command::kStartSavedTabGroupTutorial));
    GTEST_SKIP();
  }

  // Command cannot be executed if the tutorial service doesn't exist.
  command_handler_->SetTutorialServiceExists(false);
  EXPECT_FALSE(CanExecuteCommand(Command::kStartSavedTabGroupTutorial));

  // Create mock service so the command can be executed.
  auto bubble_factory_registry =
      std::make_unique<user_education::HelpBubbleFactoryRegistry>();
  user_education::TutorialRegistry registry;
  MockTutorialService service(&registry, bubble_factory_registry.get());

  // Allow command to be executed.
  command_handler_->SetTutorialServiceExists(true);

  // If the browser does not support saved tab groups, dont run the command.
  command_handler_->SetBrowserSupportsSavedTabGroups(false);
  EXPECT_FALSE(CanExecuteCommand(Command::kStartSavedTabGroupTutorial));

  // If the browser supports the new password manager and has a tutorial
  // service it should allow running commands.
  command_handler_->SetBrowserSupportsSavedTabGroups(true);
  EXPECT_TRUE(CanExecuteCommand(Command::kStartSavedTabGroupTutorial));

  ClickInfoPtr info = ClickInfo::New();
  EXPECT_CALL(*command_handler_, StartTutorial)
      .WillOnce([&](StartTutorialInPage::Params params) {
        EXPECT_EQ(params.tutorial_id, kSavedTabGroupTutorialId);
      });
  EXPECT_TRUE(
      ExecuteCommand(Command::kStartSavedTabGroupTutorial, std::move(info)));

  EXPECT_CALL(service, IsRunningTutorial).WillOnce(testing::Return(true));
  EXPECT_CALL(service, LogStartedFromWhatsNewPage)
      .WillOnce(
          [&](user_education::TutorialIdentifier tutorial_id, bool is_running) {
            EXPECT_EQ(tutorial_id, kSavedTabGroupTutorialId);
            EXPECT_TRUE(is_running);
            return;
          });

  // Manually call tutorial started callback.
  command_handler_->OnTutorialStarted(kSavedTabGroupTutorialId, &service);
}

TEST_F(BrowserCommandHandlerTest, OpenAISettingsCommand) {
  // By default, opening the password manager is allowed.
  EXPECT_TRUE(CanExecuteCommand(Command::kOpenAISettings));
  ClickInfoPtr info = ClickInfo::New();
  info->middle_button = true;
  info->meta_key = true;
  // The OpenAISettings command opens a new settings window with the
  // AI settings and the correct disposition.
  EXPECT_CALL(*command_handler_, OpenAISettings());
  EXPECT_TRUE(ExecuteCommand(Command::kOpenAISettings, std::move(info)));
}

TEST_F(BrowserCommandHandlerTest, OpenPaymentsSettingsCommand) {
  // The OpenPaymentsSettings command opens a new settings window with the
  // Payments settings sub page, and the correct disposition.
  EXPECT_TRUE(CanExecuteCommand(Command::kOpenPaymentsSettings));
  ClickInfoPtr info = ClickInfo::New();
  info->middle_button = true;
  info->meta_key = true;
  EXPECT_CALL(
      *command_handler_,
      NavigateToURL(GURL(chrome::GetSettingsUrl(chrome::kPaymentsSubPage)),
                    DispositionFromClick(*info)));
  EXPECT_TRUE(ExecuteCommand(Command::kOpenPaymentsSettings, std::move(info)));
}

TEST_F(BrowserCommandHandlerTest, OpenHistorySearchSettingsCommand) {
  // By default, opening the History Search subpage is allowed.
  EXPECT_TRUE(CanExecuteCommand(Command::KOpenHistorySearchSettings));
  ClickInfoPtr info = ClickInfo::New();
  info->middle_button = true;
  info->meta_key = true;
  // The KOpenHistorySearchSettings command opens a new settings window with the
  // History Search settings and the correct disposition.
  EXPECT_CALL(
      *command_handler_,
      NavigateToURL(GURL(chrome::GetSettingsUrl(chrome::kHistorySearchSubpage)),
                    DispositionFromClick(*info)));
  EXPECT_TRUE(
      ExecuteCommand(Command::KOpenHistorySearchSettings, std::move(info)));
}
