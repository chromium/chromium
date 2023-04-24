// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/webui/settings/about_handler.h"

#include "base/memory/raw_ptr.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/update_engine/fake_update_engine_client.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace settings {

namespace {

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
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
    ash::ConciergeClient::Shutdown();
    ash::UpdateEngineClient::Shutdown();
  }

  const content::TestWebUI::CallData& CallDataAtIndex(size_t index) {
    return *web_ui_.call_data()[index];
  }

  std::string CallGetEndOfLifeInfoAndReturnString(bool has_eol_passed) {
    size_t call_data_count_before_call = web_ui_.call_data().size();

    base::Value::List args;
    args.Append("handlerFunctionName");
    web_ui_.HandleReceivedMessage("getEndOfLifeInfo", args);
    task_environment_.RunUntilIdle();

    EXPECT_EQ(call_data_count_before_call + 1u, web_ui_.call_data().size());

    const content::TestWebUI::CallData& call_data =
        CallDataAtIndex(call_data_count_before_call);
    EXPECT_EQ("cr.webUIResponse", call_data.function_name());
    EXPECT_EQ("handlerFunctionName", call_data.arg1()->GetString());
    EXPECT_EQ(has_eol_passed,
              *call_data.arg3()->GetDict().FindBool("hasEndOfLife"));
    return *call_data.arg3()->GetDict().FindString("aboutPageEndOfLifeMessage");
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

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  content::TestWebUI web_ui_;
  std::unique_ptr<TestAboutHandler> handler_;
  raw_ptr<ash::FakeUpdateEngineClient, ExperimentalAsh>
      fake_update_engine_client_;
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

}  // namespace

}  // namespace settings
}  // namespace chromeos
