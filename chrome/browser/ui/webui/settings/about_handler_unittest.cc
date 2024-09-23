// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/webui/settings/about_handler.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/extended_updates/extended_updates_controller.h"
#include "chrome/browser/ash/extended_updates/test/mock_extended_updates_controller.h"
#include "chrome/browser/ash/extended_updates/test/scoped_extended_updates_controller.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/update_engine/fake_update_engine_client.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::HasSubstr;
using ::testing::NotNull;
using ::testing::Return;

namespace chromeos {
namespace settings {

namespace {

constexpr std::string_view kExtendedUpdatesSettingChangedMessage =
    "extended-updates-setting-changed";

class TestAboutHandler : public ::settings::AboutHandler {
 public:
  explicit TestAboutHandler(Profile* profile) : AboutHandler(profile) {}
  ~TestAboutHandler() override = default;

  // Make public for testing.
  using AboutHandler::set_clock;

  // Make public for testing.
  using AboutHandler::set_web_ui;
};

class AboutHandlerTest : public testing::Test {
 public:
  AboutHandlerTest() = default;
  ~AboutHandlerTest() override = default;
  AboutHandlerTest(const AboutHandlerTest&) = delete;
  AboutHandlerTest& operator=(const AboutHandlerTest&) = delete;

  void SetUp() override {
    fake_update_engine_client_ =
        ash::UpdateEngineClient::InitializeFakeForTest();
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);

    handler_ = std::make_unique<TestAboutHandler>(&profile_);
    handler_->set_web_ui(&web_ui_);
    handler_->RegisterMessages();
    handler_->AllowJavascriptForTesting();

    clock_ = std::make_unique<base::SimpleTestClock>();
    handler_->set_clock(clock_.get());
  }

  void TearDown() override {
    handler_.reset();
    fake_update_engine_client_ = nullptr;
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
    ash::ConciergeClient::Shutdown();
    ash::UpdateEngineClient::Shutdown();
  }

  const base::Value& CallWebUIMessage(const std::string& message,
                                      base::Value::List args = {}) {
    size_t initial_call_count = web_ui_.call_data().size();

    base::Value::List message_args;
    message_args.Append("handlerFunctionName");
    for (base::Value& arg : args) {
      message_args.Append(std::move(arg));
    }
    web_ui_.HandleReceivedMessage(message, message_args);
    task_environment_.RunUntilIdle();

    EXPECT_EQ(initial_call_count + 1u, web_ui_.call_data().size());

    const content::TestWebUI::CallData& call_data =
        *web_ui_.call_data()[initial_call_count];
    EXPECT_EQ("cr.webUIResponse", call_data.function_name());
    EXPECT_EQ("handlerFunctionName", call_data.arg1()->GetString());
    return *call_data.arg3();
  }

  std::string CallGetEndOfLifeInfoAndReturnString(bool has_eol_passed) {
    const auto& response = CallWebUIMessage("getEndOfLifeInfo").GetDict();
    EXPECT_EQ(has_eol_passed, *response.FindBool("hasEndOfLife"));
    return *response.FindString("aboutPageEndOfLifeMessage");
  }

  void SetCurrentTimeToUtc(const char* utc_date_string) {
    base::Time utc_time;
    ASSERT_TRUE(base::Time::FromUTCString(utc_date_string, &utc_time));
    clock_->SetNow(utc_time);
  }

  void SetEolDateUtc(const char* utc_date_string) {
    base::Time utc_date;
    ASSERT_TRUE(base::Time::FromUTCString(utc_date_string, &utc_date));
    fake_update_engine_client_->set_eol_date(utc_date);
  }

  void RestartJavascript() {
    handler_->DisallowJavascript();
    handler_->AllowJavascriptForTesting();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  ash::ScopedTestingCrosSettings test_cros_settings_;
  ash::ScopedStubInstallAttributes test_install_attributes_;
  TestingProfile profile_;
  content::TestWebUI web_ui_;
  std::unique_ptr<TestAboutHandler> handler_;
  raw_ptr<ash::FakeUpdateEngineClient> fake_update_engine_client_;
  std::unique_ptr<base::SimpleTestClock> clock_;
};

TEST_F(AboutHandlerTest, EndOfLifeMessageInAboutDetailsSubpage) {
  SetCurrentTimeToUtc("15 March 2020");

  SetEolDateUtc("15 November 2017");
  EXPECT_EQ(
      "This device stopped getting automatic software and security "
      "updates in November 2017. <a target=\"_blank\" href=\"https:"
      "//www.google.com/chromebook/older/\">Learn more</a>",
      CallGetEndOfLifeInfoAndReturnString(true /*=has_eol_passed*/));

  SetEolDateUtc("15 May 2023");
  EXPECT_EQ(
      "This device will get automatic software and security updates "
      "until May 2023. <a target=\"_blank\" href=\"https://support.google"
      ".com/chrome/a?p=auto-update-policy\">Learn more</a>",
      CallGetEndOfLifeInfoAndReturnString(false /*=has_eol_passed*/));

  const base::Time null_time = base::Time();
  fake_update_engine_client_->set_eol_date(null_time);
  EXPECT_EQ("", CallGetEndOfLifeInfoAndReturnString(false /*=has_eol_passed*/));
}

TEST_F(AboutHandlerTest, DeferredUpdateMessageInAboutPage) {
  update_engine::StatusResult status;
  status.set_current_operation(update_engine::Operation::UPDATED_BUT_DEFERRED);
  fake_update_engine_client_->set_default_status(status);
  fake_update_engine_client_->NotifyObserversThatStatusChanged(status);

  EXPECT_EQ(0, fake_update_engine_client_->apply_deferred_update_count());
  web_ui_.HandleReceivedMessage("applyDeferredUpdate", base::Value::List());
  EXPECT_EQ(1, fake_update_engine_client_->apply_deferred_update_count());
}

TEST_F(AboutHandlerTest, GetEndOfLifeInfoWithoutExtendedUpdatesDate) {
  SetCurrentTimeToUtc("15 March 2020");
  SetEolDateUtc("30 Oct 2023");

  const auto& response = CallWebUIMessage("getEndOfLifeInfo").GetDict();
  EXPECT_FALSE(*response.FindBool("isExtendedUpdatesDatePassed"));
  EXPECT_FALSE(*response.FindBool("isExtendedUpdatesOptInRequired"));
}

TEST_F(AboutHandlerTest, GetEndOfLifeInfoWithExtendedUpdatesDatePassed) {
  SetCurrentTimeToUtc("15 March 2020");
  base::Time eol_date, extended_date;
  ASSERT_TRUE(base::Time::FromUTCString("30 Oct 2023", &eol_date));
  ASSERT_TRUE(base::Time::FromUTCString("4 June 2019", &extended_date));
  fake_update_engine_client_->set_eol_info({
      .eol_date = eol_date,
      .extended_date = extended_date,
      .extended_opt_in_required = true,
  });

  const auto& response = CallWebUIMessage("getEndOfLifeInfo").GetDict();
  EXPECT_TRUE(*response.FindBool("isExtendedUpdatesDatePassed"));
  EXPECT_TRUE(*response.FindBool("isExtendedUpdatesOptInRequired"));
  EXPECT_THAT(*response.FindString("aboutPageEndOfLifeMessage"),
              HasSubstr("June 2019"));
}

TEST_F(AboutHandlerTest, GetEndOfLifeInfoWithExtendedUpdatesDateNotPassed) {
  SetCurrentTimeToUtc("15 March 2020");
  base::Time eol_date, extended_date;
  ASSERT_TRUE(base::Time::FromUTCString("30 Oct 2023", &eol_date));
  ASSERT_TRUE(base::Time::FromUTCString("4 June 2021", &extended_date));
  fake_update_engine_client_->set_eol_info({
      .eol_date = eol_date,
      .extended_date = extended_date,
      .extended_opt_in_required = true,
  });

  const auto& response = CallWebUIMessage("getEndOfLifeInfo").GetDict();
  EXPECT_FALSE(*response.FindBool("isExtendedUpdatesDatePassed"));
  EXPECT_TRUE(*response.FindBool("isExtendedUpdatesOptInRequired"));
  EXPECT_THAT(*response.FindString("aboutPageEndOfLifeMessage"),
              HasSubstr("June 2021"));
}

TEST_F(AboutHandlerTest, GetEndOfLifeInfoWithExtendedUpdatesOptedIn) {
  SetCurrentTimeToUtc("15 March 2020");
  base::Time eol_date, extended_date;
  ASSERT_TRUE(base::Time::FromUTCString("30 Oct 2023", &eol_date));
  ASSERT_TRUE(base::Time::FromUTCString("4 June 2021", &extended_date));
  fake_update_engine_client_->set_eol_info({
      .eol_date = eol_date,
      .extended_date = extended_date,
      .extended_opt_in_required = true,
  });

  ash::MockExtendedUpdatesController mock_controller;
  ash::ScopedExtendedUpdatesController scoped_controller(&mock_controller);

  EXPECT_CALL(mock_controller, IsOptedIn()).WillOnce(Return(true));

  const auto& response = CallWebUIMessage("getEndOfLifeInfo").GetDict();
  EXPECT_FALSE(*response.FindBool("isExtendedUpdatesDatePassed"));
  EXPECT_TRUE(*response.FindBool("isExtendedUpdatesOptInRequired"));
  EXPECT_THAT(*response.FindString("aboutPageEndOfLifeMessage"),
              HasSubstr("October 2023"));
}

TEST_F(AboutHandlerTest, HandleIsExtendedUpdatesOptInEligible) {
  ash::ExtendedUpdatesController::Params params{
      .eol_passed = false,
      .extended_date_passed = true,
      .opt_in_required = true,
  };

  ash::MockExtendedUpdatesController mock_controller;
  ash::ScopedExtendedUpdatesController scoped_controller(&mock_controller);

  EXPECT_CALL(mock_controller, IsOptInEligible(NotNull(), params))
      .WillOnce(Return(true));

  bool eligible = CallWebUIMessage("isExtendedUpdatesOptInEligible",
                                   base::Value::List()
                                       .Append(params.eol_passed)
                                       .Append(params.extended_date_passed)
                                       .Append(params.opt_in_required))
                      .GetBool();
  EXPECT_TRUE(eligible);
}

TEST_F(AboutHandlerTest, ObservesExtendedUpdatesSettingChanges) {
  ash::MockExtendedUpdatesController mock_controller;
  ash::ScopedExtendedUpdatesController scoped_controller(&mock_controller);

  EXPECT_CALL(mock_controller, HasOptInAbility(NotNull()))
      .WillOnce(Return(true));

  EXPECT_EQ(web_ui_.call_data().size(), 0u);
  EXPECT_TRUE(mock_controller.OptIn(&profile_));

  ASSERT_EQ(web_ui_.call_data().size(), 1u);
  const auto& call_data = web_ui_.call_data()[0];
  ASSERT_EQ(call_data->args().size(), 1u);
  EXPECT_EQ(call_data->args()[0].GetString(),
            kExtendedUpdatesSettingChangedMessage);
}

TEST_F(AboutHandlerTest, ObservesExtendedUpdatesSettingChangesAfterRefresh) {
  // Simulate the disallowing and allowing of Javascript during a page refresh.
  RestartJavascript();

  ash::MockExtendedUpdatesController mock_controller;
  ash::ScopedExtendedUpdatesController scoped_controller(&mock_controller);

  EXPECT_CALL(mock_controller, HasOptInAbility(NotNull()))
      .WillOnce(Return(true));

  EXPECT_EQ(web_ui_.call_data().size(), 0u);
  EXPECT_TRUE(mock_controller.OptIn(&profile_));

  ASSERT_EQ(web_ui_.call_data().size(), 1u);
  const auto& call_data = web_ui_.call_data()[0];
  ASSERT_EQ(call_data->args().size(), 1u);
  EXPECT_EQ(call_data->args()[0].GetString(),
            kExtendedUpdatesSettingChangedMessage);
}

}  // namespace

}  // namespace settings
}  // namespace chromeos
