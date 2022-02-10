// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/url_handlers_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/web_applications/test/fake_web_app_registry_controller.h"
#include "chrome/browser/web_applications/url_handler_prefs.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/url_handler_info.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace settings {
namespace {

class TestUrlHandlersHandler : public settings::UrlHandlersHandler {
 public:
  TestUrlHandlersHandler(PrefService* local_state,
                         Profile* profile,
                         web_app::WebAppRegistrar* web_app_registrar)
      : UrlHandlersHandler(local_state, profile, web_app_registrar) {}
  ~TestUrlHandlersHandler() override = default;
  TestUrlHandlersHandler(const TestUrlHandlersHandler&) = delete;
  TestUrlHandlersHandler& operator=(const TestUrlHandlersHandler&) = delete;

  using settings::UrlHandlersHandler::set_web_ui;

 private:
  friend class ::settings::UrlHandlersHandlerTest;
};

constexpr char kTestCallbackId[] = "test-callback-id";
constexpr char kEmptyList[] = R"([ ])";

}  // namespace

class UrlHandlersHandlerTest : public testing::Test {
 public:
  UrlHandlersHandlerTest()
      : scoped_testing_local_state_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(base::Time::FromString("1 Jan 2000 00:00:00 GMT", &time_1_));
  }

  ~UrlHandlersHandlerTest() override = default;
  UrlHandlersHandlerTest(const UrlHandlersHandlerTest&) = delete;
  UrlHandlersHandlerTest& operator=(const UrlHandlersHandlerTest&) = delete;

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();

    fake_registry_controller_ =
        std::make_unique<web_app::FakeWebAppRegistryController>();
    fake_registry_controller_->SetUp(profile_.get());

    handler_ = std::make_unique<TestUrlHandlersHandler>(
        local_state(), profile(), &test_app_registrar());
    handler_->set_web_ui(web_ui());
    handler_->RegisterMessages();
    handler_->AllowJavascriptForTesting();

    web_ui()->ClearTrackedCalls();

    controller().Init();
  }

  const web_app::WebApp* RegisterWebAppWithUrlHandlers(
      const GURL& app_url,
      const apps::UrlHandlers& url_handlers) {
    std::unique_ptr<web_app::WebApp> web_app =
        std::make_unique<web_app::WebApp>(web_app::GenerateAppId(
            /*manifest_id=*/absl::nullopt, GURL(app_url)));
    web_app->AddSource(web_app::Source::kDefault);
    web_app->SetName("App Name");
    web_app->SetDisplayMode(web_app::DisplayMode::kStandalone);
    web_app->SetUserDisplayMode(web_app::DisplayMode::kStandalone);
    web_app->SetStartUrl(app_url);
    web_app->SetUrlHandlers(url_handlers);
    const web_app::AppId app_id = web_app->app_id();
    web_app::url_handler_prefs::AddWebApp(
        local_state(), app_id, profile()->GetPath(), web_app->url_handlers());
    controller().RegisterApp(std::move(web_app));
    return registrar().GetAppById(app_id);
  }

  void TearDown() override {
    profile_.reset();
    handler_.reset();
  }

  web_app::FakeWebAppRegistryController& controller() {
    return *fake_registry_controller_;
  }

  web_app::WebAppRegistrar& registrar() { return controller().registrar(); }

  web_app::WebAppRegistrar& test_app_registrar() {
    return controller().registrar();
  }

  TestingProfile* profile() { return profile_.get(); }

  content::TestWebUI* web_ui() { return &web_ui_; }

  PrefService* local_state() {
    return TestingBrowserProcess::GetGlobal()->local_state();
  }

  TestUrlHandlersHandler* handler() { return handler_.get(); }

  void ParseAndExpectValue(const base::Value& value,
                           const std::string& expectation) {
    base::Value expected_value = base::test::ParseJson(expectation);
    EXPECT_EQ(value, expected_value);
  }

  void CallAndExpectGetUrlHandlers(
      const std::string& expected_enabled_handlers = "",
      const std::string& expected_disabled_handlers = "") {
    base::ListValue list_args;
    list_args.Append(kTestCallbackId);
    web_ui()->HandleReceivedMessage("getUrlHandlers", &list_args);

    ASSERT_EQ(1u, web_ui()->call_data().size());
    const auto& data = *web_ui()->call_data()[0];
    EXPECT_EQ("cr.webUIResponse", data.function_name());

    ASSERT_TRUE(data.arg1()->is_string());
    EXPECT_EQ(kTestCallbackId, data.arg1()->GetString());

    // Check that ResolveJavascriptCallback was called by the handler.
    ASSERT_TRUE(data.arg2()->is_bool());
    EXPECT_TRUE(data.arg2()->GetBool())
        << "Callback should be resolved successfully by HandleGetUrlHandlers";

    // Check data given to ResolveJavascriptCallback.
    ASSERT_TRUE(data.arg3()->is_dict());
    const base::Value* callback_data = data.arg3();
    ASSERT_TRUE(callback_data != nullptr);
    const base::Value* enabled_handlers = callback_data->FindKey("enabled");
    const base::Value* disabled_handlers = callback_data->FindKey("disabled");
    ASSERT_TRUE(enabled_handlers != nullptr);
    ASSERT_TRUE(disabled_handlers != nullptr);
    if (!expected_enabled_handlers.empty())
      ParseAndExpectValue(*enabled_handlers, expected_enabled_handlers);
    if (!expected_disabled_handlers.empty())
      ParseAndExpectValue(*disabled_handlers, expected_disabled_handlers);

    web_ui()->ClearTrackedCalls();
  }

  void ExpectUpdateUrlHandlers(
      const std::string& expected_enabled_handlers = "",
      const std::string& expected_disabled_handlers = "") {
    ASSERT_EQ(web_ui()->call_data().size(), 1u);
    const content::TestWebUI::CallData& data = *web_ui()->call_data()[0];
    EXPECT_EQ("cr.webUIListenerCallback", data.function_name());
    ASSERT_TRUE(data.arg1()->is_string());
    EXPECT_EQ("updateUrlHandlers", data.arg1()->GetString());

    if (!expected_enabled_handlers.empty())
      ParseAndExpectValue(*data.arg2(), expected_enabled_handlers);
    if (!expected_disabled_handlers.empty())
      ParseAndExpectValue(*data.arg3(), expected_disabled_handlers);

    web_ui()->ClearTrackedCalls();
  }

  void ExpectEnabledHandlersList(const std::string& expected) {
    const base::Value expected_value = base::test::ParseJson(expected);
    EXPECT_EQ(handler()->GetEnabledHandlersList(), expected_value);
  }

  void ExpectDisabledHandlersList(const std::string& expected) {
    const base::Value expected_value = base::test::ParseJson(expected);
    EXPECT_EQ(handler()->GetDisabledHandlersList(), expected_value);
  }

  void ExpectUrlHandlerPrefs(const std::string& expected_prefs) {
    const base::Value* const stored_prefs =
        local_state()->Get(prefs::kWebAppsUrlHandlerInfo);
    ASSERT_TRUE(stored_prefs);
    const base::Value expected_prefs_value =
        base::test::ParseJson(expected_prefs);
    EXPECT_EQ(*stored_prefs, expected_prefs_value);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestingProfile> profile_;
  content::TestWebUI web_ui_;
  std::unique_ptr<web_app::FakeWebAppRegistryController>
      fake_registry_controller_;
  std::unique_ptr<TestUrlHandlersHandler> handler_;
  base::Time time_1_;
  const GURL app_url_ = GURL("https://app.com");
  const GURL app2_url_ = GURL("https://app2.com");
  const url::Origin target_origin_ =
      url::Origin::Create(GURL("https://target-1.com"));
  const GURL url_in_target_origin_ =
      target_origin_.GetURL().Resolve("/index.html");
};

TEST_F(UrlHandlersHandlerTest, HandleGetUrlHandlers) {
  // Trigger HandleGetUrlHandlers and observe that it calls
  // ResolveJavascriptCallback successfully.
  CallAndExpectGetUrlHandlers(kEmptyList, kEmptyList);

  // Install app.
  const auto* web_app = RegisterWebAppWithUrlHandlers(
      app_url_, {apps::UrlHandlerInfo(target_origin_)});
  ExpectUpdateUrlHandlers(kEmptyList, kEmptyList);

  // Save user choice to open in app.
  web_app::url_handler_prefs::SaveOpenInApp(local_state(), web_app->app_id(),
                                            profile()->GetPath(),
                                            url_in_target_origin_, time_1_);
  constexpr char kEnabledHandlers[] = R"([ {
    "app_entries": [ {
      "app_id": "ahkofokocdmhbhhpkeohpoocnniagaac",
      "has_origin_wildcard": false,
      "origin_key": "https://target-1.com",
      "path": "/*",
      "publisher": "app.com",
      "short_name": "App Name"
    } ],
    "display_origin": "target-1.com"
  } ])";
  ExpectUpdateUrlHandlers(kEnabledHandlers, kEmptyList);
  // Trigger HandleGetUrlHandlers and observe that it calls
  // ResolveJavascriptCallback successfully. This isn't necessary but we call
  // "getUrlHandlers" here to check that its values are identical to values from
  // "updateUrlHandlers".
  CallAndExpectGetUrlHandlers(kEnabledHandlers, kEmptyList);

  // Save user choice to open in browser.
  web_app::url_handler_prefs::SaveOpenInBrowser(local_state(),
                                                url_in_target_origin_, time_1_);
  constexpr char kDisabledHandlers[] = R"([ {
    "display_url": "target-1.com",
    "has_origin_wildcard": false,
    "origin_key": "https://target-1.com",
    "path": "/*"
  }])";
  ExpectUpdateUrlHandlers(kEmptyList, kDisabledHandlers);
  CallAndExpectGetUrlHandlers(kEmptyList, kDisabledHandlers);
}

TEST_F(UrlHandlersHandlerTest, HandleResetUrlHandlerSavedChoice) {
  const auto* web_app = RegisterWebAppWithUrlHandlers(
      app_url_, {apps::UrlHandlerInfo(target_origin_)});
  // Prefs changes should cause data to be sent to WebUI.
  ExpectUpdateUrlHandlers(kEmptyList, kEmptyList);

  // Save user choice to open in app.
  web_app::url_handler_prefs::SaveOpenInApp(local_state(), web_app->app_id(),
                                            profile()->GetPath(),
                                            url_in_target_origin_, time_1_);
  ExpectUpdateUrlHandlers(R"([ {
    "app_entries": [ {
      "app_id": "ahkofokocdmhbhhpkeohpoocnniagaac",
      "has_origin_wildcard": false,
      "origin_key": "https://target-1.com",
      "path": "/*",
      "publisher": "app.com",
      "short_name": "App Name"
    } ],
    "display_origin": "target-1.com"
  } ])",
                          kEmptyList);

  // Trigger resetUrlHandlerSavedChoice event directly. That should result
  // in local state prefs being updated and then update to WebUI.
  base::ListValue list_args;
  list_args.Append("https://target-1.com");        // origin
  list_args.Append(false);                         // has_origin_wildcard
  list_args.Append("/*");                          // url_path
  list_args.Append(web_app->app_id());             // app_id
  web_ui()->HandleReceivedMessage("resetUrlHandlerSavedChoice", &list_args);
  ExpectUpdateUrlHandlers(kEmptyList, kEmptyList);
}

TEST_F(UrlHandlersHandlerTest, EnabledHandlers) {
  const auto* web_app = RegisterWebAppWithUrlHandlers(
      app_url_, {apps::UrlHandlerInfo(target_origin_)});
  ExpectUpdateUrlHandlers(kEmptyList, kEmptyList);
  // Save user choice to open in app.
  web_app::url_handler_prefs::SaveOpenInApp(local_state(), web_app->app_id(),
                                            profile()->GetPath(),
                                            url_in_target_origin_, time_1_);

  ExpectUpdateUrlHandlers(R"([ {
    "app_entries": [ {
      "app_id": "ahkofokocdmhbhhpkeohpoocnniagaac",
      "has_origin_wildcard": false,
      "origin_key": "https://target-1.com",
      "path": "/*",
      "publisher": "app.com",
      "short_name": "App Name"
    } ],
    "display_origin": "target-1.com"
  } ])",
                          kEmptyList);
}

TEST_F(UrlHandlersHandlerTest, DisabledHandlers) {
  RegisterWebAppWithUrlHandlers(app_url_,
                                {apps::UrlHandlerInfo(target_origin_)});
  ExpectUpdateUrlHandlers(kEmptyList, kEmptyList);

  // Save user choice to open in browser.
  web_app::url_handler_prefs::SaveOpenInBrowser(local_state(),
                                                url_in_target_origin_, time_1_);
  ExpectUpdateUrlHandlers(kEmptyList,
                          R"([ {
    "display_url": "target-1.com",
    "has_origin_wildcard": false,
    "origin_key": "https://target-1.com",
    "path": "/*"
  } ])");
}

TEST_F(UrlHandlersHandlerTest, GetDisabledHandlersList_MultipleApps) {
  RegisterWebAppWithUrlHandlers(app_url_,
                                {apps::UrlHandlerInfo(target_origin_)});
  ExpectUpdateUrlHandlers(kEmptyList, kEmptyList);

  RegisterWebAppWithUrlHandlers(app2_url_,
                                {apps::UrlHandlerInfo(target_origin_)});
  ExpectUpdateUrlHandlers(kEmptyList, kEmptyList);

  // Save user choice to open in browser.
  web_app::url_handler_prefs::SaveOpenInBrowser(local_state(),
                                                url_in_target_origin_, time_1_);

  // WebUI should show the entry to open this origin+path in the browser only
  // once even though it is written to multiple apps.
  constexpr char kDisabledHandlers[] = R"([ {
    "display_url": "target-1.com",
    "has_origin_wildcard": false,
    "origin_key": "https://target-1.com",
    "path": "/*"
  }])";
  ExpectUpdateUrlHandlers(kEmptyList, kDisabledHandlers);
}

}  // namespace settings
