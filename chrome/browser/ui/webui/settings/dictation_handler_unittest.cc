// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/dictation_handler.h"

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/command.h"

namespace settings {

constexpr char kSetDictationShortcutMessage[] = "setDictationShortcut";

class TestDictationHandler : public DictationHandler {
 public:
  using DictationHandler::AllowJavascript;
  using DictationHandler::set_web_ui;
};

class DictationHandlerTest : public testing::Test {
 public:
  DictationHandlerTest() = default;
  ~DictationHandlerTest() override = default;

  void SetUp() override {
    TestingProfile::Builder builder;
    profile_ = builder.Build();

    web_contents_factory_ = std::make_unique<content::TestWebContentsFactory>();
    web_contents_ = web_contents_factory_->CreateWebContents(profile_.get());

    test_web_ui_ = std::make_unique<content::TestWebUI>();
    test_web_ui_->set_web_contents(web_contents_);

    handler_ = std::make_unique<TestDictationHandler>();
    handler_->set_web_ui(test_web_ui_.get());
    handler_->RegisterMessages();
  }

  void TearDown() override { handler_.reset(); }

  content::TestWebUI* test_web_ui() const { return test_web_ui_.get(); }
  TestDictationHandler* handler() const { return handler_.get(); }
  Profile* profile() const { return profile_.get(); }

 protected:
  content::BrowserTaskEnvironment browser_task_environment_;

  std::unique_ptr<TestDictationHandler> handler_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::TestWebContentsFactory> web_contents_factory_;
  std::unique_ptr<content::TestWebUI> test_web_ui_;
  raw_ptr<content::WebContents> web_contents_;
};

TEST_F(DictationHandlerTest, SetValidShortcut) {
  // Ctrl+D is a valid shortcut (has modifier)
  ui::Accelerator accelerator(ui::VKEY_D, ui::EF_CONTROL_DOWN);
  std::string accelerator_str = ui::Command::AcceleratorToString(accelerator);

  base::ListValue args;
  args.Append("callback-id-valid");
  args.Append(accelerator_str);

  test_web_ui()->ProcessWebUIMessage(GURL(), kSetDictationShortcutMessage,
                                     std::move(args));

  EXPECT_EQ(accelerator_str,
            profile()->GetPrefs()->GetString(prefs::kVoiceTypingHotkey));

  EXPECT_EQ(1U, test_web_ui()->call_data().size());
  const content::TestWebUI::CallData& data = *test_web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", data.function_name());
  ASSERT_TRUE(data.arg1()->is_string());
  EXPECT_EQ("callback-id-valid", data.arg1()->GetString());
  ASSERT_TRUE(data.arg2()->is_bool());
  EXPECT_TRUE(data.arg2()->GetBool());
  ASSERT_TRUE(data.arg3()->is_bool());
  EXPECT_TRUE(data.arg3()->GetBool());
}

TEST_F(DictationHandlerTest, SetInvalidShortcutNoModifiers) {
  // Plain 'D' is invalid (no modifiers)
  base::ListValue args;
  args.Append("callback-id-invalid");
  args.Append("D");

  profile()->GetPrefs()->SetString(prefs::kVoiceTypingHotkey, "initial");

  test_web_ui()->ProcessWebUIMessage(GURL(), kSetDictationShortcutMessage,
                                     std::move(args));

  EXPECT_EQ("initial",
            profile()->GetPrefs()->GetString(prefs::kVoiceTypingHotkey));

  EXPECT_EQ(1U, test_web_ui()->call_data().size());
  const content::TestWebUI::CallData& data = *test_web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", data.function_name());
  ASSERT_TRUE(data.arg1()->is_string());
  EXPECT_EQ("callback-id-invalid", data.arg1()->GetString());
  ASSERT_TRUE(data.arg2()->is_bool());
  EXPECT_TRUE(data.arg2()->GetBool());
  ASSERT_TRUE(data.arg3()->is_bool());
  EXPECT_FALSE(data.arg3()->GetBool());
}

TEST_F(DictationHandlerTest, ClearShortcut) {
  // Empty string is valid (clears shortcut)
  base::ListValue args;
  args.Append("callback-id-clear");
  args.Append("");

  profile()->GetPrefs()->SetString(prefs::kVoiceTypingHotkey, "Ctrl+D");

  test_web_ui()->ProcessWebUIMessage(GURL(), kSetDictationShortcutMessage,
                                     std::move(args));

  EXPECT_EQ("", profile()->GetPrefs()->GetString(prefs::kVoiceTypingHotkey));

  EXPECT_EQ(1U, test_web_ui()->call_data().size());
  const content::TestWebUI::CallData& data = *test_web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", data.function_name());
  ASSERT_TRUE(data.arg1()->is_string());
  EXPECT_EQ("callback-id-clear", data.arg1()->GetString());
  ASSERT_TRUE(data.arg2()->is_bool());
  EXPECT_TRUE(data.arg2()->GetBool());
  ASSERT_TRUE(data.arg3()->is_bool());
  EXPECT_TRUE(data.arg3()->GetBool());
}

}  // namespace settings
