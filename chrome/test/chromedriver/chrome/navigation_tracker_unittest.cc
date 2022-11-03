// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/browser_info.h"
#include "chrome/test/chromedriver/chrome/javascript_dialog_manager.h"
#include "chrome/test/chromedriver/chrome/navigation_tracker.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/stub_devtools_client.h"
#include "chrome/test/chromedriver/chrome/stub_web_view.h"
#include "chrome/test/chromedriver/chrome/web_view_impl.h"
#include "chrome/test/chromedriver/net/timeout.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void AssertPendingState(NavigationTracker* tracker,
                        bool expected_is_pending) {
  bool is_pending = !expected_is_pending;
  ASSERT_EQ(kOk, tracker->IsPendingNavigation(nullptr, &is_pending).code());
  ASSERT_EQ(expected_is_pending, is_pending);
}

class DeterminingLoadStateDevToolsClient : public StubDevToolsClient {
 public:
  DeterminingLoadStateDevToolsClient(
      bool has_empty_base_url,
      bool is_loading,
      const std::string& send_event_first,
      base::DictionaryValue* send_event_first_params)
      : has_empty_base_url_(has_empty_base_url),
        is_loading_(is_loading),
        send_event_first_(send_event_first),
        send_event_first_params_(send_event_first_params) {}

  ~DeterminingLoadStateDevToolsClient() override {}

  Status SendCommandAndGetResult(const std::string& method,
                                 const base::Value::Dict& params,
                                 base::Value* result) override {
    if (method == "DOM.getDocument") {
      base::Value result_dict(base::Value::Type::DICTIONARY);
      if (has_empty_base_url_) {
        result_dict.GetDict().SetByDottedPath("root.baseURL", "about:blank");
        result_dict.GetDict().SetByDottedPath("root.documentURL",
                                              "http://test");
      } else {
        result_dict.GetDict().SetByDottedPath("root.baseURL", "http://test");
        result_dict.GetDict().SetByDottedPath("root.documentURL",
                                              "http://test");
      }
      *result = std::move(result_dict);
      return Status(kOk);
    } else if (method == "Runtime.evaluate") {
      const std::string* expression = params.FindString("expression");
      if (expression) {
        base::Value result_dict(base::Value::Type::DICTIONARY);
        if (*expression == "1")
          result_dict.GetDict().SetByDottedPath("result.value", 1);
        else if (*expression == "document.readyState")
          result_dict.GetDict().SetByDottedPath("result.value", "loading");
        *result = std::move(result_dict);
        return Status(kOk);
      }
    }

    if (send_event_first_.length()) {
      for (DevToolsEventListener* listener : listeners_) {
        Status status = listener->OnEvent(
            this, send_event_first_, *send_event_first_params_);
        if (status.IsError())
          return status;
      }
    }

    base::Value result_dict(base::Value::Type::DICTIONARY);
    result_dict.GetDict().SetByDottedPath("result.value", is_loading_);
    *result = std::move(result_dict);
    return Status(kOk);
  }

 private:
  bool has_empty_base_url_;
  bool is_loading_;
  std::string send_event_first_;
  raw_ptr<base::DictionaryValue> send_event_first_params_;
};

class EvaluateScriptWebView : public StubWebView {
 public:
  explicit EvaluateScriptWebView(StatusCode code)
      : StubWebView("1"), result_(std::string()), code_(code) {}

  Status EvaluateScript(const std::string& frame,
                        const std::string& function,
                        const bool await_promise,
                        std::unique_ptr<base::Value>* result) override {
    base::Value value(result_);
    *result = base::Value::ToUniquePtrValue(value.Clone());
    return Status(code_);
  }

  void nextEvaluateScript(std::string result, StatusCode code) {
    result_ = result;
    code_ = code;
  }

 private:
  std::string result_;
  StatusCode code_;
};

}  // namespace

TEST(NavigationTracker, FrameLoadStartStop) {
  base::DictionaryValue dict;
  BrowserInfo browser_info;
  std::unique_ptr<DevToolsClient> client_uptr =
      std::make_unique<DeterminingLoadStateDevToolsClient>(
          false, true, std::string(), &dict);
  DevToolsClient* client_ptr = client_uptr.get();
  JavaScriptDialogManager dialog_manager(client_ptr, &browser_info);
  WebViewImpl web_view(client_ptr->GetId(), true, nullptr, &browser_info,
                       std::move(client_uptr), nullptr,
                       PageLoadStrategy::kNormal);
  NavigationTracker tracker(client_ptr, &web_view, &browser_info,
                            &dialog_manager);

  base::DictionaryValue params;
  params.GetDict().Set("frameId", client_ptr->GetId());

  ASSERT_EQ(
      kOk,
      tracker.OnEvent(client_ptr, "Page.frameStartedLoading", params).code());
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, true));
  ASSERT_EQ(kOk,
            tracker.OnEvent(client_ptr, "Page.loadEventFired", params).code());
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, false));
}

// When a frame fails to load due to (for example) a DNS resolution error, we
// can sometimes see two Page.frameStartedLoading events with only a single
// Page.loadEventFired event.
TEST(NavigationTracker, FrameLoadStartStartStop) {
  base::DictionaryValue dict;
  BrowserInfo browser_info;
  std::unique_ptr<DevToolsClient> client_uptr =
      std::make_unique<DeterminingLoadStateDevToolsClient>(
          false, true, std::string(), &dict);
  DevToolsClient* client_ptr = client_uptr.get();
  JavaScriptDialogManager dialog_manager(client_ptr, &browser_info);
  WebViewImpl web_view(client_ptr->GetId(), true, nullptr, &browser_info,
                       std::move(client_uptr), nullptr,
                       PageLoadStrategy::kNormal);
  NavigationTracker tracker(client_ptr, &web_view, &browser_info,
                            &dialog_manager);

  base::DictionaryValue params;
  params.GetDict().Set("frameId", client_ptr->GetId());

  ASSERT_EQ(
      kOk,
      tracker.OnEvent(client_ptr, "Page.frameStartedLoading", params).code());
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, true));
  ASSERT_EQ(
      kOk,
      tracker.OnEvent(client_ptr, "Page.frameStartedLoading", params).code());
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, true));
  ASSERT_EQ(kOk,
            tracker.OnEvent(client_ptr, "Page.loadEventFired", params).code());
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, false));
}

TEST(NavigationTracker, MultipleFramesLoad) {
  base::DictionaryValue dict;
  BrowserInfo browser_info;
  std::unique_ptr<DevToolsClient> client_uptr =
      std::make_unique<DeterminingLoadStateDevToolsClient>(
          false, true, std::string(), &dict);
  DevToolsClient* client_ptr = client_uptr.get();
  JavaScriptDialogManager dialog_manager(client_ptr, &browser_info);
  WebViewImpl web_view(client_ptr->GetId(), true, nullptr, &browser_info,
                       std::move(client_uptr), nullptr,
                       PageLoadStrategy::kNormal);
  NavigationTracker tracker(client_ptr, &web_view, &browser_info,
                            &dialog_manager);

  base::DictionaryValue params;
  std::string top_frame_id = client_ptr->GetId();
  params.GetDict().Set("frameId", top_frame_id);

  ASSERT_EQ(
      kOk,
      tracker.OnEvent(client_ptr, "Page.frameStartedLoading", params).code());

  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, true));
  params.SetString("frameId", "2");
  ASSERT_EQ(
      kOk,
      tracker.OnEvent(client_ptr, "Page.frameStartedLoading", params).code());

  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, true));
  params.GetDict().Set("frameId", "2");
  ASSERT_EQ(
      kOk,
      tracker.OnEvent(client_ptr, "Page.frameStoppedLoading", params).code());
  // Inner frame stops loading. loading_state_ should remain true
  // since top frame is still loading
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, true));
  params.GetDict().Set("frameId", top_frame_id);
  ASSERT_EQ(kOk,
            tracker.OnEvent(client_ptr, "Page.loadEventFired", params).code());

  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, false));
  params.GetDict().Set("frameId", "3");
  ASSERT_EQ(
      kOk,
      tracker.OnEvent(client_ptr, "Page.frameStoppedLoading", params).code());

  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, false));
  ASSERT_EQ(
      kOk,
      tracker.OnEvent(client_ptr, "Page.frameStartedLoading", params).code());

  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, false));
}

TEST(NavigationTracker, NavigationScheduledForOtherFrame) {
  base::DictionaryValue dict;
  BrowserInfo browser_info;
  std::unique_ptr<DevToolsClient> client_uptr =
      std::make_unique<DeterminingLoadStateDevToolsClient>(
          false, true, std::string(), &dict);
  DevToolsClient* client_ptr = client_uptr.get();
  JavaScriptDialogManager dialog_manager(client_ptr, &browser_info);
  WebViewImpl web_view(client_ptr->GetId(), true, nullptr, &browser_info,
                       std::move(client_uptr), nullptr,
                       PageLoadStrategy::kNormal);
  NavigationTracker tracker(client_ptr, NavigationTracker::kNotLoading,
                            &web_view, &browser_info, &dialog_manager);

  base::DictionaryValue params_scheduled;
  params_scheduled.GetDict().Set("delay", 0);
  params_scheduled.GetDict().Set("frameId", "other");

  ASSERT_EQ(kOk, tracker
                     .OnEvent(client_ptr, "Page.frameScheduledNavigation",
                              params_scheduled)
                     .code());
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, false));
}

TEST(NavigationTracker, CurrentFrameLoading) {
  base::DictionaryValue dict;
  BrowserInfo browser_info;
  std::unique_ptr<DevToolsClient> client_uptr =
      std::make_unique<DeterminingLoadStateDevToolsClient>(
          false, false, std::string(), &dict);
  DevToolsClient* client_ptr = client_uptr.get();
  JavaScriptDialogManager dialog_manager(client_ptr, &browser_info);
  EvaluateScriptWebView web_view(kOk);
  NavigationTracker tracker(client_ptr, &web_view, &browser_info,
                            &dialog_manager);

  base::DictionaryValue params;
  std::string top_frame_id = client_ptr->GetId();
  std::string current_frame_id = "2";
  params.GetDict().Set("frameId", current_frame_id);

  // verify initial state
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, true));

  // Trigger new frame initialization
  ASSERT_EQ(kOk,
            tracker.OnEvent(client_ptr, "Page.frameAttached", params).code());
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, true));

  // loading state should respond to events from new frame after SetFrame
  tracker.SetFrame(current_frame_id);
  web_view.nextEvaluateScript("uninitialized", kOk);
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, true));

  ASSERT_EQ(
      kOk,
      tracker.OnEvent(client_ptr, "Page.frameStartedLoading", params).code());
  web_view.nextEvaluateScript("loading", kOk);
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, true));
  ASSERT_EQ(
      kOk,
      tracker.OnEvent(client_ptr, "Page.frameStoppedLoading", params).code());
  web_view.nextEvaluateScript("complete", kOk);
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, false));

  // loading state should not respond to unknown frame events
  params.GetDict().Set("frameId", "4");
  ASSERT_EQ(
      kOk,
      tracker.OnEvent(client_ptr, "Page.frameStartedLoading", params).code());
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, false));
  ASSERT_EQ(
      kOk,
      tracker.OnEvent(client_ptr, "Page.frameStoppedLoading", params).code());
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, false));
}

TEST(NavigationTracker, FrameAttachDetach) {
  base::DictionaryValue dict;
  BrowserInfo browser_info;
  std::unique_ptr<DevToolsClient> client_uptr =
      std::make_unique<DeterminingLoadStateDevToolsClient>(
          false, false, std::string(), &dict);
  DevToolsClient* client_ptr = client_uptr.get();
  JavaScriptDialogManager dialog_manager(client_ptr, &browser_info);
  EvaluateScriptWebView web_view(kOk);
  NavigationTracker tracker(client_ptr, &web_view, &browser_info,
                            &dialog_manager);

  base::DictionaryValue params;
  std::string top_frame_id = client_ptr->GetId();
  std::string current_frame_id = "2";
  params.GetDict().Set("frameId", current_frame_id);

  // verify initial state
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, true));

  // Trigger invalid current frame
  tracker.SetFrame(current_frame_id);
  web_view.nextEvaluateScript("SetFrame before frameAttached", kOk);
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, false));

  ASSERT_EQ(kOk,
            tracker.OnEvent(client_ptr, "Page.frameAttached", params).code());
  web_view.nextEvaluateScript("frameAttached", kOk);
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, false));

  // Trigger frame switch to valid
  tracker.SetFrame(current_frame_id);
  web_view.nextEvaluateScript("SetFrame after frameAttached", kOk);
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, true));

  ASSERT_EQ(kOk,
            tracker.OnEvent(client_ptr, "Page.frameDetached", params).code());
  web_view.nextEvaluateScript("frameDetached", kOk);
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, false));
}

TEST(NavigationTracker, SetFrameNoFrame) {
  base::DictionaryValue dict;
  BrowserInfo browser_info;
  std::unique_ptr<DevToolsClient> client_uptr =
      std::make_unique<DeterminingLoadStateDevToolsClient>(
          false, false, std::string(), &dict);
  DevToolsClient* client_ptr = client_uptr.get();
  JavaScriptDialogManager dialog_manager(client_ptr, &browser_info);
  EvaluateScriptWebView web_view(kOk);
  NavigationTracker tracker(client_ptr, &web_view, &browser_info,
                            &dialog_manager);

  base::DictionaryValue params;
  std::string top_frame_id = client_ptr->GetId();
  web_view.nextEvaluateScript("uninitialized", kOk);
  ASSERT_NO_FATAL_FAILURE(tracker.SetFrame(std::string()));
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, true));
  ASSERT_NO_FATAL_FAILURE(tracker.SetFrame("2"));
  ASSERT_NO_FATAL_FAILURE(tracker.SetFrame(std::string()));
  params.GetDict().Set("frameId", top_frame_id);
  web_view.nextEvaluateScript("complete", kOk);
  ASSERT_EQ(
      kOk,
      tracker.OnEvent(client_ptr, "Page.frameStoppedLoading", params).code());
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, false));

  // loading state should not respond to unknown frame events
  params.SetString("frameId", "2");
  ASSERT_EQ(
      kOk,
      tracker.OnEvent(client_ptr, "Page.frameStartedLoading", params).code());
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, false));
  ASSERT_EQ(
      kOk,
      tracker.OnEvent(client_ptr, "Page.frameStoppedLoading", params).code());
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, false));
}

namespace {

class FailToEvalScriptDevToolsClient : public StubDevToolsClient {
 public:
  FailToEvalScriptDevToolsClient() : is_dom_getDocument_requested_(false) {}

  ~FailToEvalScriptDevToolsClient() override {}

  Status SendCommandAndGetResult(const std::string& method,
                                 const base::Value::Dict& params,
                                 base::Value* result) override {
    if (!is_dom_getDocument_requested_ && method == "DOM.getDocument") {
      is_dom_getDocument_requested_ = true;
      base::Value result_dict(base::Value::Type::DICTIONARY);
      result_dict.GetDict().SetByDottedPath("root.baseURL", "http://test");
      *result = std::move(result_dict);
      return Status(kOk);
    }
    EXPECT_STREQ("Runtime.evaluate", method.c_str());
    *result = base::Value(base::Value::Type::DICTIONARY);
    return Status(kUnknownError, "failed to eval script");
  }

 private:
  bool is_dom_getDocument_requested_;
};

}  // namespace

TEST(NavigationTracker, UnknownStateFailsToDetermineState) {
  BrowserInfo browser_info;
  std::unique_ptr<DevToolsClient> client_uptr =
      std::make_unique<FailToEvalScriptDevToolsClient>();
  DevToolsClient* client_ptr = client_uptr.get();
  JavaScriptDialogManager dialog_manager(client_ptr, &browser_info);
  WebViewImpl web_view(client_ptr->GetId(), true, nullptr, &browser_info,
                       std::move(client_uptr), nullptr,
                       PageLoadStrategy::kNormal);
  NavigationTracker tracker(client_ptr, &web_view, &browser_info,
                            &dialog_manager);

  bool is_pending;
  ASSERT_EQ(kUnknownError,
            tracker.IsPendingNavigation(nullptr, &is_pending).code());
}

TEST(NavigationTracker, UnknownStatePageNotLoadAtAll) {
  base::DictionaryValue dict;
  BrowserInfo browser_info;
  std::unique_ptr<DevToolsClient> client_uptr =
      std::make_unique<DeterminingLoadStateDevToolsClient>(
          true, true, std::string(), &dict);
  DevToolsClient* client_ptr = client_uptr.get();
  JavaScriptDialogManager dialog_manager(client_ptr, &browser_info);
  WebViewImpl web_view(client_ptr->GetId(), true, nullptr, &browser_info,
                       std::move(client_uptr), nullptr,
                       PageLoadStrategy::kNormal);
  NavigationTracker tracker(client_ptr, &web_view, &browser_info,
                            &dialog_manager);

  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, true));
}

TEST(NavigationTracker, UnknownStateForcesStart) {
  base::DictionaryValue dict;
  BrowserInfo browser_info;
  std::unique_ptr<DevToolsClient> client_uptr =
      std::make_unique<DeterminingLoadStateDevToolsClient>(
          false, true, std::string(), &dict);
  DevToolsClient* client_ptr = client_uptr.get();
  JavaScriptDialogManager dialog_manager(client_ptr, &browser_info);
  EvaluateScriptWebView web_view(kOk);
  NavigationTracker tracker(client_ptr, &web_view, &browser_info,
                            &dialog_manager);

  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, true));
}

TEST(NavigationTracker, UnknownStateForcesStartReceivesStop) {
  base::DictionaryValue dict;
  BrowserInfo browser_info;
  std::unique_ptr<DevToolsClient> client_uptr =
      std::make_unique<DeterminingLoadStateDevToolsClient>(
          false, true, std::string(), &dict);
  DevToolsClient* client_ptr = client_uptr.get();
  JavaScriptDialogManager dialog_manager(client_ptr, &browser_info);
  WebViewImpl web_view(client_ptr->GetId(), true, nullptr, &browser_info,
                       std::move(client_uptr), nullptr,
                       PageLoadStrategy::kNormal);
  NavigationTracker tracker(client_ptr, &web_view, &browser_info,
                            &dialog_manager);

  base::DictionaryValue params;
  params.GetDict().Set("frameId", client_ptr->GetId());
  ASSERT_EQ(kOk,
            tracker.OnEvent(client_ptr, "Page.loadEventFired", params).code());
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, false));
}

TEST(NavigationTracker, OnSuccessfulNavigate) {
  BrowserInfo browser_info;
  std::string version_string = "{\"Browser\": \"Chrome/44.0.2403.125\","
                               " \"WebKit-Version\": \"537.36 (@199461)\"}";
  ASSERT_TRUE(ParseBrowserInfo(version_string, &browser_info).IsOk());

  base::DictionaryValue dict;
  std::unique_ptr<DevToolsClient> client_uptr =
      std::make_unique<DeterminingLoadStateDevToolsClient>(
          false, true, std::string(), &dict);
  DevToolsClient* client_ptr = client_uptr.get();
  JavaScriptDialogManager dialog_manager(client_ptr, &browser_info);
  EvaluateScriptWebView web_view(kOk);
  NavigationTracker tracker(client_ptr, NavigationTracker::kNotLoading,
                            &web_view, &browser_info, &dialog_manager);

  base::DictionaryValue params;
  base::DictionaryValue result;
  result.GetDict().Set("frameId", client_ptr->GetId());
  web_view.nextEvaluateScript("loading", kOk);
  tracker.OnCommandSuccess(client_ptr, "Page.navigate", &result, Timeout());
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, true));
  web_view.nextEvaluateScript("complete", kOk);
  tracker.OnEvent(client_ptr, "Page.loadEventFired", params);
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, false));
}

TEST(NavigationTracker, OnNetworkErroredNavigate) {
  BrowserInfo browser_info;
  std::string version_string =
      "{\"Browser\": \"Chrome/44.0.2403.125\","
      " \"WebKit-Version\": \"537.36 (@199461)\"}";
  ASSERT_TRUE(ParseBrowserInfo(version_string, &browser_info).IsOk());

  base::DictionaryValue dict;
  std::unique_ptr<DevToolsClient> client_uptr =
      std::make_unique<DeterminingLoadStateDevToolsClient>(
          false, true, std::string(), &dict);
  DevToolsClient* client_ptr = client_uptr.get();
  JavaScriptDialogManager dialog_manager(client_ptr, &browser_info);
  EvaluateScriptWebView web_view(kOk);
  NavigationTracker tracker(client_ptr, NavigationTracker::kNotLoading,
                            &web_view, &browser_info, &dialog_manager);

  base::DictionaryValue params;
  base::DictionaryValue result;
  result.GetDict().Set("frameId", client_ptr->GetId());
  result.GetDict().Set("errorText", "net::ERR_PROXY_CONNECTION_FAILED");
  web_view.nextEvaluateScript("loading", kOk);
  ASSERT_NE(
      kOk,
      tracker.OnCommandSuccess(client_ptr, "Page.navigate", &result, Timeout())
          .code());
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, false));
}

TEST(NavigationTracker, OnNonNetworkErroredNavigate) {
  BrowserInfo browser_info;
  std::string version_string =
      "{\"Browser\": \"Chrome/44.0.2403.125\","
      " \"WebKit-Version\": \"537.36 (@199461)\"}";
  ASSERT_TRUE(ParseBrowserInfo(version_string, &browser_info).IsOk());

  base::DictionaryValue dict;
  std::unique_ptr<DevToolsClient> client_uptr =
      std::make_unique<DeterminingLoadStateDevToolsClient>(
          false, true, std::string(), &dict);
  DevToolsClient* client_ptr = client_uptr.get();
  JavaScriptDialogManager dialog_manager(client_ptr, &browser_info);
  EvaluateScriptWebView web_view(kOk);
  NavigationTracker tracker(client_ptr, NavigationTracker::kNotLoading,
                            &web_view, &browser_info, &dialog_manager);

  base::DictionaryValue params;
  base::DictionaryValue result;
  result.GetDict().Set("frameId", client_ptr->GetId());
  result.GetDict().Set("errorText", "net::ERR_CERT_COMMON_NAME_INVALID");
  web_view.nextEvaluateScript("loading", kOk);
  tracker.OnCommandSuccess(client_ptr, "Page.navigate", &result, Timeout());
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, true));
  web_view.nextEvaluateScript("complete", kOk);
  tracker.OnEvent(client_ptr, "Page.loadEventFired", params);
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, false));
}

namespace {

class TargetClosedDevToolsClient : public StubDevToolsClient {
 public:
  TargetClosedDevToolsClient() {}

  ~TargetClosedDevToolsClient() override {}

  Status SendCommandAndGetResult(const std::string& method,
                                 const base::Value::Dict& params,
                                 base::Value* result) override {
    *result = base::Value(base::Value::Type::DICTIONARY);
    return Status(kUnknownError, "Inspected target navigated or closed");
  }
};

}  // namespace

TEST(NavigationTracker, TargetClosedInIsPendingNavigation) {
  BrowserInfo browser_info;
  std::unique_ptr<DevToolsClient> client_uptr =
      std::make_unique<TargetClosedDevToolsClient>();
  DevToolsClient* client_ptr = client_uptr.get();
  JavaScriptDialogManager dialog_manager(client_ptr, &browser_info);
  WebViewImpl web_view(client_ptr->GetId(), true, nullptr, &browser_info,
                       std::move(client_uptr), nullptr,
                       PageLoadStrategy::kNormal);
  NavigationTracker tracker(client_ptr, &web_view, &browser_info,
                            &dialog_manager);

  bool is_pending;
  ASSERT_EQ(kOk, tracker.IsPendingNavigation(nullptr, &is_pending).code());
  ASSERT_TRUE(is_pending);
}
