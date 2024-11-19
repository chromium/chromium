// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/devtools_client_impl.h"
#include "chrome/test/chromedriver/chrome/frame_tracker.h"
#include "chrome/test/chromedriver/chrome/page_load_strategy.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/target_utils.h"
#include "chrome/test/chromedriver/chrome/web_view_impl.h"
#include "chrome/test/chromedriver/chrome/web_view_info.h"
#include "chrome/test/chromedriver/element_commands.h"
#include "chrome/test/chromedriver/element_util.h"
#include "chrome/test/chromedriver/logging.h"
#include "chrome/test/chromedriver/net/sync_websocket.h"
#include "chrome/test/chromedriver/session.h"
#include "chrome/test/chromedriver/test/command_injecting_socket.h"
#include "chrome/test/chromedriver/test/integration_test.h"
#include "chrome/test/chromedriver/test/sync_websocket_wrapper.h"
#include "chrome/test/chromedriver/window_commands.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class SocketDecoratorTest : public IntegrationTest {
 protected:
  SocketDecoratorTest() = default;

  template <class SocketDecorator>
  Status SetUpConnection(SocketDecorator** decorator_ptr) {
    Status status{kOk};
    status = pipe_builder_.BuildSocket();
    if (status.IsError()) {
      return status;
    }
    std::unique_ptr<SyncWebSocket> original_socket = pipe_builder_.TakeSocket();
    std::unique_ptr<SocketDecorator> decorator =
        std::make_unique<SocketDecorator>(std::move(original_socket));
    SocketDecorator* ptr = decorator.get();
    if (!decorator->Connect(GURL())) {
      return Status{kSessionNotCreated,
                    "failed to set up the connection with the browser"};
    }
    status = browser_client_->SetSocket(std::move(decorator));
    if (status.IsError()) {
      return status;
    }
    Timeout timeout{base::Seconds(10)};
    base::Value::Dict result;
    status = browser_client_->SendCommandAndGetResultWithTimeout(
        "Browser.getVersion", base::Value::Dict(), &timeout, &result);
    if (status.IsError()) {
      return status;
    }
    status = browser_info_.FillFromBrowserVersionResponse(result);
    if (status.IsError()) {
      return status;
    }
    pipe_builder_.CloseChildEndpoints();
    *decorator_ptr = ptr;
    return Status{kOk};
  }
};

Status AttachToFirstPage(DevToolsClient& browser_client,
                         Timeout& timeout,
                         std::unique_ptr<DevToolsClient>& client) {
  Status status = target_utils::WaitForPage(browser_client, timeout);
  if (status.IsError()) {
    return status;
  }
  WebViewsInfo views_info;
  status = target_utils::GetWebViewsInfo(browser_client, &timeout, views_info);
  if (status.IsError()) {
    return status;
  }
  const WebViewInfo* view_info = views_info.FindFirst(WebViewInfo::kPage);
  if (view_info == nullptr) {
    return Status{kNoSuchWindow, "first tab not found"};
  }
  return target_utils::AttachToPageTarget(browser_client, view_info->id,
                                          &timeout, client);
}

class PageCrashTest : public SocketDecoratorTest,
                      public testing::WithParamInterface<int> {
 public:
  static const int kSkipTestStep = 10;
};

}  // namespace

TEST_P(PageCrashTest, WaitForNavigation) {
  CommandInjectingSocket* socket;
  ASSERT_TRUE(StatusOk(SetUpConnection(&socket)));
  socket->SetMethod("Page.crash");
  Timeout timeout{base::Seconds(100)};
  http_server_.SetDataForPath("away.html", "<span>AWAY!</span>");
  GURL away_url = http_server_.http_url().Resolve("away.html");

  for (int skip_count = GetParam(); skip_count < GetParam() + kSkipTestStep;
       ++skip_count) {
    std::unique_ptr<DevToolsClient> client;
    ASSERT_TRUE(StatusOk(AttachToFirstPage(*browser_client_, timeout, client)));
    WebViewImpl web_view(client->GetId(), true, nullptr, &browser_info_,
                         std::move(client), std::nullopt,
                         PageLoadStrategy::kNormal, true);
    web_view.AttachTo(browser_client_.get());
    socket->SetSessionId(web_view.GetSessionId());

    web_view.Load(away_url.spec(), &timeout);

    socket->SetSkipCount(skip_count);
    Status status = web_view.WaitForPendingNavigations("", timeout, false);
    const bool is_saturated = socket->IsSaturated();

    socket->SetSkipCount(1'000'000'000);
    base::Value::Dict params;
    params.Set("url", "data:,");
    ASSERT_TRUE(StatusOk(browser_client_->SendCommand("Target.createTarget",
                                                      std::move(params))));
    params = base::Value::Dict();
    params.Set("targetId", web_view.GetId());
    ASSERT_TRUE(StatusOk(
        browser_client_->SendCommand("Target.closeTarget", std::move(params))));

    if (is_saturated) {
      std::set<StatusCode> actual_code = {status.code()};
      ASSERT_THAT(actual_code, testing::IsSubsetOf({kTabCrashed}))
          << "skip_count=" << skip_count;
    } else {
      // The skip count is too large.
      ASSERT_TRUE(StatusOk(status)) << "skip_count=" << skip_count;
      break;
    }
  }
}

INSTANTIATE_TEST_SUITE_P(SkipRanges,
                         PageCrashTest,
                         ::testing::Range(0, 20, PageCrashTest::kSkipTestStep));

namespace {

class BrowserCrashTest : public SocketDecoratorTest,
                         public testing::WithParamInterface<int> {
 public:
  static const int kSkipTestStep = 1;
};

}  // namespace

TEST_P(BrowserCrashTest, WaitForNavigation) {
  CommandInjectingSocket* socket;
  ASSERT_TRUE(StatusOk(SetUpConnection(&socket)));
  socket->SetMethod("Browser.crash");
  Timeout timeout{base::Seconds(100)};
  http_server_.SetDataForPath("away.html", "<span>AWAY!</span>");
  GURL away_url = http_server_.http_url().Resolve("away.html");

  std::unique_ptr<DevToolsClient> client;
  ASSERT_TRUE(StatusOk(AttachToFirstPage(*browser_client_, timeout, client)));
  WebViewImpl web_view(client->GetId(), true, nullptr, &browser_info_,
                       std::move(client), std::nullopt,
                       PageLoadStrategy::kNormal, true);
  web_view.AttachTo(browser_client_.get());
  web_view.Load(away_url.spec(), &timeout);

  socket->SetSkipCount(GetParam());
  Status status = web_view.WaitForPendingNavigations("", timeout, false);

  if (socket->IsSaturated()) {
    // It can happen that the injected command has not taken effect before the
    // next command. Therefore kOk is an acceptable status.
    std::set<StatusCode> actual_code = {status.code()};
    ASSERT_THAT(actual_code, testing::IsSubsetOf({kDisconnected, kOk}))
        << "skip_count=" << GetParam();
  } else {
    // The skip count is too large.
    ASSERT_TRUE(StatusOk(status)) << "skip_count=" << GetParam();
  }
}

INSTANTIATE_TEST_SUITE_P(SkipRanges,
                         BrowserCrashTest,
                         ::testing::Range(0,
                                          5,
                                          BrowserCrashTest::kSkipTestStep));
