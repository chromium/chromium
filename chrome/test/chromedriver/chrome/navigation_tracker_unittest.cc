// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/compiler_specific.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/browser_info.h"
#include "chrome/test/chromedriver/chrome/javascript_dialog_manager.h"
#include "chrome/test/chromedriver/chrome/navigation_tracker.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/stub_devtools_client.h"
#include "chrome/test/chromedriver/net/timeout.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void AssertPendingState(NavigationTracker* tracker,
                        const std::string& frame_id,
                        bool expected_is_pending) {
  bool is_pending = !expected_is_pending;
  ASSERT_EQ(
      kOk, tracker->IsPendingNavigation(frame_id, nullptr, &is_pending).code());
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

  Status SendCommandAndGetResult(
      const std::string& method,
      const base::DictionaryValue& params,
      std::unique_ptr<base::DictionaryValue>* result) override {
    if (method == "DOM.getDocument") {
      base::DictionaryValue result_dict;
      if (has_empty_base_url_) {
        result_dict.SetString("root.baseURL", "about:blank");
        result_dict.SetString("root.documentURL", "http://test");
      } else {
        result_dict.SetString("root.baseURL", "http://test");
        result_dict.SetString("root.documentURL", "http://test");
      }
      result->reset(result_dict.DeepCopy());
      return Status(kOk);
    } else if (method == "Runtime.evaluate") {
      std::string expression;
      if (params.GetString("expression", &expression)) {
        base::DictionaryValue result_dict;
        if (expression == "1")
          result_dict.SetInteger("result.value", 1);
        else if (expression == "document.readyState")
          result_dict.SetString("result.value", "loading");
        result->reset(result_dict.DeepCopy());
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

    base::DictionaryValue result_dict;
    result_dict.SetBoolean("result.value", is_loading_);
    result->reset(result_dict.DeepCopy());
    return Status(kOk);
  }

 private:
  bool has_empty_base_url_;
  bool is_loading_;
  std::string send_event_first_;
  base::DictionaryValue* send_event_first_params_;
};

}  // namespace

TEST(NavigationTracker, FrameLoadStartStop) {
  base::DictionaryValue dict;
  DeterminingLoadStateDevToolsClient client(false, true, std::string(), &dict);
  BrowserInfo browser_info;
  JavaScriptDialogManager dialog_manager(&client, &browser_info);
  NavigationTracker tracker(&client, &browser_info, &dialog_manager);

  base::DictionaryValue params;
  params.SetString("frameId", client.GetId());

  ASSERT_EQ(
      kOk, tracker.OnEvent(&client, "Page.frameStartedLoading", params).code());
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, client.GetId(), true));
  ASSERT_EQ(kOk,
            tracker.OnEvent(&client, "Page.loadEventFired", params).code());
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, client.GetId(), false));
}

// When a frame fails to load due to (for example) a DNS resolution error, we
// can sometimes see two Page.frameStartedLoading events with only a single
// Page.loadEventFired event.
TEST(NavigationTracker, FrameLoadStartStartStop) {
  base::DictionaryValue dict;
  DeterminingLoadStateDevToolsClient client(false, true, std::string(), &dict);
  BrowserInfo browser_info;
  JavaScriptDialogManager dialog_manager(&client, &browser_info);
  NavigationTracker tracker(&client, &browser_info, &dialog_manager);

  base::DictionaryValue params;
  params.SetString("frameId", client.GetId());

  ASSERT_EQ(
      kOk, tracker.OnEvent(&client, "Page.frameStartedLoading", params).code());
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, client.GetId(), true));
  ASSERT_EQ(
      kOk, tracker.OnEvent(&client, "Page.frameStartedLoading", params).code());
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, client.GetId(), true));
  ASSERT_EQ(kOk,
            tracker.OnEvent(&client, "Page.loadEventFired", params).code());
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, client.GetId(), false));
}

TEST(NavigationTracker, MultipleFramesLoad) {
  base::DictionaryValue dict;
  DeterminingLoadStateDevToolsClient client(false, true, std::string(), &dict);
  BrowserInfo browser_info;
  JavaScriptDialogManager dialog_manager(&client, &browser_info);
  NavigationTracker tracker(&client, &browser_info, &dialog_manager);
  base::DictionaryValue params;

  std::string top_frame_id = client.GetId();
  params.SetString("frameId", top_frame_id);
  ASSERT_EQ(
      kOk, tracker.OnEvent(&client, "Page.frameStartedLoading", params).code());

  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, client.GetId(), true));
  params.SetString("frameId", "2");
  ASSERT_EQ(
      kOk, tracker.OnEvent(&client, "Page.frameStartedLoading", params).code());

  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, "2", true));
  params.SetString("frameId", "2");
  ASSERT_EQ(
      kOk, tracker.OnEvent(&client, "Page.frameStoppedLoading", params).code());
  // Inner frame stops loading. loading_state_ should remain true
  // since top frame is still loading
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, "2", true));
  params.SetString("frameId", top_frame_id);
  ASSERT_EQ(kOk,
            tracker.OnEvent(&client, "Page.loadEventFired", params).code());

  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, top_frame_id, false));
  params.SetString("frameId", "3");
  ASSERT_EQ(
      kOk, tracker.OnEvent(&client, "Page.frameStoppedLoading", params).code());

  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, "3", false));
  ASSERT_EQ(
      kOk, tracker.OnEvent(&client, "Page.frameStartedLoading", params).code());

  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, "3", false));
}

TEST(NavigationTracker, NavigationScheduledForOtherFrame) {
  base::DictionaryValue dict;
  DeterminingLoadStateDevToolsClient client(false, true, std::string(), &dict);
  BrowserInfo browser_info;
  JavaScriptDialogManager dialog_manager(&client, &browser_info);
  NavigationTracker tracker(
      &client, NavigationTracker::kNotLoading, &browser_info, &dialog_manager);
  base::DictionaryValue params_scheduled;
  params_scheduled.SetInteger("delay", 0);
  params_scheduled.SetString("frameId", "other");

  ASSERT_EQ(
      kOk,
      tracker.OnEvent(
          &client, "Page.frameScheduledNavigation", params_scheduled).code());
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, client.GetId(), false));
}

namespace {

class FailToEvalScriptDevToolsClient : public StubDevToolsClient {
 public:
  FailToEvalScriptDevToolsClient() : is_dom_getDocument_requested_(false) {}

  ~FailToEvalScriptDevToolsClient() override {}

  Status SendCommandAndGetResult(
      const std::string& method,
      const base::DictionaryValue& params,
      std::unique_ptr<base::DictionaryValue>* result) override {
    if (!is_dom_getDocument_requested_ && method == "DOM.getDocument") {
      is_dom_getDocument_requested_ = true;
      base::DictionaryValue result_dict;
      result_dict.SetString("root.baseURL", "http://test");
      result->reset(result_dict.DeepCopy());
      return Status(kOk);
    }
    EXPECT_STREQ("Runtime.evaluate", method.c_str());
    return Status(kUnknownError, "failed to eval script");
  }

 private:
  bool is_dom_getDocument_requested_;
};

}  // namespace

TEST(NavigationTracker, UnknownStateFailsToDetermineState) {
  FailToEvalScriptDevToolsClient client;
  BrowserInfo browser_info;
  JavaScriptDialogManager dialog_manager(&client, &browser_info);
  NavigationTracker tracker(&client, &browser_info, &dialog_manager);
  bool is_pending;
  ASSERT_EQ(kUnknownError,
            tracker.IsPendingNavigation("f", nullptr, &is_pending).code());
}

TEST(NavigationTracker, UnknownStatePageNotLoadAtAll) {
  base::DictionaryValue params;
  DeterminingLoadStateDevToolsClient client(
      true, true, std::string(), &params);
  BrowserInfo browser_info;
  JavaScriptDialogManager dialog_manager(&client, &browser_info);
  NavigationTracker tracker(&client, &browser_info, &dialog_manager);
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, "f", true));
}

TEST(NavigationTracker, UnknownStateForcesStart) {
  base::DictionaryValue params;
  DeterminingLoadStateDevToolsClient client(
      false, true, std::string(), &params);
  BrowserInfo browser_info;
  JavaScriptDialogManager dialog_manager(&client, &browser_info);
  NavigationTracker tracker(&client, &browser_info, &dialog_manager);
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, client.GetId(), true));
}

TEST(NavigationTracker, UnknownStateForcesStartReceivesStop) {
  base::DictionaryValue dict;
  DeterminingLoadStateDevToolsClient client(false, true, std::string(), &dict);
  BrowserInfo browser_info;
  JavaScriptDialogManager dialog_manager(&client, &browser_info);
  NavigationTracker tracker(&client, &browser_info, &dialog_manager);

  base::DictionaryValue params;
  params.SetString("frameId", client.GetId());
  ASSERT_EQ(kOk,
            tracker.OnEvent(&client, "Page.loadEventFired", params).code());
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, client.GetId(), false));
}

TEST(NavigationTracker, OnSuccessfulNavigate) {
  base::DictionaryValue params;
  DeterminingLoadStateDevToolsClient client(
      false, true, std::string(), &params);
  BrowserInfo browser_info;
  std::string version_string = "{\"Browser\": \"Chrome/44.0.2403.125\","
                               " \"WebKit-Version\": \"537.36 (@199461)\"}";
  ASSERT_TRUE(ParseBrowserInfo(version_string, &browser_info).IsOk());
  JavaScriptDialogManager dialog_manager(&client, &browser_info);
  NavigationTracker tracker(
      &client, NavigationTracker::kNotLoading, &browser_info, &dialog_manager);
  base::DictionaryValue result;
  result.SetString("frameId", client.GetId());
  tracker.OnCommandSuccess(&client, "Page.navigate", result, Timeout());
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, client.GetId(), true));
  tracker.OnEvent(&client, "Page.loadEventFired", params);
  ASSERT_NO_FATAL_FAILURE(AssertPendingState(&tracker, client.GetId(), false));
}
