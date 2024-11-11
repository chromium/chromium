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
#include "chrome/test/chromedriver/test/integration_test.h"
#include "chrome/test/chromedriver/window_commands.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char kElementIdKey[] = "element-6066-11e4-a52e-4f735466cecf";

template <int Code>
testing::AssertionResult StatusCodeIs(const Status& status) {
  if (status.code() == Code) {
    return testing::AssertionSuccess();
  } else {
    return testing::AssertionFailure() << status.message();
  }
}

class SyncWebSocketWrapper : public SyncWebSocket {
 public:
  explicit SyncWebSocketWrapper(std::unique_ptr<SyncWebSocket> wrapped_socket)
      : wrapped_socket_(std::move(wrapped_socket)) {}

  ~SyncWebSocketWrapper() override = default;

  void SetId(const std::string& socket_id) override {
    wrapped_socket_->SetId(socket_id);
  }

  bool IsConnected() override { return wrapped_socket_->IsConnected(); }

  bool Connect(const GURL& url) override {
    return wrapped_socket_->Connect(url);
  }

  bool Send(const std::string& message) override {
    return wrapped_socket_->Send(message);
  }

  StatusCode ReceiveNextMessage(std::string* message,
                                const Timeout& timeout) override {
    return wrapped_socket_->ReceiveNextMessage(message, timeout);
  }

  bool HasNextMessage() override { return wrapped_socket_->HasNextMessage(); }

  void SetNotificationCallback(base::RepeatingClosure callback) override {
    wrapped_socket_->SetNotificationCallback(std::move(callback));
  }

 protected:
  std::unique_ptr<SyncWebSocket> wrapped_socket_;
};

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

enum class InterceptionMode {
  PostponeInterceptedResponse,
  DiscardInterceptedResponse,
};

// This socket does the following:
// * If the target detached event arrives before the response to
//   Input.dispatchMouseEvents the message sequence is unaltered.
// * If the Input.dispatchMouseEvents response arrives before the target
//   detached event the response is delayed until the event is received.
class ResponseInterceptingSocket : public SyncWebSocketWrapper {
 public:
  explicit ResponseInterceptingSocket(
      std::unique_ptr<SyncWebSocket> wrapped_socket)
      : SyncWebSocketWrapper(std::move(wrapped_socket)) {}

  void StartInterception(InterceptionMode interception_mode) {
    interception_started_ = true;
    if (interception_mode == InterceptionMode::DiscardInterceptedResponse) {
      discard_the_response_ = true;
    }
  }

  bool DetachIsDetected() const { return detach_is_detected_; }

  bool Send(const std::string& message) override {
    SniffCommand(message);
    return wrapped_socket_->Send(message);
  }

  bool HasNextMessage() override {
    return wrapped_socket_->HasNextMessage() ||
           (detach_is_detected_ && !delayed_response_.empty());
  }

  StatusCode ReceiveNextMessage(std::string* message,
                                const Timeout& timeout) override {
    while (true) {
      if (detach_is_detected_ && !delayed_response_.empty()) {
        *message = std::move(delayed_response_);
        delayed_response_ = "";
        return StatusCode::kOk;
      }
      std::string received_message;
      StatusCode code =
          wrapped_socket_->ReceiveNextMessage(&received_message, timeout);

      std::optional<base::Value> maybe_value = std::nullopt;
      if (code == StatusCode::kOk && interception_started_ &&
          !detach_is_detected_) {
        maybe_value = base::JSONReader::Read(received_message);
      }
      if (maybe_value && maybe_value->is_dict()) {
        // target detach is not intercepted yet.
        if (IsDispatchMouseEventResponse(maybe_value->GetDict())) {
          if (!discard_the_response_) {
            delayed_response_ = received_message;
          }
          continue;
        }
        if (IsTargetDetachedEvent(maybe_value->GetDict())) {
          detach_is_detected_ = true;
        }
      }
      *message = std::move(received_message);
      return code;
    }
  }

  void SniffCommand(const std::string& message) {
    if (!interception_started_ || awaited_response_id_ >= 0) {
      return;
    }
    std::optional<base::Value> maybe_value = base::JSONReader::Read(message);
    if (!maybe_value || !maybe_value->is_dict()) {
      return;
    }
    base::Value::Dict& dict = maybe_value->GetDict();
    const std::string* maybe_method = dict.FindString("method");
    if (maybe_method == nullptr ||
        *maybe_method != "Input.dispatchMouseEvent") {
      return;
    }
    std::optional<int> maybe_buttons =
        dict.FindIntByDottedPath("params.buttons");
    if (maybe_buttons.value_or(0) != 1) {
      return;
    }
    std::optional<int> maybe_cmd_id = dict.FindInt("id");
    awaited_response_id_ = maybe_cmd_id.value_or(-1);
  }

  bool IsTargetDetachedEvent(const base::Value::Dict& dict) {
    const std::string* maybe_method = dict.FindString("method");
    if (maybe_method == nullptr) {
      return false;
    }
    return *maybe_method == "Target.detachedFromTarget";
  }

  bool IsDispatchMouseEventResponse(const base::Value::Dict& dict) {
    if (awaited_response_id_ < 0) {
      return false;
    }
    std::optional<int> maybe_cmd_id = dict.FindInt("id");
    return awaited_response_id_ == maybe_cmd_id.value_or(-1);
  }

 private:
  bool interception_started_ = false;
  bool detach_is_detected_ = false;
  bool discard_the_response_ = false;
  int awaited_response_id_ = -1;
  std::string delayed_response_;
};

Status Navigate(Session& session, WebView& web_view, const GURL& url) {
  base::Value::Dict params;
  params.Set("url", url.spec());
  std::unique_ptr<base::Value> result;
  // Navigation timeout is initialized with the session defaults
  Timeout timeout;
  return ExecuteGet(&session, &web_view, params, &result, &timeout);
}

Status FindElement(Session& session,
                   WebView& web_view,
                   const std::string& strategy,
                   const std::string& selector,
                   Timeout& timeout,
                   base::Value::Dict& element_id) {
  base::Value::Dict params;
  std::unique_ptr<base::Value> tmp_result;
  params.Set("using", strategy);
  params.Set("value", selector);
  Status status = ExecuteFindElement(50, &session, &web_view, params,
                                     &tmp_result, &timeout);
  if (status.IsError()) {
    return status;
  }
  if (!tmp_result->is_dict()) {
    return Status{kUnknownError, "element_id is not a dictionary"};
  }
  element_id = std::move(tmp_result->GetDict());
  return status;
}

Status WaitForElement(Session& session,
                      WebView& web_view,
                      const std::string& strategy,
                      const std::string& selector,
                      Timeout& timeout,
                      base::Value::Dict& element_id) {
  Status status{kOk};
  do {
    if (status.IsError()) {
      base::PlatformThread::Sleep(base::Milliseconds(50));
    }
    status =
        FindElement(session, web_view, strategy, selector, timeout, element_id);
  } while (status.code() == kNoSuchElement ||
           status.code() == kNoSuchShadowRoot);
  return status;
}

Status FindElements(Session& session,
                    WebView& web_view,
                    const std::string& strategy,
                    const std::string& selector,
                    Timeout& timeout,
                    base::Value::List& element_id_list) {
  base::Value::Dict params;
  std::unique_ptr<base::Value> tmp_result;
  params.Set("using", strategy);
  params.Set("value", selector);
  Status status = ExecuteFindElements(50, &session, &web_view, params,
                                      &tmp_result, &timeout);
  if (status.IsError()) {
    return status;
  }
  if (!tmp_result->is_list()) {
    return Status{kUnknownError, "element_id is not a dictionary"};
  }
  element_id_list = std::move(tmp_result->GetList());
  return status;
}

Status SwitchToFrame(Session& session_,
                     WebView& web_view,
                     const base::Value::Dict& frame_id,
                     Timeout& timeout) {
  base::Value::Dict params;
  std::unique_ptr<base::Value> result;
  params.Set("id", frame_id.Clone());
  return ExecuteSwitchToFrame(&session_, &web_view, params, &result, &timeout);
}

Status ClickElement(Session& session,
                    WebView& web_view,
                    const base::Value::Dict& element_id) {
  std::unique_ptr<base::Value> result;
  const std::string* maybe_id = element_id.FindString(kElementIdKey);
  if (maybe_id == nullptr) {
    return Status{kUnknownError, "no element id was provided"};
  }
  return ExecuteClickElement(&session, &web_view, *maybe_id,
                             base::Value::Dict(), &result);
}

GURL ReplaceIPWithLocalhost(const GURL& original) {
  GURL::Replacements replacements;
  replacements.SetHostStr("localhost");
  return original.ReplaceComponents(replacements);
}

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

class RemoteToLocalNavigationTest : public SocketDecoratorTest {
 protected:
  void SetUp() override {
    SocketDecoratorTest::SetUp();
    GURL root_url = http_server_.http_url();
    http_server_.SetDataForPath("local.html", "<p>DONE!</p>");
    local_url_ = root_url.Resolve("local.html");
    http_server_.SetDataForPath(
        "remote.html", base::StringPrintf("<a href=\"%s\">To Local</a>",
                                          local_url_.spec().c_str()));
    remote_url_ = ReplaceIPWithLocalhost(root_url).Resolve("remote.html");
    http_server_.SetDataForPath(
        "main.html",
        base::StringPrintf("<iframe src=\"%s\">", remote_url_.spec().c_str()));
    main_url_ = root_url.Resolve("main.html");

    http_server_.SetDataForPath("away.html", "<span>AWAY!</span>");
    away_url_ = root_url.Resolve("away.html");
  }

  GURL local_url_;
  GURL remote_url_;
  GURL main_url_;
  GURL away_url_;
};

class DispatchingMouseEventsTest
    : public RemoteToLocalNavigationTest,
      public testing::WithParamInterface<InterceptionMode> {
 public:
  InterceptionMode InterceptionMode() const { return GetParam(); }
};

}  // namespace

TEST_P(DispatchingMouseEventsTest, TolerateTargetDetach) {
  ResponseInterceptingSocket* socket;
  ASSERT_TRUE(StatusOk(SetUpConnection(&socket)));
  Timeout timeout{base::Seconds(60)};
  std::unique_ptr<DevToolsClient> client;
  ASSERT_TRUE(StatusOk(AttachToFirstPage(*browser_client_, timeout, client)));
  WebViewImpl web_view(client->GetId(), true, nullptr, &browser_info_,
                       std::move(client), std::nullopt,
                       PageLoadStrategy::kNormal, true);
  web_view.AttachTo(browser_client_.get());

  ASSERT_TRUE(StatusOk(Navigate(session_, web_view, main_url_)));

  base::Value::Dict frame_id;
  ASSERT_TRUE(StatusOk(WaitForElement(session_, web_view, "tag name", "iframe",
                                      timeout, frame_id)));

  ASSERT_TRUE(StatusOk(SwitchToFrame(session_, web_view, frame_id, timeout)));

  base::Value::Dict anchor_id;
  ASSERT_TRUE(StatusOk(
      WaitForElement(session_, web_view, "tag name", "a", timeout, anchor_id)));

  socket->StartInterception(InterceptionMode());

  std::unique_ptr<base::Value> result;
  ASSERT_TRUE(StatusOk(ClickElement(session_, web_view, anchor_id)));
  ASSERT_TRUE(socket->DetachIsDetected());

  // Navigation has happened
  base::Value::Dict paragraph_id;
  ASSERT_TRUE(StatusOk(WaitForElement(session_, web_view, "tag name", "p",
                                      timeout, paragraph_id)));
}

INSTANTIATE_TEST_SUITE_P(
    Interception,
    DispatchingMouseEventsTest,
    ::testing::Values(InterceptionMode::DiscardInterceptedResponse,
                      InterceptionMode::PostponeInterceptedResponse));

namespace {

class NavigationCausingSocket : public SyncWebSocketWrapper {
 public:
  explicit NavigationCausingSocket(
      std::unique_ptr<SyncWebSocket> wrapped_socket)
      : SyncWebSocketWrapper(std::move(wrapped_socket)) {}

  void SetSkipCount(int count) { skip_count_ = count; }

  void SetFrameId(const std::string& frame_id) {
    frame_for_navigation_ = frame_id;
  }

  void SetSessionId(const std::string& session_id) {
    session_for_navigation_ = session_id;
  }

  void SetUrl(const GURL& url) { url_for_navigation_ = url; }

  bool IsSaturated() const { return skip_count_ < 0; }

  bool Send(const std::string& message) override {
    if (skip_count_ == 0) {
      base::Value::Dict params;
      EXPECT_TRUE(!url_for_navigation_.is_empty());
      params.Set("url", url_for_navigation_.spec());
      if (!frame_for_navigation_.empty()) {
        params.Set("frameId", frame_for_navigation_);
      }
      base::Value::Dict command;
      command.Set("id", next_cmd_id++);
      command.Set("method", "Page.navigate");
      command.Set("params", std::move(params));
      if (!session_for_navigation_.empty()) {
        command.Set("sessionId", session_for_navigation_);
      }
      std::string json;
      if (!base::JSONWriter::Write(command, &json)) {
        return false;
      }
      if (!wrapped_socket_->Send(json)) {
        return false;
      }
    }
    --skip_count_;
    return wrapped_socket_->Send(message);
  }

  bool InterceptResponse(const std::string& message) {
    std::optional<base::Value> maybe_response = base::JSONReader::Read(message);
    if (!maybe_response.has_value() || !maybe_response->is_dict()) {
      return false;
    }
    std::optional<int> maybe_id = maybe_response->GetDict().FindInt("id");
    return maybe_id.value_or(0) >= 1000'000'000;
  }

  StatusCode ReceiveNextMessage(std::string* message,
                                const Timeout& timeout) override {
    StatusCode code = StatusCode::kOk;
    std::string received_message;
    // This loop tries to remove the response to the injected command. Otherwise
    // DevToolsClientImpl gets confused.
    do {
      received_message.clear();
      code = wrapped_socket_->ReceiveNextMessage(&received_message, timeout);
    } while (code == StatusCode::kOk && InterceptResponse(received_message));

    if (code == StatusCode::kOk) {
      *message = std::move(received_message);
    }
    return code;
  }

  bool HasNextMessage() override { return wrapped_socket_->HasNextMessage(); }

 private:
  int skip_count_ = 1000'000'000;
  int next_cmd_id = 1000'000'000;
  std::string frame_for_navigation_;
  std::string session_for_navigation_;
  GURL url_for_navigation_;
};

class MouseClickNavigationInjectionTest
    : public RemoteToLocalNavigationTest,
      public testing::WithParamInterface<int> {
 public:
  static const int kSkipTestStep = 10;
};

}  // namespace

TEST_P(MouseClickNavigationInjectionTest, ClickWhileNavigating) {
  NavigationCausingSocket* socket;
  ASSERT_TRUE(StatusOk(SetUpConnection(&socket)));
  socket->SetUrl(away_url_);
  Timeout timeout{base::Seconds(100)};
  std::unique_ptr<DevToolsClient> client;
  ASSERT_TRUE(StatusOk(AttachToFirstPage(*browser_client_, timeout, client)));
  WebViewImpl web_view(client->GetId(), true, nullptr, &browser_info_,
                       std::move(client), std::nullopt,
                       PageLoadStrategy::kNormal, true);
  web_view.AttachTo(browser_client_.get());

  for (int skip_count = GetParam(); skip_count < GetParam() + kSkipTestStep;
       ++skip_count) {
    // avoid preliminary injected navigation
    socket->SetSkipCount(1000'000'000);
    ASSERT_TRUE(StatusOk(Navigate(session_, web_view, main_url_)))
        << "skip_count=" << skip_count;

    base::Value::Dict frame_id;
    ASSERT_TRUE(StatusOk(WaitForElement(session_, web_view, "tag name",
                                        "iframe", timeout, frame_id)))
        << "skip_count=" << skip_count;

    ASSERT_TRUE(StatusOk(SwitchToFrame(session_, web_view, frame_id, timeout)))
        << "skip_count=" << skip_count;

    socket->SetFrameId(session_.GetCurrentFrameId());
    WebView* child_web_view = web_view.GetFrameTracker()->GetTargetForFrame(
        session_.GetCurrentFrameId());
    ASSERT_NE(nullptr, child_web_view);
    const std::string session_id = child_web_view->GetSessionId();
    socket->SetSessionId(session_id);

    base::Value::Dict anchor_id;
    ASSERT_TRUE(StatusOk(WaitForElement(session_, web_view, "tag name", "a",
                                        timeout, anchor_id)))
        << "skip_count=" << skip_count;

    socket->SetSkipCount(skip_count);

    std::unique_ptr<base::Value> result;
    Status status = ClickElement(session_, web_view, anchor_id);

    // If the injected navigation has happened and it was detected then the only
    // two two acceptable outcomes are: kStaleElementReference and
    // kNoSuchExecutionContext. These will lead to retry by the
    // ExecuteWindowCommand function.
    // If there were no injection or the injected navigation has happened too
    // late, somewhere around Input.deispatchMouseEvent, and it was not detected
    // then the expected status code is kOk.

    if (socket->IsSaturated()) {
      std::set<StatusCode> actual_code = {status.code()};
      ASSERT_THAT(actual_code, testing::IsSubsetOf({kStaleElementReference,
                                                    kAbortedByNavigation, kOk}))
          << "skip_count=" << skip_count;
    } else {
      // The skip count is too large.
      ASSERT_TRUE(StatusOk(status)) << "skip_count=" << skip_count;
      break;
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    SkipRanges,
    MouseClickNavigationInjectionTest,
    ::testing::Range(0, 100, MouseClickNavigationInjectionTest::kSkipTestStep));

namespace {

class ScriptNavigateTest : public RemoteToLocalNavigationTest,
                           public testing::WithParamInterface<int> {
 public:
  static const int kSkipTestStep = 10;
};

}  // namespace

TEST_P(ScriptNavigateTest, ScriptNavigationWhileNavigating) {
  NavigationCausingSocket* socket;
  ASSERT_TRUE(StatusOk(SetUpConnection(&socket)));
  socket->SetUrl(away_url_);
  Timeout timeout{base::Seconds(100)};
  std::unique_ptr<DevToolsClient> client;
  ASSERT_TRUE(StatusOk(AttachToFirstPage(*browser_client_, timeout, client)));
  WebViewImpl web_view(client->GetId(), true, nullptr, &browser_info_,
                       std::move(client), std::nullopt,
                       PageLoadStrategy::kNormal, true);
  web_view.AttachTo(browser_client_.get());
  bool expect_no_injections = false;

  for (int skip_count = GetParam(); skip_count < GetParam() + kSkipTestStep;
       ++skip_count) {
    // avoid preliminary injected navigation
    socket->SetSkipCount(1000'000'000);
    ASSERT_TRUE(StatusOk(Navigate(session_, web_view, main_url_)))
        << "skip_count=" << skip_count;

    base::Value::Dict frame_id;
    ASSERT_TRUE(StatusOk(WaitForElement(session_, web_view, "tag name",
                                        "iframe", timeout, frame_id)))
        << "skip_count=" << skip_count;

    ASSERT_TRUE(StatusOk(SwitchToFrame(session_, web_view, frame_id, timeout)))
        << "skip_count=" << skip_count;

    socket->SetFrameId(session_.GetCurrentFrameId());
    WebView* child_web_view = web_view.GetFrameTracker()->GetTargetForFrame(
        session_.GetCurrentFrameId());
    ASSERT_NE(nullptr, child_web_view);
    const std::string session_id = child_web_view->GetSessionId();
    socket->SetSessionId(session_id);

    base::Value::Dict anchor_id;
    ASSERT_TRUE(StatusOk(WaitForElement(session_, web_view, "tag name", "a",
                                        timeout, anchor_id)))
        << "skip_count=" << skip_count;

    socket->SetSkipCount(skip_count);

    base::Value::Dict params;
    params.Set("script", "location.href=arguments[0]");
    base::Value::List args;
    args.Append(local_url_.spec().c_str());
    params.Set("args", std::move(args));

    std::unique_ptr<base::Value> result;
    Status status =
        ExecuteExecuteScript(&session_, &web_view, params, &result, &timeout);

    // If the injected navigation has happened and it was detected then the only
    // two two acceptable outcomes are: kScriptTimeout and
    // kAbortedByNavigation. The last one will lead to retry by the
    // ExecuteWindowCommand function.
    // If there were no injection or the injected navigation has happened too
    // late and it was not detected then the expected status code is kOk.

    if (socket->IsSaturated()) {
      ASSERT_FALSE(expect_no_injections);
      std::set<StatusCode> actual_code = {status.code()};
      ASSERT_THAT(actual_code, testing::IsSubsetOf(
                                   {kAbortedByNavigation, kScriptTimeout, kOk}))
          << "skip_count=" << skip_count;
      // The last injection can lead to kScriptTimeout because it is unclear to
      // ChromeDriver who initiated the navigation. However in this case the
      // bigger skip count will never saturate.
      expect_no_injections = status.code() == kScriptTimeout;
    } else {
      // The skip count is too large.
      ASSERT_TRUE(StatusOk(status)) << "skip_count=" << skip_count;
      break;
    }
  }
}

INSTANTIATE_TEST_SUITE_P(SkipRanges,
                         ScriptNavigateTest,
                         ::testing::Range(0,
                                          20,
                                          ScriptNavigateTest::kSkipTestStep));

namespace {

class FindElementsTest : public RemoteToLocalNavigationTest,
                         public testing::WithParamInterface<int> {
 public:
  static const int kSkipTestStep = 10;
};

}  // namespace

TEST_P(FindElementsTest, FindElementsWhileNavigating) {
  NavigationCausingSocket* socket;
  ASSERT_TRUE(StatusOk(SetUpConnection(&socket)));
  socket->SetUrl(away_url_);
  Timeout timeout{base::Seconds(100)};
  std::unique_ptr<DevToolsClient> client;
  ASSERT_TRUE(StatusOk(AttachToFirstPage(*browser_client_, timeout, client)));
  WebViewImpl web_view(client->GetId(), true, nullptr, &browser_info_,
                       std::move(client), std::nullopt,
                       PageLoadStrategy::kNormal, true);
  web_view.AttachTo(browser_client_.get());

  for (int skip_count = GetParam(); skip_count < GetParam() + kSkipTestStep;
       ++skip_count) {
    // avoid preliminary injected navigation
    socket->SetSkipCount(1000'000'000);
    ASSERT_TRUE(StatusOk(Navigate(session_, web_view, main_url_)))
        << "skip_count=" << skip_count;

    base::Value::Dict frame_id;
    ASSERT_TRUE(StatusOk(WaitForElement(session_, web_view, "tag name",
                                        "iframe", timeout, frame_id)))
        << "skip_count=" << skip_count;

    ASSERT_TRUE(StatusOk(SwitchToFrame(session_, web_view, frame_id, timeout)))
        << "skip_count=" << skip_count;

    socket->SetFrameId(session_.GetCurrentFrameId());
    WebView* child_web_view = web_view.GetFrameTracker()->GetTargetForFrame(
        session_.GetCurrentFrameId());
    ASSERT_NE(nullptr, child_web_view);
    const std::string session_id = child_web_view->GetSessionId();
    socket->SetSessionId(session_id);

    base::Value::Dict anchor_id;
    ASSERT_TRUE(StatusOk(WaitForElement(session_, web_view, "tag name", "a",
                                        timeout, anchor_id)))
        << "skip_count=" << skip_count;

    socket->SetSkipCount(skip_count);

    base::Value::List elemend_id_list;
    Status status = FindElements(session_, web_view, "tag name", "a", timeout,
                                 elemend_id_list);

    // If the injected navigation has happened and it was detected then the only
    // acceptable outcome is: kAbortedByNavigation. It will lead to retry by the
    // ExecuteWindowCommand function.
    // If there were no injection or the injected navigation has happened too
    // late and it was not detected then the expected status code is kOk.

    if (socket->IsSaturated()) {
      std::set<StatusCode> actual_code = {status.code()};
      ASSERT_THAT(actual_code, testing::IsSubsetOf({kAbortedByNavigation, kOk}))
          << "skip_count=" << skip_count;
    } else {
      // The skip count is too large.
      ASSERT_TRUE(StatusOk(status)) << "skip_count=" << skip_count;
      break;
    }
  }
}

INSTANTIATE_TEST_SUITE_P(SkipRanges,
                         FindElementsTest,
                         ::testing::Range(0,
                                          20,
                                          FindElementsTest::kSkipTestStep));
