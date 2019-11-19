// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/navigation_tracker.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/browser_info.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/javascript_dialog_manager.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/net/timeout.h"

namespace {

const char kAutomationExtensionBackgroundPage[] =
    "chrome-extension://aapnijgdinlhnhlmodcfapnahmbfebeb/"
    "_generated_background_page.html";

// Match to content/browser/devtools/devTools_session const of same name
const char kTargetClosedMessage[] = "Inspected target navigated or closed";

Status MakeNavigationCheckFailedStatus(Status command_status) {
  if (command_status.code() == kUnexpectedAlertOpen)
    return Status(kUnexpectedAlertOpen);
  else if (command_status.code() == kTimeout)
    return Status(kTimeout);
  else
    return Status(kUnknownError, "cannot determine loading status",
                  command_status);
}

}  // namespace

NavigationTracker::NavigationTracker(
    DevToolsClient* client,
    const BrowserInfo* browser_info,
    const JavaScriptDialogManager* dialog_manager,
    const bool is_eager)
    : client_(client),
      loading_state_(kUnknown),
      top_frame_id_(client->GetId()),
      dialog_manager_(dialog_manager),
      is_eager_(is_eager),
      timed_out_(false) {
  client_->AddListener(this);
}

NavigationTracker::NavigationTracker(
    DevToolsClient* client,
    LoadingState known_state,
    const BrowserInfo* browser_info,
    const JavaScriptDialogManager* dialog_manager,
    const bool is_eager)
    : client_(client),
      loading_state_(known_state),
      top_frame_id_(client->GetId()),
      dialog_manager_(dialog_manager),
      is_eager_(is_eager),
      timed_out_(false) {
  client_->AddListener(this);
}

NavigationTracker::~NavigationTracker() {}

Status NavigationTracker::IsPendingNavigation(const std::string& frame_id,
                                              const Timeout* timeout,
                                              bool* is_pending) {
  if (dialog_manager_->IsDialogOpen()) {
    // The render process is paused while modal dialogs are open, so
    // Runtime.evaluate will block and time out if we attempt to call it. In
    // this case we can consider the page to have loaded, so that we return
    // control back to the test and let it dismiss the dialog.
    *is_pending = false;
    return Status(kOk);
  }

  // Some DevTools commands (e.g. Input.dispatchMouseEvent) are handled in the
  // browser process, and may cause the renderer process to start a new
  // navigation. We need to call Runtime.evaluate to force a roundtrip to the
  // renderer process, and make sure that we notice any pending navigations
  // (see crbug.com/524079).
  base::DictionaryValue params;
  params.SetString("expression", "1");
  std::unique_ptr<base::DictionaryValue> result;
  Status status = client_->SendCommandAndGetResultWithTimeout(
      "Runtime.evaluate", params, timeout, &result);
  int value = 0;
  if (status.code() == kDisconnected) {
    // If we receive a kDisconnected status code from Runtime.evaluate, don't
    // wait for pending navigations to complete, since we won't see any more
    // events from it until we reconnect.
    *is_pending = false;
    return Status(kOk);
  } else if (status.code() == kUnexpectedAlertOpen) {
    // The JS event loop is paused while modal dialogs are open, so return
    // control to the test so that it can dismiss the dialog.
    *is_pending = false;
    return Status(kOk);
  } else if (status.IsError() ||
             !result->GetInteger("result.value", &value) ||
             value != 1) {
    return MakeNavigationCheckFailedStatus(status);
  }

  if (loading_state_ == kUnknown) {
    // In the case that a http request is sent to server to fetch the page
    // content and the server hasn't responded at all, a dummy page is created
    // for the new window. In such case, the baseURL will be 'about:blank'.
    base::DictionaryValue empty_params;
    std::unique_ptr<base::DictionaryValue> result;
    Status status = client_->SendCommandAndGetResultWithTimeout(
        "DOM.getDocument", empty_params, timeout, &result);
    std::string base_url;
    std::string doc_url;
    if (status.IsError() || !result->GetString("root.baseURL", &base_url) ||
        !result->GetString("root.documentURL", &doc_url))
      return MakeNavigationCheckFailedStatus(status);
    if (doc_url != "about:blank" && base_url == "about:blank") {
      *is_pending = true;
      loading_state_ = kLoading;
      return Status(kOk);
    }

    // If we're loading the ChromeDriver automation extension background page,
    // look for a known function to determine the loading status.
    if (base_url == kAutomationExtensionBackgroundPage) {
      bool function_exists = false;
      status = CheckFunctionExists(timeout, &function_exists);
      if (status.IsError())
        return MakeNavigationCheckFailedStatus(status);
      loading_state_ = function_exists ? kNotLoading : kLoading;
    }
    status = DetermineUnknownLoadingState();
    if (status.IsError())
      return MakeNavigationCheckFailedStatus(status);
  }
  *is_pending = loading_state_ == kLoading;
  return Status(kOk);
}

Status NavigationTracker::CheckFunctionExists(const Timeout* timeout,
                                              bool* exists) {
  base::DictionaryValue params;
  params.SetString("expression", "typeof(getWindowInfo)");
  std::unique_ptr<base::DictionaryValue> result;
  Status status = client_->SendCommandAndGetResultWithTimeout(
      "Runtime.evaluate", params, timeout, &result);
  std::string type;
  if (status.IsError() || !result->GetString("result.value", &type))
    return MakeNavigationCheckFailedStatus(status);
  *exists = type == "function";
  return Status(kOk);
}

void NavigationTracker::set_timed_out(bool timed_out) {
  timed_out_ = timed_out;
}

bool NavigationTracker::IsNonBlocking() const {
  return false;
}

Status NavigationTracker::OnConnected(DevToolsClient* client) {
  loading_state_ = kUnknown;
  // Enable page domain notifications to allow tracking navigation state.
  base::DictionaryValue empty_params;
  return client_->SendCommand("Page.enable", empty_params);
}

Status NavigationTracker::OnEvent(DevToolsClient* client,
                                  const std::string& method,
                                  const base::DictionaryValue& params) {
  if (method == "Page.loadEventFired" ||
      (is_eager_ && method == "Page.domContentEventFired")) {
    loading_state_ = kNotLoading;
  } else if (method == "Page.frameStartedLoading") {
    // If frame that started loading is the top frame
    // set loading_state_ to loading. If it is a subframe
    // the loading state should not change
    std::string frame_id;
    if (!params.GetString("frameId", &frame_id))
      return Status(kUnknownError, "missing or invalid 'frameId'");
    if (frame_id == top_frame_id_) {
      loading_state_ = kLoading;
    }
  } else if (method == "Page.frameStoppedLoading") {
    // Sometimes Page.frameStoppedLoading fires without
    // an associated Page.loadEventFired. If this happens
    // for the top frame, assume loading has finished.
    std::string frame_id;
    if (!params.GetString("frameId", &frame_id))
      return Status(kUnknownError, "missing or invalid 'frameId'");
    if (frame_id == top_frame_id_) {
      loading_state_ = kNotLoading;
    }
  } else if (method == "Inspector.targetCrashed") {
    loading_state_ = kNotLoading;
  }
  if (timed_out_)
    loading_state_ = kNotLoading;
  return Status(kOk);
}

Status NavigationTracker::DetermineUnknownLoadingState() {
  base::DictionaryValue params;
  params.SetString("expression", "document.readyState");
  std::unique_ptr<base::DictionaryValue> result;
  Status status =
      client_->SendCommandAndGetResult("Runtime.evaluate", params, &result);
  std::string ready_state;
  if (status.IsError() || !result->GetString("result.value", &ready_state)) {
    return MakeNavigationCheckFailedStatus(status);
  }
  if (ready_state == "complete" ||
      (is_eager_ && ready_state == "interactive")) {
    loading_state_ = kNotLoading;
  } else {
    loading_state_ = kLoading;
  }
  return Status(kOk);
}

Status NavigationTracker::OnCommandSuccess(
    DevToolsClient* client,
    const std::string& method,
    const base::DictionaryValue& result,
    const Timeout& command_timeout) {
  // Check for start of navigation. In some case response to navigate is delayed
  // until after the command has already timed out, in which case it has already
  // been cancelled or will be cancelled soon, and should be ignored.
  if ((method == "Page.navigate" || method == "Page.navigateToHistoryEntry") &&
      loading_state_ != kLoading && !command_timeout.IsExpired()) {
    // At this point the browser has initiated the navigation, but besides that,
    // it is unknown what will happen.
    //
    // There are a few cases (perhaps more):
    // 1 The RenderFrameHost has already queued FrameMsg_Navigate and loading
    //   will start shortly.
    // 2 The RenderFrameHost has already queued FrameMsg_Navigate and loading
    //   will never start because it is just an in-page fragment navigation.
    // 3 The RenderFrameHost is suspended and hasn't queued FrameMsg_Navigate
    //   yet. This happens for cross-site navigations. The RenderFrameHost
    //   will not queue FrameMsg_Navigate until it is ready to unload the
    //   previous page (after running unload handlers and such).
    // TODO(nasko): Revisit case 3, since now unload handlers are run in the
    // background. http://crbug.com/323528.
    //
    // To determine whether a load is expected, do a round trip to the
    // renderer to ask what the URL is.
    // If case #1, by the time the command returns, the frame started to load
    // event will also have been received, since the DevTools command will
    // be queued behind FrameMsg_Navigate.
    // If case #2, by the time the command returns, the navigation will
    // have already happened, although no frame start/stop events will have
    // been received.
    // If case #3, the URL will be blank if the navigation hasn't been started
    // yet. In that case, expect a load to happen in the future.
    loading_state_ = kUnknown;
    base::DictionaryValue params;
    params.SetString("expression", "document.URL");
    std::unique_ptr<base::DictionaryValue> result;
    Status status(kOk);
    for (int attempt = 0; attempt < 3; attempt++) {
      status = client_->SendCommandAndGetResultWithTimeout(
          "Runtime.evaluate", params, &command_timeout, &result);
      if (status.code() == kUnknownError &&
          status.message().find(kTargetClosedMessage) != std::string::npos) {
        continue;
      } else {
        break;
      }
    }

    std::string url;
    if (status.IsError() || !result->GetString("result.value", &url))
      return MakeNavigationCheckFailedStatus(status);
    if (loading_state_ == kUnknown && url.empty())
      loading_state_ = kLoading;
  }
  return Status(kOk);
}
