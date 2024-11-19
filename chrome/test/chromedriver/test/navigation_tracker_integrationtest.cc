// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/navigation_tracker.h"
#include "chrome/test/chromedriver/chrome/target_utils.h"
#include "chrome/test/chromedriver/chrome/web_view_impl.h"
#include "chrome/test/chromedriver/chrome/web_view_info.h"
#include "chrome/test/chromedriver/test/integration_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class NavigationTrackerTest : public IntegrationTest {
 protected:
  NavigationTrackerTest() = default;
};

}  // namespace

TEST_F(NavigationTrackerTest, SimpleNavigation) {
  Status status{kOk};
  ASSERT_TRUE(StatusOk(SetUpConnection()));
  Timeout timeout{base::Seconds(60)};
  status = target_utils::WaitForTab(*browser_client_, timeout);
  ASSERT_TRUE(StatusOk(status));
  WebViewsInfo views_info;
  status = target_utils::GetTopLevelViewsInfo(*browser_client_, &timeout,
                                              views_info);
  ASSERT_TRUE(StatusOk(status));
  const WebViewInfo* view_info = views_info.FindFirst(WebViewInfo::kTab);
  ASSERT_NE(view_info, nullptr);
  std::unique_ptr<DevToolsClient> client;
  status = target_utils::AttachToPageOrTabTarget(
      *browser_client_, view_info->id, &timeout, client, /*is_tab=*/true);
  ASSERT_TRUE(StatusOk(status));
  std::unique_ptr<WebViewImpl> tab_view = WebViewImpl::CreateTabTargetWebView(
      view_info->id, true, &browser_info_, std::move(client), std::nullopt,
      PageLoadStrategy::kNormal, true, nullptr);
  tab_view->AttachTo(browser_client_.get());
  tab_view->WaitForPendingActivePage(timeout);
  WebViewImpl web_view(view_info->id, true, nullptr, tab_view.get(),
                       &browser_info_, std::move(client), std::nullopt,
                       PageLoadStrategy::kNormal, true);
  web_view.AttachTo(browser_client_.get());

  http_server_.SetDataForPath("test.html", "<span>DONE!</span>");

  GURL root_url = http_server_.http_url();
  EXPECT_TRUE(
      StatusOk(web_view.Load(root_url.Resolve("test.html").spec(), &timeout)));
  web_view.WaitForPendingNavigations("", timeout, true);
  std::unique_ptr<base::Value> result;
  CallFunctionOptions options;
  options.include_shadow_root = false;
  EXPECT_TRUE(StatusOk(web_view.CallFunctionWithTimeout(
      "",
      "function(){"
      "  return document.querySelector('span').textContent;"
      "}",
      base::Value::List(), timeout.GetRemainingTime(), options, &result)));
  ASSERT_TRUE(result->is_string());
  const std::string text = result->GetString();
  EXPECT_EQ("DONE!", text);
}
