// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <memory>

#include "chrome/browser/ui/webui/settings/settings_clear_browsing_data_handler.h"

#include "base/values.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

constexpr char kGetInstalledApps[] = "getInstalledApps";
constexpr char kWebUiFunctionName[] = "webUiCallbackName";

}  // namespace

namespace settings {

class TestingClearBrowsingDataHandler : public ClearBrowsingDataHandler {
 public:
  TestingClearBrowsingDataHandler(content::WebUI* webui, Profile* profile)
      : ClearBrowsingDataHandler(webui, profile) {
    set_web_ui(webui);
  }
  TestingClearBrowsingDataHandler& operator=(
      const TestingClearBrowsingDataHandler&) = delete;
  TestingClearBrowsingDataHandler(const TestingClearBrowsingDataHandler&) =
      delete;
};

class ClearBrowsingDataHandlerBrowserTest
    : public web_app::WebAppControllerBrowserTest {
 public:
  ClearBrowsingDataHandlerBrowserTest() = default;
  ~ClearBrowsingDataHandlerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    WebAppControllerBrowserTest::SetUpOnMainThread();

    handler_ = std::make_unique<TestingClearBrowsingDataHandler>(
        web_ui(), browser()->profile());
    handler_->AllowJavascriptForTesting();
    handler_->RegisterMessages();
  }

  void TearDownOnMainThread() override { handler_.reset(); }

 protected:
  web_app::AppId InstallAndLaunchApp(GURL& url) {
    auto app_id = InstallPWA(url);

    ui_test_utils::UrlLoadObserver url_observer(
        url, content::NotificationService::AllSources());

    auto* app_browser =
        ClearBrowsingDataHandlerBrowserTest::LaunchWebAppBrowser(app_id);
    url_observer.Wait();
    DCHECK(app_browser);
    DCHECK(app_browser != browser());

    return app_id;
  }

  ClearBrowsingDataHandler* handler() { return handler_.get(); }
  content::TestWebUI* web_ui() { return &web_ui_; }

 private:
  std::unique_ptr<ClearBrowsingDataHandler> handler_;
  content::TestWebUI web_ui_;
};

IN_PROC_BROWSER_TEST_F(ClearBrowsingDataHandlerBrowserTest, GetInstalledApps) {
  GURL url(https_server()->GetURL("/title1.html"));
  InstallAndLaunchApp(url);
  base::Value::List args;
  args.Append(kWebUiFunctionName);
  args.Append(1);

  web_ui()->HandleReceivedMessage(kGetInstalledApps, args);
  const content::TestWebUI::CallData& call_data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", call_data.function_name());
  EXPECT_EQ(kWebUiFunctionName, call_data.arg1()->GetString());
  ASSERT_TRUE(call_data.arg2()->GetBool());

  // Get results from JS callback.
  const base::Value::List& result = call_data.arg3()->GetList();
  ASSERT_EQ(1U, result.size());
  auto& installed_app = result.back();
  ASSERT_EQ(url.host(), *(installed_app.FindStringKey("registerableDomain")));
}

}  // namespace settings
