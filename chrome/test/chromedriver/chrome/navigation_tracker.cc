// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/navigation_tracker.h"

#include <unordered_map>

#include "base/debug/stack_trace.h"
#include "base/logging.h"
#include "base/sequence_checker_impl.h"
#include "base/strings/string_util.h"
#include "base/uuid.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/net/timeout.h"

namespace {

// Match to content/browser/devtools/devTools_session const of same name
const char kTargetClosedMessage[] = "Inspected target navigated or closed";

Status MakeNavigationCheckFailedStatus(Status command_status) {
  // Report specific errors to callers for proper handling
  if (command_status.code() == kUnexpectedAlertOpen ||
      command_status.code() == kTimeout ||
      command_status.code() == kNavigationDetectedByRemoteEnd ||
      command_status.code() == kNoSuchExecutionContext) {
    return command_status;
  }

  return Status(kUnknownError, "cannot determine loading status",
                command_status);
}
std::unordered_map<std::string, int> error_codes({
#define NET_ERROR(label, value) {#label, value},
#include "net/base/net_error_list.h"
#undef NET_ERROR
});

const char kNetErrorStart[] = "net::ERR_";

bool IsNetworkError(const std::string& error_text) {
  if (!base::StartsWith(error_text, kNetErrorStart,
                        base::CompareCase::SENSITIVE)) {
    return false;
  }

  auto it = error_codes.find(error_text.substr(strlen(kNetErrorStart)));
  if (it == error_codes.end())
    return false;

  // According to comments in net/base/net_error_list.h
  // range 100-199: Connection related errors
  auto val = it->second;
  return val <= -100 && val >= -199;
}

class ObjectGroup {
 public:
  explicit ObjectGroup(DevToolsClient* client)
      : client_(client),
        object_group_name_(base::Uuid::GenerateRandomV4().AsLowercaseString()) {
  }

  ~ObjectGroup() {
    base::Value::Dict params;
    params.Set("objectGroup", object_group_name_);
    client_->SendCommandAndIgnoreResponse("Runtime.releaseObjectGroup", params);
  }

  const std::string& name() const { return object_group_name_; }

 private:
  raw_ptr<DevToolsClient> client_;
  std::string object_group_name_;
};

}  // namespace

NavigationTracker::NavigationTracker(
    DevToolsClient* client,
    WebView* web_view,
    const bool is_eager)
    : client_(client),
      web_view_(web_view),
      top_frame_id_(client->GetId()),
      is_eager_(is_eager),
      timed_out_(false),
      loading_state_(nullptr) {
  client_->AddListener(this);
  InitCurrentFrame(kUnknown);
}

NavigationTracker::NavigationTracker(
    DevToolsClient* client,
    LoadingState known_state,
    WebView* web_view,
    const bool is_eager)
    : client_(client),
      web_view_(web_view),
      top_frame_id_(client->GetId()),
      is_eager_(is_eager),
      timed_out_(false),
      loading_state_(nullptr) {
  client_->AddListener(this);
  InitCurrentFrame(known_state);
}

NavigationTracker::~NavigationTracker() = default;

void NavigationTracker::SetFrame(const std::string& new_frame_id) {
  if (new_frame_id.empty())
    current_frame_id_ = top_frame_id_;
  else
    current_frame_id_ = new_frame_id;
  auto it = frame_to_state_map_.find(current_frame_id_);
  if (it == frame_to_state_map_.end())
    SetCurrentFrameInvalid();
  else
    loading_state_ = &it->second;
}

Status NavigationTracker::IsPendingNavigation(const Timeout* timeout,
                                              bool* is_pending) {
  if (client_->IsDialogOpen()) {
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
  base::Value::Dict params;
  params.Set("expression", "1");
  base::Value::Dict result;
  Status status = client_->SendCommandAndGetResultWithTimeout(
      "Runtime.evaluate", params, timeout, &result);
  if (status.code() == kDisconnected) {
    // If we receive a kDisconnected status code from Runtime.evaluate, don't
    // wait for pending navigations to complete, since we won't see any more
    // events from it until we reconnect.
    *is_pending = false;
    return Status(kOk);
  }
  if (status.code() == kTargetDetached) {
    // If we receive a kTargetDetached status code from Runtime.evaluate, don't
    // wait for pending navigations to complete, since the page has been closed.
    *is_pending = false;
    return status;
  }
  if (status.code() == kUnexpectedAlertOpen) {
    // The JS event loop is paused while modal dialogs are open, so return
    // control to the test so that it can dismiss the dialog.
    *is_pending = false;
    return Status(kOk);
  }
  if (status.code() == kUnknownError &&
      status.message().find(kTargetClosedMessage) != std::string::npos) {
    *is_pending = true;
    return Status(kOk);
  }
  if (status.IsError() ||
      result.FindIntByDottedPath("result.value").value_or(0) != 1) {
    return MakeNavigationCheckFailedStatus(status);
  }

  if (!HasCurrentFrame()) {
    *is_pending = false;
    return Status(kOk);
  }
  if (GetLoadingState() == kUnknown) {
    // In the case that a http request is sent to server to fetch the page
    // content and the server hasn't responded at all, a dummy page is created
    // for the new window. In such case, the baseURL will be 'about:blank'.
    {
      // Scope for object_group
      ObjectGroup object_group(client_);

      base::Value::Dict eval_params;
      eval_params.Set("expression", "document");
      eval_params.Set("objectGroup", object_group.name());
      status = client_->SendCommandAndGetResultWithTimeout(
          "Runtime.evaluate", eval_params, timeout, &result);
      if (status.IsError()) {
        return MakeNavigationCheckFailedStatus(status);
      }
      std::string* object_id = result.FindStringByDottedPath("result.objectId");
      if (!object_id) {
        return MakeNavigationCheckFailedStatus(status);
      }

      base::Value::Dict describe_node_params;
      describe_node_params.Set("objectId", std::move(*object_id));
      status = client_->SendCommandAndGetResultWithTimeout(
          "DOM.describeNode", describe_node_params, timeout, &result);
      if (status.IsError()) {
        return MakeNavigationCheckFailedStatus(status);
      }
    }
    std::string* base_url = result.FindStringByDottedPath("node.baseURL");
    std::string* doc_url = result.FindStringByDottedPath("node.documentURL");
    if (!base_url || !doc_url)
      return MakeNavigationCheckFailedStatus(status);

    // Need to check current frame valid again to avoid accessing invalid
    // pointer loading_state_ because while getting result current frame
    // state may have changed.
    if (!HasCurrentFrame()) {
      *is_pending = false;
      return Status(kOk);
    }

    if (*doc_url != "about:blank" && *base_url == "about:blank") {
      *is_pending = true;
      *loading_state_ = kLoading;
      return Status(kOk);
    }

    status = UpdateCurrentLoadingState();
    if (status.code() == kNoSuchExecutionContext)
      *loading_state_ = kLoading;
    else if (status.IsError())
      return MakeNavigationCheckFailedStatus(status);
  }
  *is_pending = GetLoadingState() == kLoading;
  return Status(kOk);
}

Status NavigationTracker::CheckFunctionExists(const Timeout* timeout,
                                              bool* exists) {
  base::Value::Dict params;
  params.Set("expression", "typeof(getWindowInfo)");
  base::Value::Dict result;
  Status status = client_->SendCommandAndGetResultWithTimeout(
      "Runtime.evaluate", params, timeout, &result);
  if (status.IsError())
    return MakeNavigationCheckFailedStatus(status);
  std::string* type = result.FindStringByDottedPath("result.value");
  if (!type)
    return MakeNavigationCheckFailedStatus(status);
  *exists = *type == "function";
  return Status(kOk);
}

void NavigationTracker::set_timed_out(bool timed_out) {
  timed_out_ = timed_out;
}

bool NavigationTracker::IsNonBlocking() const {
  return false;
}

Status NavigationTracker::OnConnected(DevToolsClient* client) {
  ClearFrameStates();
  InitCurrentFrame(kUnknown);
  return Status{kOk};
}

Status NavigationTracker::OnEvent(DevToolsClient* client,
                                  const std::string& method,
                                  const base::Value::Dict& params) {
  if (client->IsMainPage() &&
      (method == "Page.loadEventFired" ||
       (is_eager_ && method == "Page.domContentEventFired"))) {
    frame_to_state_map_[top_frame_id_] = kNotLoading;
    return UpdateCurrentLoadingState();
  }
  if (method == "Page.frameAttached") {
    const std::string* frame_id = params.FindString("frameId");
    if (!frame_id)
      return Status(kUnknownError, "missing or invalid 'frameId'");
    frame_to_state_map_[*frame_id] = kUnknown;
  } else if (method == "Page.frameDetached") {
    const std::string* frame_id = params.FindString("frameId");
    if (!frame_id) {
      return Status(kUnknownError, "missing or invalid 'frameId'");
    }
    if (*frame_id == current_frame_id_) {
      SetCurrentFrameInvalid();
    }
    frame_to_state_map_.erase(*frame_id);
  } else if (method == "Page.frameStartedLoading") {
    // If frame that started loading is the current frame
    // set loading_state_ to loading. If it is another subframe
    // the loading state should not change
    const std::string* frame_id = params.FindString("frameId");
    if (!frame_id)
      return Status(kUnknownError, "missing or invalid 'frameId'");
    frame_to_state_map_[*frame_id] = kLoading;
  } else if (method == "Page.frameStoppedLoading") {
    // Sometimes Page.frameStoppedLoading fires without
    // an associated Page.loadEventFired. If this happens
    // for the current frame, assume loading has finished.
    const std::string* frame_id = params.FindString("frameId");
    if (!frame_id)
      return Status(kUnknownError, "missing or invalid 'frameId'");
    frame_to_state_map_[*frame_id] = kNotLoading;
  } else if (method == "Inspector.targetCrashed") {
    ClearFrameStates();
    InitCurrentFrame(kNotLoading);
  }
  return Status(kOk);
}

Status NavigationTracker::OnCommandSuccess(DevToolsClient* client,
                                           const std::string& method,
                                           const base::Value::Dict* result,
                                           const Timeout& command_timeout) {
  // Check if Page.navigate has any error from top frame
  if (method == "Page.navigate" && result) {
    const std::string* error_text = result->FindString("errorText");
    if (error_text && IsNetworkError(*error_text))
      return Status(kUnknownError, *error_text);
  }

  // Check for start of navigation. In some case response to navigate is delayed
  // until after the command has already timed out, in which case it has already
  // been cancelled or will be cancelled soon, and should be ignored.
  if (HasCurrentFrame() &&
      (method == "Page.navigate" || method == "Page.navigateToHistoryEntry") &&
      GetLoadingState() != kLoading && !command_timeout.IsExpired()) {
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
    *loading_state_ = kUnknown;
    base::Value::Dict params;
    params.Set("expression", "document.URL");
    base::Value::Dict result_dict;
    Status status(kOk);
    for (int attempt = 0; attempt < 3; attempt++) {
      status = client_->SendCommandAndGetResultWithTimeout(
          "Runtime.evaluate", params, &command_timeout, &result_dict);
      if (status.code() != kUnknownError ||
          status.message().find(kTargetClosedMessage) == std::string::npos) {
        break;
      }
    }

    if (status.IsError())
      return MakeNavigationCheckFailedStatus(status);
    std::string* url = result_dict.FindStringByDottedPath("result.value");
    if (!url)
      return MakeNavigationCheckFailedStatus(status);
    if (GetLoadingState() == kUnknown && url->empty())
      *loading_state_ = kLoading;
  }
  return Status(kOk);
}

Status NavigationTracker::UpdateCurrentLoadingState() {
  if (current_frame_id_.empty()) {
    // Under cases such as frame detached but current frame has not been
    // set yet, we don't know what is the current frame to check
    return Status(kOk);
  }

  std::unique_ptr<base::Value> result;
  Status status = web_view_->EvaluateScript(
      current_frame_id_, "document.readyState", false, &result);
  if (GetLoadingState() == kNotLoading) {
    // While calling EvaluateScript, some events may have arrived to indicate
    // that the page has finished loading. These events can be generated after
    // document.readyState is evaluated but processed by ChromeDriver before
    // EvaluateScript returns. In this case, it is important to keep the state
    // as not loading, to avoid deadlock.
    return Status(kOk);
  }
  if (status.code() == kNoSuchExecutionContext) {
    *loading_state_ = kLoading;
    // result is not set in this case, so return here
    return Status(kOk);
  }
  if (status.IsError()) {
    return MakeNavigationCheckFailedStatus(status);
  }
  std::string ready_state = result->GetString();
  if (ready_state == "complete" ||
      (is_eager_ && ready_state == "interactive")) {
    *loading_state_ = kNotLoading;
  } else {
    *loading_state_ = kLoading;
  }
  return Status(kOk);
}

NavigationTracker::LoadingState NavigationTracker::GetLoadingState() const {
  if (!HasCurrentFrame() || timed_out_)
    return kNotLoading;
  return *loading_state_;
}

bool NavigationTracker::HasCurrentFrame() const {
  return !current_frame_id_.empty();
}

void NavigationTracker::SetCurrentFrameInvalid() {
  current_frame_id_.clear();
  loading_state_ = &dummy_state_;
}

void NavigationTracker::InitCurrentFrame(LoadingState state) {
  current_frame_id_ = top_frame_id_;
  auto it = frame_to_state_map_.insert({current_frame_id_, state}).first;
  loading_state_ = &it->second;
}

void NavigationTracker::ClearFrameStates() {
  SetCurrentFrameInvalid();
  frame_to_state_map_.clear();
}
