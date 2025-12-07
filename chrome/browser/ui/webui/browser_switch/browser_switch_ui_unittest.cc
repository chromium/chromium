// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/browser_switch/browser_switch_ui.h"

#include <memory>
#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#include "chrome/browser/browser_switcher/browser_switcher_service.h"
#include "chrome/browser/browser_switcher/browser_switcher_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestBrowserSwitchHandler : public BrowserSwitchHandler {
 public:
  explicit TestBrowserSwitchHandler(content::WebUI* web_ui) {
    set_web_ui(web_ui);
  }

  ~TestBrowserSwitchHandler() override = default;
};

class BrowserSwitchHandlerTest : public testing::Test {
 public:
  ~BrowserSwitchHandlerTest() override = default;

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();

    profile_->GetTestingPrefService()->SetManagedPref(
        browser_switcher::prefs::kEnabled, base::Value(true));

    web_contents_ =
        content::WebContents::Create(content::WebContents::CreateParams(
            profile_.get(), content::SiteInstance::Create(profile_.get())));
    web_ui_.set_web_contents(web_contents_.get());

    browser_switcher::BrowserSwitcherServiceFactory::GetInstance()
        ->SetTestingFactory(
            profile_.get(),
            base::BindRepeating([](content::BrowserContext* context) {
              return static_cast<std::unique_ptr<KeyedService>>(
                  std::make_unique<browser_switcher::BrowserSwitcherService>(
                      Profile::FromBrowserContext(context)));
            }));

    handler_ = std::make_unique<TestBrowserSwitchHandler>(&web_ui_);
    handler_->RegisterMessages();
    handler_->AllowJavascriptForTesting();
  }

  content::TestWebUI* web_ui() { return &web_ui_; }

 private:
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  content::TestWebUI web_ui_;
  std::unique_ptr<TestBrowserSwitchHandler> handler_;
};

TEST_F(BrowserSwitchHandlerTest, HandleIsBrowserSwitchEnabled) {
  base::Value::List args;
  args.Append("callbackId");
  web_ui()->HandleReceivedMessage("isBrowserSwitcherEnabled", args);

  EXPECT_EQ(web_ui()->call_data().size(), 1U);
  const content::TestWebUI::CallData& call_data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", call_data.function_name());
  EXPECT_EQ("callbackId", call_data.arg1()->GetString());
  EXPECT_TRUE(call_data.arg2()->GetBool());

  EXPECT_TRUE(call_data.arg3()->GetBool());
}

TEST_F(BrowserSwitchHandlerTest, GetBrowserSwitchInternalsJson) {
  base::Value::List args;
  args.Append("callbackId");
  web_ui()->HandleReceivedMessage("getBrowserSwitchInternalsJson", args);

  EXPECT_EQ(web_ui()->call_data().size(), 1U);
  const content::TestWebUI::CallData& call_data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", call_data.function_name());
  EXPECT_EQ("callbackId", call_data.arg1()->GetString());
  EXPECT_TRUE(call_data.arg2()->GetBool());

  const std::string& json_string = call_data.arg3()->GetString();
  auto parsed_json =
      base::JSONReader::Read(json_string, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  const base::Value::Dict& dict = parsed_json->GetDict();
  const base::Value::Dict* policies = dict.FindDict("policies");
  EXPECT_TRUE(policies->FindBool("BrowserSwitcherEnabled").value());
}

// TODO(nicolaso): Improve coverage.
// TODO(thelex): Improve coverage.

}  // namespace
