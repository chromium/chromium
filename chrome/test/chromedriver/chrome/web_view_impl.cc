// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/web_view_impl.h"

#include <stddef.h>
#include <algorithm>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/test/chromedriver/chrome/browser_info.h"
#include "chrome/test/chromedriver/chrome/cast_tracker.h"
#include "chrome/test/chromedriver/chrome/devtools_client_impl.h"
#include "chrome/test/chromedriver/chrome/dom_tracker.h"
#include "chrome/test/chromedriver/chrome/download_directory_override_manager.h"
#include "chrome/test/chromedriver/chrome/frame_tracker.h"
#include "chrome/test/chromedriver/chrome/geolocation_override_manager.h"
#include "chrome/test/chromedriver/chrome/heap_snapshot_taker.h"
#include "chrome/test/chromedriver/chrome/javascript_dialog_manager.h"
#include "chrome/test/chromedriver/chrome/js.h"
#include "chrome/test/chromedriver/chrome/mobile_emulation_override_manager.h"
#include "chrome/test/chromedriver/chrome/navigation_tracker.h"
#include "chrome/test/chromedriver/chrome/network_conditions_override_manager.h"
#include "chrome/test/chromedriver/chrome/non_blocking_navigation_tracker.h"
#include "chrome/test/chromedriver/chrome/page_load_strategy.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/ui_events.h"
#include "chrome/test/chromedriver/net/timeout.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"

namespace {

const int kWaitForNavigationStopSeconds = 10;

Status GetContextIdForFrame(WebViewImpl* web_view,
                            const std::string& frame,
                            int* context_id) {
  if (frame.empty() || frame == web_view->GetId()) {
    *context_id = 0;
    return Status(kOk);
  }
  Status status =
      web_view->GetFrameTracker()->GetContextIdForFrame(frame, context_id);
  if (status.IsError())
    return status;
  return Status(kOk);
}

WebViewImpl* GetTargetForFrame(WebViewImpl* web_view,
                               const std::string& frame) {
  return frame.empty()
             ? web_view
             : static_cast<WebViewImpl*>(
                   web_view->GetFrameTracker()->GetTargetForFrame(frame));
}

const char* GetAsString(MouseEventType type) {
  switch (type) {
    case kPressedMouseEventType:
      return "mousePressed";
    case kReleasedMouseEventType:
      return "mouseReleased";
    case kMovedMouseEventType:
      return "mouseMoved";
    case kWheelMouseEventType:
      return "mouseWheel";
    default:
      return "";
  }
}

const char* GetAsString(TouchEventType type) {
  switch (type) {
    case kTouchStart:
      return "touchStart";
    case kTouchEnd:
      return "touchEnd";
    case kTouchMove:
      return "touchMove";
    case kTouchCancel:
      return "touchCancel";
    default:
      return "";
  }
}

const char* GetAsString(MouseButton button) {
  switch (button) {
    case kLeftMouseButton:
      return "left";
    case kMiddleMouseButton:
      return "middle";
    case kRightMouseButton:
      return "right";
    case kBackMouseButton:
      return "back";
    case kForwardMouseButton:
      return "forward";
    case kNoneMouseButton:
      return "none";
    default:
      return "";
  }
}

const char* GetAsString(KeyEventType type) {
  switch (type) {
    case kKeyDownEventType:
      return "keyDown";
    case kKeyUpEventType:
      return "keyUp";
    case kRawKeyDownEventType:
      return "rawKeyDown";
    case kCharEventType:
      return "char";
    default:
      return "";
  }
}

const char* GetAsString(PointerType type) {
  switch (type) {
    case kMouse:
      return "mouse";
    case kPen:
      return "pen";
    default:
      NOTREACHED();
      return "";
  }
}

std::unique_ptr<base::DictionaryValue> GenerateTouchPoint(
    const TouchEvent& event) {
  std::unique_ptr<base::DictionaryValue> point(new base::DictionaryValue());
  base::Value::Dict& dict = point->GetDict();
  dict.Set("x", event.x);
  dict.Set("y", event.y);
  dict.Set("radiusX", event.radiusX);
  dict.Set("radiusY", event.radiusY);
  dict.Set("rotationAngle", event.rotationAngle);
  dict.Set("force", event.force);
  dict.Set("tangentialPressure", event.tangentialPressure);
  dict.Set("tiltX", event.tiltX);
  dict.Set("tiltY", event.tiltY);
  dict.Set("twist", event.twist);
  dict.Set("id", event.id);
  return point;
}

}  // namespace

WebViewImpl::WebViewImpl(const std::string& id,
                         const bool w3c_compliant,
                         const WebViewImpl* parent,
                         const BrowserInfo* browser_info,
                         std::unique_ptr<DevToolsClient> client)
    : id_(id),
      w3c_compliant_(w3c_compliant),
      browser_info_(browser_info),
      is_locked_(false),
      is_detached_(false),
      parent_(parent),
      client_(std::move(client)),
      dom_tracker_(nullptr),
      frame_tracker_(nullptr),
      dialog_manager_(nullptr),
      mobile_emulation_override_manager_(nullptr),
      geolocation_override_manager_(nullptr),
      network_conditions_override_manager_(nullptr),
      heap_snapshot_taker_(nullptr),
      is_service_worker_(true) {
  client_->SetOwner(this);
}

WebViewImpl::WebViewImpl(const std::string& id,
                         const bool w3c_compliant,
                         const WebViewImpl* parent,
                         const BrowserInfo* browser_info,
                         std::unique_ptr<DevToolsClient> client,
                         const DeviceMetrics* device_metrics,
                         std::string page_load_strategy)
    : id_(id),
      w3c_compliant_(w3c_compliant),
      browser_info_(browser_info),
      is_locked_(false),
      is_detached_(false),
      parent_(parent),
      client_(std::move(client)),
      dom_tracker_(new DomTracker(client_.get())),
      frame_tracker_(new FrameTracker(client_.get(), this, browser_info)),
      dialog_manager_(new JavaScriptDialogManager(client_.get(), browser_info)),
      mobile_emulation_override_manager_(
          new MobileEmulationOverrideManager(client_.get(), device_metrics)),
      geolocation_override_manager_(
          new GeolocationOverrideManager(client_.get())),
      network_conditions_override_manager_(
          new NetworkConditionsOverrideManager(client_.get())),
      heap_snapshot_taker_(new HeapSnapshotTaker(client_.get())),
      is_service_worker_(false) {
  // Downloading in headless mode requires the setting of
  // Browser.setDownloadBehavior. This is handled by the
  // DownloadDirectoryOverrideManager, which is only instantiated
  // in headless chrome.
  if (browser_info->is_headless)
    download_directory_override_manager_ =
        std::make_unique<DownloadDirectoryOverrideManager>(client_.get());
  // Child WebViews should not have their own navigation_tracker, but defer
  // all related calls to their parent. All WebViews must have either parent_
  // or navigation_tracker_
  if (!parent_)
    navigation_tracker_ = std::unique_ptr<PageLoadStrategy>(
        PageLoadStrategy::Create(page_load_strategy, client_.get(), this,
                                 browser_info, dialog_manager_.get()));
  client_->SetOwner(this);
}

WebViewImpl::~WebViewImpl() {}

bool WebViewImpl::IsServiceWorker() const {
  return is_service_worker_;
}

WebViewImpl* WebViewImpl::CreateChild(const std::string& session_id,
                                      const std::string& target_id) const {
  // While there may be a deep hierarchy of WebViewImpl instances, the
  // hierarchy for DevToolsClientImpl is flat - there's a root which
  // sends/receives over the socket, and all child sessions are considered
  // its children (one level deep at most).
  DevToolsClientImpl* root_client =
      static_cast<DevToolsClientImpl*>(client_.get()->GetRootClient());
  std::unique_ptr<DevToolsClient> child_client(
      std::make_unique<DevToolsClientImpl>(root_client, session_id));
  WebViewImpl* child = new WebViewImpl(
      target_id, w3c_compliant_, this, browser_info_, std::move(child_client),
      nullptr,
      IsNonBlocking() ? PageLoadStrategy::kNone : PageLoadStrategy::kNormal);
  if (!IsNonBlocking()) {
    // Find Navigation Tracker for the top of the WebViewImpl hierarchy
    const WebViewImpl* currentView = this;
    while (currentView->parent_)
      currentView = currentView->parent_;
    PageLoadStrategy* pls = currentView->navigation_tracker_.get();
    NavigationTracker* nt = static_cast<NavigationTracker*>(pls);
    child->client_->AddListener(static_cast<DevToolsEventListener*>(nt));
  }
  return child;
}

std::string WebViewImpl::GetId() {
  return id_;
}

bool WebViewImpl::WasCrashed() {
  return client_->WasCrashed();
}

Status WebViewImpl::ConnectIfNecessary() {
  return client_->ConnectIfNecessary();
}

Status WebViewImpl::SetUpDevTools() {
  return client_->SetUpDevTools();
}

Status WebViewImpl::HandleReceivedEvents() {
  return client_->HandleReceivedEvents();
}

Status WebViewImpl::GetUrl(std::string* url) {
  base::DictionaryValue params;
  base::DictionaryValue result;
  Status status = client_->SendCommandAndGetResult("Page.getNavigationHistory",
                                                   params, &result);
  if (status.IsError())
    return status;
  absl::optional<int> current_index = result.GetDict().FindInt("currentIndex");
  if (!current_index)
    return Status(kUnknownError, "navigation history missing currentIndex");
  base::Value::List* entries = result.GetDict().FindList("entries");
  if (!entries)
    return Status(kUnknownError, "navigation history missing entries");
  if (static_cast<int>(entries->size()) <= *current_index ||
      !(*entries)[*current_index].is_dict()) {
    return Status(kUnknownError, "navigation history missing entry");
  }
  base::Value& entry = (*entries)[*current_index];
  if (!entry.GetDict().FindString("url"))
    return Status(kUnknownError, "navigation history entry is missing url");
  *url = *entry.GetDict().FindString("url");
  return Status(kOk);
}

Status WebViewImpl::Load(const std::string& url, const Timeout* timeout) {
  // Javascript URLs will cause a hang while waiting for the page to stop
  // loading, so just disallow.
  if (base::StartsWith(url, "javascript:",
                       base::CompareCase::INSENSITIVE_ASCII))
    return Status(kUnknownError, "unsupported protocol");
  base::DictionaryValue params;
  params.GetDict().Set("url", url);
  if (IsNonBlocking()) {
    // With non-bloakcing navigation tracker, the previous navigation might
    // still be in progress, and this can cause the new navigate command to be
    // ignored on Chrome v63 and above. Stop previous navigation first.
    client_->SendCommand("Page.stopLoading", base::DictionaryValue());
    // Use SendCommandAndIgnoreResponse to ensure no blocking occurs.
    return client_->SendCommandAndIgnoreResponse("Page.navigate", params);
  }
  return client_->SendCommandWithTimeout("Page.navigate", params, timeout);
}

Status WebViewImpl::Reload(const Timeout* timeout) {
  base::DictionaryValue params;
  params.GetDict().Set("ignoreCache", false);
  return client_->SendCommandWithTimeout("Page.reload", params, timeout);
}

Status WebViewImpl::Freeze(const Timeout* timeout) {
  base::DictionaryValue params;
  params.GetDict().Set("state", "frozen");
  return client_->SendCommandWithTimeout("Page.setWebLifecycleState", params,
                                         timeout);
}

Status WebViewImpl::Resume(const Timeout* timeout) {
  base::DictionaryValue params;
  params.GetDict().Set("state", "active");
  return client_->SendCommandWithTimeout("Page.setWebLifecycleState", params,
                                         timeout);
}

Status WebViewImpl::SendCommand(const std::string& cmd,
                                const base::DictionaryValue& params) {
  return client_->SendCommand(cmd, params);
}

Status WebViewImpl::SendCommandFromWebSocket(
    const std::string& cmd,
    const base::DictionaryValue& params,
    const int client_cmd_id) {
  return client_->SendCommandFromWebSocket(cmd, params, client_cmd_id);
}

Status WebViewImpl::SendCommandAndGetResult(
        const std::string& cmd,
        const base::DictionaryValue& params,
        std::unique_ptr<base::Value>* value) {
  std::unique_ptr<base::DictionaryValue> result =
      std::make_unique<base::DictionaryValue>();
  Status status = client_->SendCommandAndGetResult(cmd, params, result.get());
  if (status.IsError())
    return status;
  *value = std::move(result);
  return Status(kOk);
}

Status WebViewImpl::TraverseHistory(int delta, const Timeout* timeout) {
  base::DictionaryValue params;
  base::DictionaryValue result;
  Status status = client_->SendCommandAndGetResult("Page.getNavigationHistory",
                                                   params, &result);
  if (status.IsError()) {
    // TODO(samuong): remove this once we stop supporting WebView on KitKat.
    // Older versions of WebView on Android (on KitKat and earlier) do not have
    // the Page.getNavigationHistory DevTools command handler, so fall back to
    // using JavaScript to navigate back and forward. WebView reports its build
    // number as 0, so use the error status to detect if we can't use the
    // DevTools command.
    if (browser_info_->browser_name == "webview")
      return TraverseHistoryWithJavaScript(delta);
    else
      return status;
  }

  absl::optional<int> current_index = result.GetDict().FindInt("currentIndex");
  if (!current_index)
    return Status(kUnknownError, "DevTools didn't return currentIndex");

  base::Value::List* entries = result.GetDict().FindList("entries");
  if (!entries)
    return Status(kUnknownError, "DevTools didn't return entries");

  if ((*current_index + delta) < 0 ||
      (static_cast<int>(entries->size()) <= *current_index + delta) ||
      !(*entries)[*current_index + delta].is_dict()) {
    // The WebDriver spec says that if there are no pages left in the browser's
    // history (i.e. |current_index + delta| is out of range), then we must not
    // navigate anywhere.
    return Status(kOk);
  }

  base::Value& entry = (*entries)[*current_index + delta];
  absl::optional<int> entry_id = entry.FindIntKey("id");
  if (!entry_id)
    return Status(kUnknownError, "history entry does not have an id");
  params.GetDict().Set("entryId", *entry_id);

  return client_->SendCommandWithTimeout("Page.navigateToHistoryEntry", params,
                                         timeout);
}

Status WebViewImpl::TraverseHistoryWithJavaScript(int delta) {
  std::unique_ptr<base::Value> value;
  if (delta == -1)
    return EvaluateScript(std::string(), "window.history.back();", false,
                          &value);
  else if (delta == 1)
    return EvaluateScript(std::string(), "window.history.forward();", false,
                          &value);
  else
    return Status(kUnknownError, "expected delta to be 1 or -1");
}

Status WebViewImpl::EvaluateScriptWithTimeout(
    const std::string& frame,
    const std::string& expression,
    const base::TimeDelta& timeout,
    const bool awaitPromise,
    std::unique_ptr<base::Value>* result) {
  WebViewImpl* target = GetTargetForFrame(this, frame);
  if (target != nullptr && target != this) {
    if (target->IsDetached())
      return Status(kTargetDetached);
    WebViewImplHolder target_holder(target);
    return target->EvaluateScriptWithTimeout(frame, expression, timeout,
                                             awaitPromise, result);
  }

  int context_id;
  Status status = GetContextIdForFrame(this, frame, &context_id);
  if (status.IsError())
    return status;
  return internal::EvaluateScriptAndGetValue(
      client_.get(), context_id, expression, timeout, awaitPromise, result);
}

Status WebViewImpl::EvaluateScript(const std::string& frame,
                                   const std::string& expression,
                                   const bool awaitPromise,
                                   std::unique_ptr<base::Value>* result) {
  return EvaluateScriptWithTimeout(frame, expression, base::TimeDelta::Max(),
                                   awaitPromise, result);
}

Status WebViewImpl::CallFunctionWithTimeout(
    const std::string& frame,
    const std::string& function,
    const base::ListValue& args,
    const base::TimeDelta& timeout,
    std::unique_ptr<base::Value>* result) {
  std::string json;
  base::JSONWriter::Write(args, &json);
  std::string w3c = w3c_compliant_ ? "true" : "false";
  // TODO(zachconrad): Second null should be array of shadow host ids.
  std::string expression = base::StringPrintf(
      "(%s).apply(null, [%s, %s, %s])",
      kCallFunctionScript,
      function.c_str(),
      json.c_str(),
      w3c.c_str());
  std::unique_ptr<base::Value> temp_result;
  Status status =
      EvaluateScriptWithTimeout(frame, expression, timeout, true, &temp_result);
  if (status.IsError())
      return status;
  return internal::ParseCallFunctionResult(*temp_result, result);
}

Status WebViewImpl::CallFunction(const std::string& frame,
                                 const std::string& function,
                                 const base::ListValue& args,
                                 std::unique_ptr<base::Value>* result) {
  // Timeout set to Max is treated as no timeout.
  return CallFunctionWithTimeout(frame, function, args, base::TimeDelta::Max(),
                                 result);
}

Status WebViewImpl::CallAsyncFunction(const std::string& frame,
                                      const std::string& function,
                                      const base::ListValue& args,
                                      const base::TimeDelta& timeout,
                                      std::unique_ptr<base::Value>* result) {
  return CallAsyncFunctionInternal(
      frame, function, args, false, timeout, result);
}

Status WebViewImpl::CallUserSyncScript(const std::string& frame,
                                       const std::string& script,
                                       const base::ListValue& args,
                                       const base::TimeDelta& timeout,
                                       std::unique_ptr<base::Value>* result) {
  base::ListValue sync_args;
  sync_args.Append(script);
  // Clone needed since Append only accepts Value as an rvalue.
  sync_args.Append(args.Clone());
  return CallFunctionWithTimeout(frame, kExecuteScriptScript, sync_args,
                                 timeout, result);
}

Status WebViewImpl::CallUserAsyncFunction(
    const std::string& frame,
    const std::string& function,
    const base::ListValue& args,
    const base::TimeDelta& timeout,
    std::unique_ptr<base::Value>* result) {
  return CallAsyncFunctionInternal(
      frame, function, args, true, timeout, result);
}

Status WebViewImpl::GetFrameByFunction(const std::string& frame,
                                       const std::string& function,
                                       const base::ListValue& args,
                                       std::string* out_frame) {
  WebViewImpl* target = GetTargetForFrame(this, frame);
  if (target != nullptr && target != this) {
    if (target->IsDetached())
      return Status(kTargetDetached);
    WebViewImplHolder target_holder(target);
    return target->GetFrameByFunction(frame, function, args, out_frame);
  }

  int context_id;
  Status status = GetContextIdForFrame(this, frame, &context_id);
  if (status.IsError())
    return status;
  bool found_node;
  int node_id;
  status = internal::GetNodeIdFromFunction(
      client_.get(), context_id, function, args,
      &found_node, &node_id, w3c_compliant_);
  if (status.IsError())
    return status;
  if (!found_node)
    return Status(kNoSuchFrame);
  return dom_tracker_->GetFrameIdForNode(node_id, out_frame);
}

Status WebViewImpl::DispatchTouchEventsForMouseEvents(
    const std::vector<MouseEvent>& events,
    const std::string& frame) {
  // Touch events are filtered by the compositor if there are no touch listeners
  // on the page. Wait two frames for the compositor to sync with the main
  // thread to get consistent behavior.
  base::DictionaryValue promise_params;
  promise_params.GetDict().Set(
      "expression",
      "new Promise(x => setTimeout(() => setTimeout(x, 20), 20))");
  promise_params.GetDict().Set("awaitPromise", true);
  client_->SendCommand("Runtime.evaluate", promise_params);
  for (auto it = events.begin(); it != events.end(); ++it) {
    base::DictionaryValue params;

    switch (it->type) {
      case kPressedMouseEventType:
        params.GetDict().Set("type", "touchStart");
        break;
      case kReleasedMouseEventType:
        params.GetDict().Set("type", "touchEnd");
        break;
      case kMovedMouseEventType:
        if (it->button == kNoneMouseButton)
          continue;
        params.GetDict().Set("type", "touchMove");
        break;
      default:
        break;
    }

    std::unique_ptr<base::ListValue> touchPoints(new base::ListValue);
    if (it->type != kReleasedMouseEventType) {
      std::unique_ptr<base::DictionaryValue> touchPoint(
          new base::DictionaryValue);
      touchPoint->GetDict().Set("x", it->x);
      touchPoint->GetDict().Set("y", it->y);
      touchPoints->Append(
          base::Value::FromUniquePtrValue(std::move(touchPoint)));
    }
    params.SetList("touchPoints", std::move(touchPoints));
    params.GetDict().Set("modifiers", it->modifiers);
    Status status = client_->SendCommand("Input.dispatchTouchEvent", params);
    if (status.IsError())
      return status;
  }
  return Status(kOk);
}

Status WebViewImpl::DispatchMouseEvents(const std::vector<MouseEvent>& events,
                                        const std::string& frame,
                                        bool async_dispatch_events) {
  if (mobile_emulation_override_manager_->IsEmulatingTouch())
    return DispatchTouchEventsForMouseEvents(events, frame);

  Status status(kOk);
  for (auto it = events.begin(); it != events.end(); ++it) {
    base::DictionaryValue params;
    base::Value::Dict& dict = params.GetDict();
    std::string type = GetAsString(it->type);
    dict.Set("type", type);
    dict.Set("x", it->x);
    dict.Set("y", it->y);
    dict.Set("modifiers", it->modifiers);
    dict.Set("button", GetAsString(it->button));
    dict.Set("buttons", it->buttons);
    dict.Set("clickCount", it->click_count);
    dict.Set("force", it->force);
    dict.Set("tangentialPressure", it->tangentialPressure);
    dict.Set("tiltX", it->tiltX);
    dict.Set("tiltY", it->tiltY);
    dict.Set("twist", it->twist);
    dict.Set("pointerType", GetAsString(it->pointer_type));
    if (type == "mouseWheel") {
      dict.Set("deltaX", it->delta_x);
      dict.Set("deltaY", it->delta_y);
    }

    const bool last_event = (it == events.end() - 1);
    if (async_dispatch_events || !last_event) {
      status = client_->SendCommandAndIgnoreResponse("Input.dispatchMouseEvent",
                                                     params);
    } else {
      status = client_->SendCommand("Input.dispatchMouseEvent", params);
    }

    if (status.IsError())
      return status;
  }
  return status;
}

Status WebViewImpl::DispatchTouchEvent(const TouchEvent& event,
                                       bool async_dispatch_events) {
  base::DictionaryValue params;
  std::string type = GetAsString(event.type);
  params.GetDict().Set("type", type);
  base::ListValue point_list;
  Status status(kOk);
  if (type == "touchStart" || type == "touchMove") {
    std::unique_ptr<base::DictionaryValue> point = GenerateTouchPoint(event);
    point_list.Append(base::Value::FromUniquePtrValue(std::move(point)));
  }
  params.GetDict().Set("touchPoints", std::move(point_list));
  if (async_dispatch_events) {
    status = client_->SendCommandAndIgnoreResponse("Input.dispatchTouchEvent",
                                                   params);
  } else {
    status = client_->SendCommand("Input.dispatchTouchEvent", params);
  }
  return status;
}

Status WebViewImpl::DispatchTouchEvents(const std::vector<TouchEvent>& events,
                                        bool async_dispatch_events) {
  for (auto it = events.begin(); it != events.end(); ++it) {
    const bool last_event = (it == events.end() - 1);
    Status status =
        DispatchTouchEvent(*it, async_dispatch_events || !last_event);
    if (status.IsError())
      return status;
  }
  return Status(kOk);
}

Status WebViewImpl::DispatchTouchEventWithMultiPoints(
    const std::vector<TouchEvent>& events,
    bool async_dispatch_events) {
  if (events.size() == 0)
    return Status(kOk);

  base::DictionaryValue params;
  Status status(kOk);
  size_t touch_count = 1;
  for (const TouchEvent& event : events) {
    base::ListValue point_list;
    int32_t current_time =
        (base::Time::Now() - base::Time::UnixEpoch()).InMilliseconds();
    params.GetDict().Set("timestamp", current_time);
    std::string type = GetAsString(event.type);
    params.GetDict().Set("type", type);
    if (type == "touchCancel")
      continue;

    point_list.Append(
        base::Value::FromUniquePtrValue(GenerateTouchPoint(event)));
    params.GetDict().Set("touchPoints", std::move(point_list));

    if (async_dispatch_events || touch_count < events.size()) {
      status = client_->SendCommandAndIgnoreResponse("Input.dispatchTouchEvent",
                                                     params);
    } else {
      status = client_->SendCommand("Input.dispatchTouchEvent", params);
    }
    if (status.IsError())
      return status;

    touch_count++;
  }
  return Status(kOk);
}

Status WebViewImpl::DispatchKeyEvents(const std::vector<KeyEvent>& events,
                                      bool async_dispatch_events) {
  Status status(kOk);
  for (auto it = events.begin(); it != events.end(); ++it) {
    base::DictionaryValue params;
    base::Value::Dict& dict = params.GetDict();
    dict.Set("type", GetAsString(it->type));
    if (it->modifiers & kNumLockKeyModifierMask) {
      dict.Set("isKeypad", true);
      dict.Set("modifiers", it->modifiers & ~kNumLockKeyModifierMask);
    } else {
      dict.Set("modifiers", it->modifiers);
    }
    dict.Set("text", it->modified_text);
    dict.Set("unmodifiedText", it->unmodified_text);
    dict.Set("windowsVirtualKeyCode", it->key_code);
    std::string code;
    if (it->is_from_action) {
      code = it->code;
    } else {
      ui::DomCode dom_code = ui::UsLayoutKeyboardCodeToDomCode(it->key_code);
      code = ui::KeycodeConverter::DomCodeToCodeString(dom_code);
    }

    bool is_ctrl_cmd_key_down = false;
#if BUILDFLAG(IS_MAC)
    if (it->modifiers & kMetaKeyModifierMask)
      is_ctrl_cmd_key_down = true;
#else
    if (it->modifiers & kControlKeyModifierMask)
      is_ctrl_cmd_key_down = true;
#endif
    if (!code.empty())
      dict.Set("code", code);
    if (!it->key.empty())
      dict.Set("key", it->key);
    else if (it->is_from_action)
      dict.Set("key", it->modified_text);

    if (is_ctrl_cmd_key_down) {
      std::string command;
      if (code == "KeyA") {
        command = "SelectAll";
      } else if (code == "KeyC") {
        command = "Copy";
      } else if (code == "KeyX") {
        command = "Cut";
      } else if (code == "KeyY") {
        command = "Redo";
      } else if (code == "KeyV") {
        if (it->modifiers & kShiftKeyModifierMask)
          command = "PasteAndMatchStyle";
        else
          command = "Paste";
      } else if (code == "KeyZ") {
        if (it->modifiers & kShiftKeyModifierMask)
          command = "Redo";
        else
          command = "Undo";
      }

      std::unique_ptr<base::ListValue> command_list(new base::ListValue);
      command_list->Append(command);
      params.SetList("commands", std::move(command_list));
    }

    if (it->location != 0) {
      // The |location| parameter in DevTools protocol only accepts 1 (left
      // modifiers) and 2 (right modifiers). For location 3 (numeric keypad),
      // it is necessary to set the |isKeypad| parameter.
      if (it->location == 3)
        dict.Set("isKeypad", true);
      else
        dict.Set("location", it->location);
    }

    const bool last_event = (it == events.end() - 1);
    if (async_dispatch_events || !last_event) {
      status = client_->SendCommandAndIgnoreResponse("Input.dispatchKeyEvent",
                                                     params);
    } else {
      status = client_->SendCommand("Input.dispatchKeyEvent", params);
    }

    if (status.IsError())
      return status;
  }
  return status;
}

Status WebViewImpl::GetCookies(base::Value* cookies,
                               const std::string& current_page_url) {
  base::DictionaryValue params;
  base::DictionaryValue result;

  if (browser_info_->browser_name != "webview") {
    base::ListValue url_list;
    url_list.Append(current_page_url);
    params.GetDict().Set("urls", url_list.Clone());
    Status status =
        client_->SendCommandAndGetResult("Network.getCookies", params, &result);
    if (status.IsError())
      return status;
  } else {
    Status status =
        client_->SendCommandAndGetResult("Page.getCookies", params, &result);
    if (status.IsError())
      return status;
  }

  base::Value* const cookies_tmp = result.FindListKey("cookies");
  if (!cookies_tmp)
    return Status(kUnknownError, "DevTools didn't return cookies");
  *cookies = cookies_tmp->Clone();
  return Status(kOk);
}

Status WebViewImpl::DeleteCookie(const std::string& name,
                                 const std::string& url,
                                 const std::string& domain,
                                 const std::string& path) {
  base::DictionaryValue params;
  base::Value::Dict& dict = params.GetDict();
  dict.Set("url", url);
  std::string command;
  dict.Set("name", name);
  dict.Set("domain", domain);
  dict.Set("path", path);
  command = "Network.deleteCookies";
  return client_->SendCommand(command, params);
}

Status WebViewImpl::AddCookie(const std::string& name,
                              const std::string& url,
                              const std::string& value,
                              const std::string& domain,
                              const std::string& path,
                              const std::string& sameSite,
                              bool secure,
                              bool httpOnly,
                              double expiry) {
  base::DictionaryValue params;
  base::Value::Dict& dict = params.GetDict();
  dict.Set("name", name);
  dict.Set("url", url);
  dict.Set("value", value);
  dict.Set("domain", domain);
  dict.Set("path", path);
  dict.Set("secure", secure);
  dict.Set("httpOnly", httpOnly);
  if (!sameSite.empty())
    dict.Set("sameSite", sameSite);
  if (expiry >= 0)
    dict.Set("expires", expiry);

  base::DictionaryValue result;
  Status status =
      client_->SendCommandAndGetResult("Network.setCookie", params, &result);
  if (status.IsError())
    return Status(kUnableToSetCookie);
  if (!result.FindBoolKey("success").value_or(false))
    return Status(kUnableToSetCookie);
  return Status(kOk);
}

Status WebViewImpl::WaitForPendingNavigations(const std::string& frame_id,
                                              const Timeout& timeout,
                                              bool stop_load_on_timeout) {
  // This function should not be called for child WebViews
  if (parent_ != nullptr)
    return Status(kUnsupportedOperation,
                  "Call WaitForPendingNavigations only on the parent WebView");
  VLOG(0) << "Waiting for pending navigations...";
  const auto not_pending_navigation = base::BindRepeating(
      &WebViewImpl::IsNotPendingNavigation, base::Unretained(this), frame_id,
      base::Unretained(&timeout));
  Status status = client_->HandleEventsUntil(not_pending_navigation, timeout);
  if (status.code() == kTimeout && stop_load_on_timeout) {
    VLOG(0) << "Timed out. Stopping navigation...";
    navigation_tracker_->set_timed_out(true);
    client_->SendCommand("Page.stopLoading", base::DictionaryValue());
    // We don't consider |timeout| here to make sure the navigation actually
    // stops and we cleanup properly after a command that caused a navigation
    // that timed out.  Otherwise we might have to wait for that before
    // executing the next command, and it will be counted towards its timeout.
    Status new_status = client_->HandleEventsUntil(
        not_pending_navigation,
        Timeout(base::Seconds(kWaitForNavigationStopSeconds)));
    navigation_tracker_->set_timed_out(false);
    if (new_status.IsError())
      status = new_status;
  }
  VLOG(0) << "Done waiting for pending navigations. Status: "
          << status.message();
  return status;
}

Status WebViewImpl::IsPendingNavigation(const Timeout* timeout,
                                        bool* is_pending) const {
  if (navigation_tracker_)
    return navigation_tracker_->IsPendingNavigation(timeout, is_pending);
  else
    return parent_->IsPendingNavigation(timeout, is_pending);
}

JavaScriptDialogManager* WebViewImpl::GetJavaScriptDialogManager() {
  return dialog_manager_.get();
}

MobileEmulationOverrideManager* WebViewImpl::GetMobileEmulationOverrideManager()
    const {
  return mobile_emulation_override_manager_.get();
}

Status WebViewImpl::OverrideGeolocation(const Geoposition& geoposition) {
  return geolocation_override_manager_->OverrideGeolocation(geoposition);
}

Status WebViewImpl::OverrideNetworkConditions(
    const NetworkConditions& network_conditions) {
  return network_conditions_override_manager_->OverrideNetworkConditions(
      network_conditions);
}

Status WebViewImpl::OverrideDownloadDirectoryIfNeeded(
    const std::string& download_directory) {
  if (download_directory_override_manager_)
    return download_directory_override_manager_
        ->OverrideDownloadDirectoryWhenConnected(download_directory);
  return Status(kOk);
}

Status WebViewImpl::CaptureScreenshot(
    std::string* screenshot,
    const base::DictionaryValue& params) {
  base::DictionaryValue result;
  Timeout timeout(base::Seconds(10));
  Status status = client_->SendCommandAndGetResultWithTimeout(
      "Page.captureScreenshot", params, &timeout, &result);
  if (status.IsError())
    return status;
  if (!result.GetString("data", screenshot))
    return Status(kUnknownError, "expected string 'data' in response");
  return Status(kOk);
}

Status WebViewImpl::PrintToPDF(const base::DictionaryValue& params,
                               std::string* pdf) {
  // https://bugs.chromium.org/p/chromedriver/issues/detail?id=3517
  if (!browser_info_->is_headless) {
    return Status(kUnknownError,
                  "PrintToPDF is only supported in headless mode");
  }
  base::DictionaryValue result;
  Timeout timeout(base::Seconds(10));
  Status status = client_->SendCommandAndGetResultWithTimeout(
      "Page.printToPDF", params, &timeout, &result);
  if (status.IsError()) {
    if (status.code() == kUnknownError) {
      return Status(kInvalidArgument, status);
    }
    return status;
  }
  if (!result.GetString("data", pdf))
    return Status(kUnknownError, "expected string 'data' in response");
  return Status(kOk);
}

Status WebViewImpl::GetNodeIdByElement(const std::string& frame,
                                       const base::Value& element,
                                       int* node_id) {
  if (!element.is_dict())
    return Status(kUnknownError, "'element' is not a dictionary");
  int context_id;
  Status status = GetContextIdForFrame(this, frame, &context_id);
  if (status.IsError())
    return status;
  base::ListValue args;
  args.Append(element.Clone());
  bool found_node;
  status = internal::GetNodeIdFromFunction(
      client_.get(), context_id, "function(element) { return element; }", args,
      &found_node, node_id, w3c_compliant_);
  if (status.IsError())
    return status;
  if (!found_node)
    return Status(kNoSuchElement, "no node ID for given element");
  return Status(kOk);
}

Status WebViewImpl::SetFileInputFiles(const std::string& frame,
                                      const base::Value& element,
                                      const std::vector<base::FilePath>& files,
                                      const bool append) {
  if (!element.is_dict())
    return Status(kUnknownError, "'element' is not a dictionary");
  WebViewImpl* target = GetTargetForFrame(this, frame);
  if (target != nullptr && target != this) {
    if (target->IsDetached())
      return Status(kTargetDetached);
    WebViewImplHolder target_holder(target);
    return target->SetFileInputFiles(frame, element, files, append);
  }

  int node_id;
  Status status = GetNodeIdByElement(frame, element, &node_id);
  if (status.IsError())
    return status;

  base::ListValue file_list;
  // if the append flag is true, we need to retrieve the files that
  // already exist in the element and add them too.
  // Additionally, we need to add the old files first so that it looks
  // like we're appending files.
  if (append) {
    // Convert the node_id to a Runtime.RemoteObject
    std::string inputRemoteObjectId;
    {
      base::DictionaryValue cmd_result;
      base::DictionaryValue params;
      params.GetDict().Set("nodeId", node_id);
      status = client_->SendCommandAndGetResult("DOM.resolveNode", params,
                                                &cmd_result);
      if (status.IsError())
        return status;
      if (!cmd_result.GetString("object.objectId", &inputRemoteObjectId))
        return Status(kUnknownError, "DevTools didn't return objectId");
    }

    // figure out how many files there are
    absl::optional<int> numberOfFiles = 0;
    {
      base::DictionaryValue cmd_result;
      base::DictionaryValue params;
      params.GetDict().Set("functionDeclaration",
                           "function() { return this.files.length }");
      params.GetDict().Set("objectId", inputRemoteObjectId);
      status = client_->SendCommandAndGetResult("Runtime.callFunctionOn",
                                                params, &cmd_result);
      if (status.IsError())
        return status;
      numberOfFiles = cmd_result.FindIntPath("result.value");
      if (!numberOfFiles)
        return Status(kUnknownError, "DevTools didn't return value");
    }

    // Ask for each Runtime.RemoteObject and add them to the list
    for (int i = 0; i < *numberOfFiles; i++) {
      std::string fileObjectId;
      {
        base::DictionaryValue cmd_result;
        base::DictionaryValue params;
        params.GetDict().Set(
            "functionDeclaration",
            "function() { return this.files[" + std::to_string(i) + "] }");
        params.GetDict().Set("objectId", inputRemoteObjectId);

        status = client_->SendCommandAndGetResult("Runtime.callFunctionOn",
                                                  params, &cmd_result);
        if (status.IsError())
          return status;
        if (!cmd_result.GetString("result.objectId", &fileObjectId))
          return Status(kUnknownError, "DevTools didn't return objectId");
      }

      // Now convert each RemoteObject into the full path
      {
        base::DictionaryValue params;
        params.GetDict().Set("objectId", fileObjectId);
        base::DictionaryValue getFileInfoResult;
        status = client_->SendCommandAndGetResult("DOM.getFileInfo", params,
                                                  &getFileInfoResult);
        if (status.IsError())
          return status;
        // Add the full path to the file_list
        std::string fullPath;
        if (!getFileInfoResult.GetString("path", &fullPath))
          return Status(kUnknownError, "DevTools didn't return path");
        file_list.Append(fullPath);
      }
    }
  }

  // Now add the new files
  for (size_t i = 0; i < files.size(); ++i) {
    if (!files[i].IsAbsolute()) {
      return Status(kUnknownError,
                    "path is not absolute: " + files[i].AsUTF8Unsafe());
    }
    if (files[i].ReferencesParent()) {
      return Status(kUnknownError,
                    "path is not canonical: " + files[i].AsUTF8Unsafe());
    }
    file_list.Append(files[i].AsUTF8Unsafe());
  }

  base::DictionaryValue setFilesParams;
  setFilesParams.GetDict().Set("nodeId", node_id);
  setFilesParams.GetDict().Set("files", file_list.Clone());
  return client_->SendCommand("DOM.setFileInputFiles", setFilesParams);
}

Status WebViewImpl::TakeHeapSnapshot(std::unique_ptr<base::Value>* snapshot) {
  return heap_snapshot_taker_->TakeSnapshot(snapshot);
}

Status WebViewImpl::InitProfileInternal() {
  base::DictionaryValue params;

  return client_->SendCommand("Profiler.enable", params);
}

Status WebViewImpl::StopProfileInternal() {
  base::DictionaryValue params;
  Status status_debug = client_->SendCommand("Debugger.disable", params);
  Status status_profiler = client_->SendCommand("Profiler.disable", params);

  if (status_debug.IsError()) {
    return status_debug;
  } else if (status_profiler.IsError()) {
    return status_profiler;
  }

  return Status(kOk);
}

Status WebViewImpl::StartProfile() {
  Status status_init = InitProfileInternal();

  if (status_init.IsError())
    return status_init;

  base::DictionaryValue params;
  return client_->SendCommand("Profiler.start", params);
}

Status WebViewImpl::EndProfile(std::unique_ptr<base::Value>* profile_data) {
  base::DictionaryValue params;
  std::unique_ptr<base::DictionaryValue> profile_result =
      std::make_unique<base::DictionaryValue>();

  Status status = client_->SendCommandAndGetResult("Profiler.stop", params,
                                                   profile_result.get());

  if (status.IsError()) {
    Status disable_profile_status = StopProfileInternal();
    if (disable_profile_status.IsError()) {
      return disable_profile_status;
    } else {
      return status;
    }
  }

  *profile_data = std::move(profile_result);
  return status;
}

Status WebViewImpl::SynthesizeTapGesture(int x,
                                         int y,
                                         int tap_count,
                                         bool is_long_press) {
  base::DictionaryValue params;
  base::Value::Dict& dict = params.GetDict();
  dict.Set("x", x);
  dict.Set("y", y);
  dict.Set("tapCount", tap_count);
  if (is_long_press)
    dict.Set("duration", 1500);
  return client_->SendCommand("Input.synthesizeTapGesture", params);
}

Status WebViewImpl::SynthesizeScrollGesture(int x,
                                            int y,
                                            int xoffset,
                                            int yoffset) {
  base::DictionaryValue params;
  base::Value::Dict& dict = params.GetDict();
  dict.Set("x", x);
  dict.Set("y", y);
  // Chrome's synthetic scroll gesture is actually a "swipe" gesture, so the
  // direction of the swipe is opposite to the scroll (i.e. a swipe up scrolls
  // down, and a swipe left scrolls right).
  dict.Set("xDistance", -xoffset);
  dict.Set("yDistance", -yoffset);
  return client_->SendCommand("Input.synthesizeScrollGesture", params);
}

Status WebViewImpl::CallAsyncFunctionInternal(
    const std::string& frame,
    const std::string& function,
    const base::ListValue& args,
    bool is_user_supplied,
    const base::TimeDelta& timeout,
    std::unique_ptr<base::Value>* result) {
  base::ListValue async_args;
  async_args.Append("return (" + function + ").apply(null, arguments);");
  async_args.Append(args.Clone());
  async_args.Append(is_user_supplied);
  std::unique_ptr<base::Value> tmp;
  Timeout local_timeout(timeout);
  Status status = CallFunctionWithTimeout(frame, kExecuteAsyncScriptScript,
                                          async_args, timeout, &tmp);
  if (status.IsError())
    return status;

  const char kDocUnloadError[] = "document unloaded while waiting for result";
  std::string kQueryResult = base::StringPrintf(
      "function() {"
      "  var info = document.$chrome_asyncScriptInfo;"
      "  if (!info)"
      "    return {status: %d, value: '%s'};"
      "  var result = info.result;"
      "  if (!result)"
      "    return {status: 0};"
      "  delete info.result;"
      "  return result;"
      "}",
      kJavaScriptError,
      kDocUnloadError);
  const base::TimeDelta kOneHundredMs = base::Milliseconds(100);

  while (true) {
    base::ListValue no_args;
    std::unique_ptr<base::Value> query_value;
    status = CallFunction(frame, kQueryResult, no_args, &query_value);
    if (status.IsError()) {
      if (status.code() == kNoSuchFrame)
        return Status(kJavaScriptError, kDocUnloadError);
      return status;
    }

    base::DictionaryValue* result_info = NULL;
    if (!query_value->GetAsDictionary(&result_info))
      return Status(kUnknownError, "async result info is not a dictionary");
    absl::optional<int> status_code = result_info->FindIntKey("status");
    if (!status_code)
      return Status(kUnknownError, "async result info has no int 'status'");
    if (*status_code != kOk) {
      std::string message;
      result_info->GetString("value", &message);
      return Status(static_cast<StatusCode>(*status_code), message);
    }

    if (base::Value* value = result_info->FindKey("value")) {
      *result = base::Value::ToUniquePtrValue(value->Clone());
      return Status(kOk);
    }

    // Since async-scripts return immediately, need to time period here instead.
    if (local_timeout.IsExpired())
      return Status(kTimeout);

    base::PlatformThread::Sleep(
        std::min(kOneHundredMs, local_timeout.GetRemainingTime()));
  }
}

void WebViewImpl::SetFrame(const std::string& new_frame_id) {
  if (!is_service_worker_)
    navigation_tracker_->SetFrame(new_frame_id);
}

Status WebViewImpl::IsNotPendingNavigation(const std::string& frame_id,
                                           const Timeout* timeout,
                                           bool* is_not_pending) {
  if (!frame_id.empty() && !frame_tracker_->IsKnownFrame(frame_id)) {
    // Frame has already been destroyed.
    *is_not_pending = true;
    return Status(kOk);
  }
  bool is_pending;
  Status status =
      navigation_tracker_->IsPendingNavigation(timeout, &is_pending);
  if (status.IsError())
    return status;
  // An alert may block the pending navigation.
  if (dialog_manager_->IsDialogOpen()) {
    std::string alert_text;
    status = dialog_manager_->GetDialogMessage(&alert_text);
    if (status.IsError())
      return Status(kUnexpectedAlertOpen);
    return Status(kUnexpectedAlertOpen, "{Alert text : " + alert_text + "}");
  }

  *is_not_pending = !is_pending;
  return Status(kOk);
}

bool WebViewImpl::IsNonBlocking() const {
  if (navigation_tracker_)
    return navigation_tracker_->IsNonBlocking();
  else
    return parent_->IsNonBlocking();
}

bool WebViewImpl::IsOOPIF(const std::string& frame_id) {
  WebView* target = GetTargetForFrame(this, frame_id);
  return target != nullptr && frame_id == target->GetId();
}

FrameTracker* WebViewImpl::GetFrameTracker() const {
  return frame_tracker_.get();
}

const WebViewImpl* WebViewImpl::GetParent() const {
  return parent_;
}

bool WebViewImpl::Lock() {
  bool was_locked = is_locked_;
  is_locked_ = true;
  return was_locked;
}

void WebViewImpl::Unlock() {
  is_locked_ = false;
}

bool WebViewImpl::IsLocked() const {
  return is_locked_;
}

void WebViewImpl::SetDetached() {
  is_detached_ = true;
  client_->SetDetached();
}

bool WebViewImpl::IsDetached() const {
  return is_detached_;
}

std::unique_ptr<base::Value> WebViewImpl::GetCastSinks() {
  if (!cast_tracker_)
    cast_tracker_ = std::make_unique<CastTracker>(client_.get());
  HandleReceivedEvents();
  return base::Value::ToUniquePtrValue(cast_tracker_->sinks().Clone());
}

std::unique_ptr<base::Value> WebViewImpl::GetCastIssueMessage() {
  if (!cast_tracker_)
    cast_tracker_ = std::make_unique<CastTracker>(client_.get());
  HandleReceivedEvents();
  return base::Value::ToUniquePtrValue(cast_tracker_->issue().Clone());
}

WebViewImplHolder::WebViewImplHolder(WebViewImpl* web_view) {
  // Lock input web view and all its parents, to prevent them from being
  // deleted while still in use. Inside |items_|, each web view must appear
  // before its parent. This ensures the destructor unlocks the web views in
  // the right order.
  while (web_view != nullptr) {
    Item item;
    item.web_view = web_view;
    item.was_locked = web_view->Lock();
    items_.push_back(item);
    web_view = const_cast<WebViewImpl*>(web_view->GetParent());
  }
}

WebViewImplHolder::~WebViewImplHolder() {
  for (Item& item : items_) {
    // Once we find a web view that is still locked, then all its parents must
    // also be locked.
    if (item.was_locked)
      break;
    WebViewImpl* web_view = item.web_view;
    if (!web_view->IsDetached())
      web_view->Unlock();
    else if (web_view->GetParent() != nullptr)
      web_view->GetParent()->GetFrameTracker()->DeleteTargetForFrame(
          web_view->GetId());
  }
}

namespace internal {

Status EvaluateScript(DevToolsClient* client,
                      int context_id,
                      const std::string& expression,
                      EvaluateScriptReturnType return_type,
                      const base::TimeDelta& timeout,
                      const bool awaitPromise,
                      std::unique_ptr<base::DictionaryValue>* result) {
  base::DictionaryValue params;
  base::Value::Dict& dict = params.GetDict();
  dict.Set("expression", expression);
  if (context_id)
    dict.Set("contextId", context_id);
  dict.Set("returnByValue", return_type == ReturnByValue);
  dict.Set("awaitPromise", awaitPromise);
  base::Value cmd_result;

  Timeout local_timeout(timeout);
  Status status = client->SendCommandAndGetResultWithTimeout(
      "Runtime.evaluate", params, &local_timeout, &cmd_result);
  if (status.IsError())
    return status;

  if (cmd_result.is_dict() && cmd_result.FindKey("exceptionDetails")) {
    std::string description = "unknown";
    if (const std::string* maybe_description =
            cmd_result.FindStringPath("result.description")) {
      description = *maybe_description;
    }
    return Status(kUnknownError,
                  "Runtime.evaluate threw exception: " + description);
  }

  base::Value* unscoped_result = cmd_result.FindDictKey("result");
  if (!unscoped_result)
    return Status(kUnknownError, "evaluate missing dictionary 'result'");
  auto result_value = base::Value::ToUniquePtrValue(unscoped_result->Clone());
  *result = base::DictionaryValue::From(std::move(result_value));
  return Status(kOk);
}

Status EvaluateScriptAndGetObject(DevToolsClient* client,
                                  int context_id,
                                  const std::string& expression,
                                  const base::TimeDelta& timeout,
                                  const bool awaitPromise,
                                  bool* got_object,
                                  std::string* object_id) {
  std::unique_ptr<base::DictionaryValue> result;
  Status status = EvaluateScript(client, context_id, expression, ReturnByObject,
                                 timeout, awaitPromise, &result);
  if (status.IsError())
    return status;
  const base::Value* object_id_val = result->FindKey("objectId");
  if (!object_id_val) {
    *got_object = false;
    return Status(kOk);
  }
  if (!object_id_val->is_string())
    return Status(kUnknownError, "evaluate has invalid 'objectId'");
  *object_id = object_id_val->GetString();
  *got_object = true;
  return Status(kOk);
}

Status EvaluateScriptAndGetValue(DevToolsClient* client,
                                 int context_id,
                                 const std::string& expression,
                                 const base::TimeDelta& timeout,
                                 const bool awaitPromise,
                                 std::unique_ptr<base::Value>* result) {
  std::unique_ptr<base::DictionaryValue> temp_result;
  Status status = EvaluateScript(client, context_id, expression, ReturnByValue,
                                 timeout, awaitPromise, &temp_result);
  if (status.IsError())
    return status;

  std::string type;
  if (!temp_result->GetString("type", &type))
    return Status(kUnknownError, "Runtime.evaluate missing string 'type'");

  if (type == "undefined") {
    *result = std::make_unique<base::Value>();
  } else {
    base::Value* value = temp_result->FindKey("value");
    if (value == nullptr)
      return Status(kUnknownError, "Runtime.evaluate missing 'value'");
    *result = base::Value::ToUniquePtrValue(value->Clone());
  }
  return Status(kOk);
}

Status ParseCallFunctionResult(const base::Value& temp_result,
                               std::unique_ptr<base::Value>* result) {
  const base::DictionaryValue* dict;
  if (!temp_result.GetAsDictionary(&dict))
    return Status(kUnknownError, "call function result must be a dictionary");
  absl::optional<int> status_code = dict->FindIntKey("status");
  if (!status_code) {
    return Status(kUnknownError,
                  "call function result missing int 'status'");
  }
  if (*status_code != kOk) {
    std::string message;
    dict->GetString("value", &message);
    return Status(static_cast<StatusCode>(*status_code), message);
  }
  const base::Value* unscoped_value = dict->FindKey("value");
  if (unscoped_value == nullptr) {
    // Missing 'value' indicates the JavaScript code didn't return a value.
    return Status(kOk);
  }
  *result = base::Value::ToUniquePtrValue(unscoped_value->Clone());
  return Status(kOk);
}

Status GetNodeIdFromFunction(DevToolsClient* client,
                             int context_id,
                             const std::string& function,
                             const base::ListValue& args,
                             bool* found_node,
                             int* node_id,
                             bool w3c_compliant) {
  DCHECK(found_node);
  DCHECK(node_id);
  std::string json;
  base::JSONWriter::Write(args, &json);
  std::string w3c = w3c_compliant ? "true" : "false";
  // TODO(zachconrad): Second null should be array of shadow host ids.
  std::string expression = base::StringPrintf(
      "(%s).apply(null, [%s, %s, %s, true])",
      kCallFunctionScript,
      function.c_str(),
      json.c_str(),
      w3c.c_str());

  bool got_object;
  std::string element_id;
  Status status = internal::EvaluateScriptAndGetObject(
      client, context_id, expression, base::TimeDelta::Max(), true, &got_object,
      &element_id);
  if (status.IsError())
    return status;
  if (!got_object) {
    *found_node = false;
    return Status(kOk);
  }

  base::DictionaryValue cmd_result;
  {
    base::DictionaryValue params;
    params.GetDict().Set("objectId", element_id);
    status =
        client->SendCommandAndGetResult("DOM.requestNode", params, &cmd_result);
  }
  {
    // Release the remote object before doing anything else.
    base::DictionaryValue params;
    params.GetDict().Set("objectId", element_id);
    Status release_status =
        client->SendCommand("Runtime.releaseObject", params);
    if (release_status.IsError()) {
      LOG(ERROR) << "Failed to release remote object: "
                 << release_status.message();
    }
  }
  if (status.IsError())
    return status;

  absl::optional<int> maybe_node_id = cmd_result.FindIntKey("nodeId");
  if (!maybe_node_id)
    return Status(kUnknownError, "DOM.requestNode missing int 'nodeId'");

  // Note that this emulates the previous Deprecated GetInteger behavior, but
  // should likely be changed.
  *node_id = *maybe_node_id;
  *found_node = true;
  return Status(kOk);
}



}  // namespace internal
