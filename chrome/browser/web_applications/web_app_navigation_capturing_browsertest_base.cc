// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_navigation_capturing_browsertest_base.h"

#include <utility>

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/run_until.h"
#include "build/buildflag.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"

using blink::mojom::ManifestLaunchHandler_ClientMode;

namespace web_app {

WebAppNavigationCapturingBrowserTestBase::
    WebAppNavigationCapturingBrowserTestBase() {
  std::map<std::string, std::string> parameters;
  parameters["link_capturing_state"] = "reimpl_default_on";
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kPwaNavigationCapturing, parameters);

#if BUILDFLAG(IS_CHROMEOS)
  // TODO(crbug.com/366547977): CrOS doesn't use our nav capturing
  // implementation.
  NOTREACHED();
#endif
}

WebAppNavigationCapturingBrowserTestBase::
    ~WebAppNavigationCapturingBrowserTestBase() = default;

Browser*
WebAppNavigationCapturingBrowserTestBase::CallWindowOpenExpectNewBrowser(
    content::WebContents* contents,
    const GURL& url,
    bool with_opener) {
  ui_test_utils::BrowserChangeObserver browser_observer(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  CallWindowOpen(contents, url, with_opener);
  return browser_observer.Wait();
}

content::WebContents*
WebAppNavigationCapturingBrowserTestBase::CallWindowOpenExpectNewTab(
    content::WebContents* contents,
    const GURL& url,
    bool with_opener) {
  ui_test_utils::TabAddedWaiter tab_added_waiter(browser());
  CallWindowOpen(contents, url, with_opener);
  content::WebContents* new_contents = tab_added_waiter.Wait();
  return new_contents;
}

void WebAppNavigationCapturingBrowserTestBase::CallWindowOpen(
    content::WebContents* contents,
    const GURL& url,
    bool with_opener) {
  constexpr char kScript[] = R"(
          window.open($1, '_blank', 'noopener');
    )";
  constexpr char kScriptWithOpener[] = R"(
          window.open($1, '_blank', 'opener,popup=false');
    )";
  std::string script =
      content::JsReplace(with_opener ? kScriptWithOpener : kScript, url);
  DLOG(INFO) << "Executing script: " << script;
  EXPECT_TRUE(content::ExecJs(contents, script));
}

void WebAppNavigationCapturingBrowserTestBase::WaitForLaunchParams(
    content::WebContents* contents,
    int min_launch_params_to_wait_for) {
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_TRUE(base::test::RunUntil([&] {
    return content::EvalJs(
               contents,
               "launchParamsTargetUrls && launchParamsTargetUrls.length >= " +
                   base::NumberToString(min_launch_params_to_wait_for))
        .ExtractBool();
  }));
}

std::vector<GURL> WebAppNavigationCapturingBrowserTestBase::GetLaunchParams(
    content::WebContents* contents,
    const std::string& params) {
  std::vector<GURL> launch_params;
  content::EvalJsResult launchParamsResults =
      content::EvalJs(contents->GetPrimaryMainFrame(),
                      "'" + params + "' in window ? " + params + " : []");
  EXPECT_THAT(launchParamsResults, content::EvalJsResult::IsOk());
  base::Value::List launchParamsTargetUrls = launchParamsResults.ExtractList();
  if (!launchParamsTargetUrls.empty()) {
    for (const base::Value& url : launchParamsTargetUrls) {
      launch_params.push_back(GURL(url.GetString()));
    }
  }
  return launch_params;
}

}  // namespace web_app
