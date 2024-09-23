// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/reset_settings_handler.h"

#include <stddef.h>

#include "base/values.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/profile_resetter/fake_profile_resetter.h"
#include "chrome/browser/profile_resetter/profile_resetter.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

using settings::ResetSettingsHandler;

namespace {

class TestingResetSettingsHandler : public ResetSettingsHandler {
 public:
  TestingResetSettingsHandler(
      TestingProfile* profile, content::WebUI* web_ui)
      : ResetSettingsHandler(profile),
        resetter_(profile) {
    set_web_ui(web_ui);
  }

  TestingResetSettingsHandler(const TestingResetSettingsHandler&) = delete;
  TestingResetSettingsHandler& operator=(const TestingResetSettingsHandler&) =
      delete;

  size_t Resets() const { return resetter_.Resets(); }

  using settings::ResetSettingsHandler::HandleResetProfileSettings;

 protected:
  ProfileResetter* GetResetter() override { return &resetter_; }

 private:
  FakeProfileResetter resetter_;
};

class ResetSettingsHandlerTest : public testing::Test {
 public:
  ResetSettingsHandlerTest() {
    google_brand::BrandForTesting brand_for_testing("");
    handler_ =
        std::make_unique<TestingResetSettingsHandler>(&profile_, &web_ui_);
  }

  TestingResetSettingsHandler* handler() { return handler_.get(); }
  content::TestWebUI* web_ui() { return &web_ui_; }

 private:
  // The order here matters.
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  content::TestWebUI web_ui_;
  std::unique_ptr<TestingResetSettingsHandler> handler_;
};

TEST_F(ResetSettingsHandlerTest, HandleResetProfileSettings) {
  base::Value::List list;
  std::string expected_callback_id("dummyCallbackId");
  list.Append(expected_callback_id);
  list.Append(false);
  list.Append("");
  handler()->HandleResetProfileSettings(list);
  // Check that the delegate ProfileResetter was called.
  EXPECT_EQ(1u, handler()->Resets());
  // Check that Javascript side is notified after resetting is done.
  EXPECT_EQ("cr.webUIResponse",
            web_ui()->call_data()[0]->function_name());
  const std::string* callback_id =
      web_ui()->call_data()[0]->arg1()->GetIfString();
  EXPECT_NE(nullptr, callback_id);
  EXPECT_EQ(expected_callback_id, *callback_id);
}

}  // namespace
