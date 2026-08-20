// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/omnibox_everywhere_settings_handler.h"

#include <memory>
#include <string>

#include "base/values.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"

namespace settings {

namespace {

const char kCallbackId[] = "test-callback-id";

class TestingOmniboxEverywhereSettingsHandler
    : public OmniboxEverywhereSettingsHandler {
 public:
  using OmniboxEverywhereSettingsHandler::set_web_ui;
};

}  // namespace

class OmniboxEverywhereSettingsHandlerTest : public testing::Test {
 public:
  OmniboxEverywhereSettingsHandlerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("TestProfile");
    web_contents_ = web_contents_factory_.CreateWebContents(profile_);
    web_ui_.set_web_contents(web_contents_);

    handler_ = std::make_unique<TestingOmniboxEverywhereSettingsHandler>();
    handler_->set_web_ui(&web_ui_);
    handler_->RegisterMessages();
    handler_->AllowJavascriptForTesting();
  }

  void TearDown() override { handler_.reset(); }

  TestingProfile* profile() { return profile_; }
  content::TestWebUI* web_ui() { return &web_ui_; }
  TestingOmniboxEverywhereSettingsHandler* handler() { return handler_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  raw_ptr<TestingProfile> profile_ = nullptr;
  content::TestWebUI web_ui_;
  std::unique_ptr<TestingOmniboxEverywhereSettingsHandler> handler_;
};

TEST_F(OmniboxEverywhereSettingsHandlerTest, GetDefaultShortcut) {
  base::ListValue args;
  args.Append(kCallbackId);
  handler()->HandleGetOmniboxEverywhereShortcut(args);

  EXPECT_EQ(1U, web_ui()->call_data().size());
  const auto& call_data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", call_data.function_name());
  EXPECT_EQ(kCallbackId, call_data.arg1()->GetString());
  EXPECT_TRUE(call_data.arg2()->GetBool());
  EXPECT_FALSE(call_data.arg3()->GetString().empty());
}

TEST_F(OmniboxEverywhereSettingsHandlerTest, SetAndGetCustomValidShortcut) {
  const std::string custom_shortcut = "Ctrl+Shift+Space";

  base::ListValue set_args;
  set_args.Append(kCallbackId);
  set_args.Append(custom_shortcut);
  handler()->HandleSetOmniboxEverywhereShortcut(set_args);

  EXPECT_EQ(1U, web_ui()->call_data().size());
  const auto& set_call_data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", set_call_data.function_name());
  EXPECT_EQ(kCallbackId, set_call_data.arg1()->GetString());
  EXPECT_TRUE(set_call_data.arg2()->GetBool());
  EXPECT_TRUE(set_call_data.arg3()->GetBool());  // is_valid = true

  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();
  EXPECT_EQ(custom_shortcut,
            local_state->GetString(
                omnibox_everywhere::prefs::kOmniboxEverywhereHotkey));

  base::ListValue get_args;
  get_args.Append("callback-2");
  handler()->HandleGetOmniboxEverywhereShortcut(get_args);

  const auto& get_call_data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", get_call_data.function_name());
  EXPECT_EQ("callback-2", get_call_data.arg1()->GetString());
  EXPECT_TRUE(get_call_data.arg2()->GetBool());
  EXPECT_FALSE(get_call_data.arg3()->GetString().empty());
}

TEST_F(OmniboxEverywhereSettingsHandlerTest, SetInvalidShortcutRejected) {
  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();
  local_state->SetString(omnibox_everywhere::prefs::kOmniboxEverywhereHotkey,
                         "");

  // A key combination with no modifier is invalid.
  base::ListValue set_args;
  set_args.Append(kCallbackId);
  set_args.Append("A");
  handler()->HandleSetOmniboxEverywhereShortcut(set_args);

  EXPECT_EQ(1U, web_ui()->call_data().size());
  const auto& call_data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", call_data.function_name());
  EXPECT_EQ(kCallbackId, call_data.arg1()->GetString());
  EXPECT_TRUE(call_data.arg2()->GetBool());
  EXPECT_FALSE(call_data.arg3()->GetBool());  // is_valid = false

  // Local state pref must remain unchanged.
  EXPECT_EQ("", local_state->GetString(
                    omnibox_everywhere::prefs::kOmniboxEverywhereHotkey));
}

TEST_F(OmniboxEverywhereSettingsHandlerTest, SetEmptyShortcutClearsPref) {
  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();
  local_state->SetString(omnibox_everywhere::prefs::kOmniboxEverywhereHotkey,
                         "Ctrl+Shift+Space");

  base::ListValue set_args;
  set_args.Append(kCallbackId);
  set_args.Append("");
  handler()->HandleSetOmniboxEverywhereShortcut(set_args);

  EXPECT_EQ(1U, web_ui()->call_data().size());
  const auto& call_data = *web_ui()->call_data().back();
  EXPECT_TRUE(call_data.arg3()->GetBool());  // is_valid = true

  EXPECT_EQ("", local_state->GetString(
                    omnibox_everywhere::prefs::kOmniboxEverywhereHotkey));
}

TEST_F(OmniboxEverywhereSettingsHandlerTest, SetShortcutSuspensionState) {
  base::ListValue args_suspend;
  args_suspend.Append(true);
  handler()->HandleSetOmniboxEverywhereShortcutSuspensionState(args_suspend);
  EXPECT_TRUE(handler()->is_shortcut_suspended_for_testing());

  base::ListValue args_resume;
  args_resume.Append(false);
  handler()->HandleSetOmniboxEverywhereShortcutSuspensionState(args_resume);
  EXPECT_FALSE(handler()->is_shortcut_suspended_for_testing());
}

TEST_F(OmniboxEverywhereSettingsHandlerTest, TeardownResumesSuspension) {
  base::ListValue args_suspend;
  args_suspend.Append(true);
  handler()->HandleSetOmniboxEverywhereShortcutSuspensionState(args_suspend);
  EXPECT_TRUE(handler()->is_shortcut_suspended_for_testing());

  handler()->DisallowJavascript();
  EXPECT_FALSE(handler()->is_shortcut_suspended_for_testing());
}

}  // namespace settings
