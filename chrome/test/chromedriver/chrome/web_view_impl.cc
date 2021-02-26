// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/web_view_impl.h"

#include <stddef.h>
#include <utility>

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
  point->SetInteger("x", event.x);
  point->SetInteger("y", event.y);
  point->SetDouble("radiusX", event.radiusX);
  point->SetDouble("radiusY", event.radiusY);
  point->SetDouble("rotationAngle", event.rotationAngle);
  point->SetDouble("force", event.force);
  point->SetDouble("tangentialPressure", event.tangentialPressure);
  point->SetInteger("tiltX", event.tiltX);
  point->SetInteger("tiltY", event.tiltY);
  point->SetInteger("twist", event.twist);
  point->SetInteger("id", event.id);
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
  std::unique_ptr<base::DictionaryValue> result;
  Status status = client_->SendCommandAndGetResult(
      "Page.getNavigationHistory", params, &result);
  if (status.IsError())
    return status;
  int current_index = 0;
  if (!result->GetInteger("currentIndex", &current_index))
    return Status(kUnknownError, "navigation history missing currentIndex");
  base::ListValue* entries = nullptr;
  if (!result->GetList("entries", &entries))
    return Status(kUnknownError, "navigation history missing entries");
  base::DictionaryValue* entry = nullptr;
  if (!entries->GetDictionary(current_index, &entry))
    return Status(kUnknownError, "navigation history missing entry");
  if (!entry->GetString("url", url))
    return Status(kUnknownError, "navigation history entry is missing url");
  return Status(kOk);
}

Status WebViewImpl::Load(const std::string& url, const Timeout* timeout) {
  // Javascript URLs will cause a hang while waiting for the page to stop
  // loading, so just disallow.
  if (base::StartsWith(url, "javascript:",
                       base::CompareCase::INSENSITIVE_ASCII))
    return Status(kUnknownError, "unsupported protocol");
  base::DictionaryValue params;
  params.SetString("url", url);
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
  params.SetBoolean("ignoreCache", false);
  return client_->SendCommandWithTimeout("Page.reload", params, timeout);
}

Status WebViewImpl::Freeze(const Timeout* timeout) {
  base::DictionaryValue params;
  params.SetString("state", "frozen");
  return client_->SendCommandWithTimeout("Page.setWebLifecycleState", params,
                                         timeout);
}

Status WebViewImpl::Resume(const Timeout* timeout) {
  base::DictionaryValue params;
  params.SetString("state", "active");
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
  std::unique_ptr<base::DictionaryValue> result;
  Status status = client_->SendCommandAndGetResult(cmd, params, &result);
  if (status.IsError())
    return status;
  *value = std::move(result);
  return Status(kOk);
}

Status WebViewImpl::TraverseHistory(int delta, const Timeout* timeout) {
  base::DictionaryValue params;
  std::unique_ptr<base::DictionaryValue> result;
  Status status = client_->SendCommandAndGetResult(
      "Page.getNavigationHistory", params, &result);
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

  int current_index;
  if (!result->GetInteger("currentIndex", &current_index))
    return Status(kUnknownError, "DevTools didn't return currentIndex");

  base::ListValue* entries;
  if (!result->GetList("entries", &entries))
    return Status(kUnknownError, "DevTools didn't return entries");

  base::DictionaryValue* entry;
  if (!entries->GetDictionary(current_index + delta, &entry)) {
    // The WebDriver spec says that if there are no pages left in the browser's
    // history (i.e. |current_index + delta| is out of range), then we must not
    // navigate anywhere.
    return Status(kOk);
  }

  int entry_id;
  if (!entry->GetInteger("id", &entry_id))
    return Status(kUnknownError, "history entry does not have an id");
  params.SetInteger("entryId", entry_id);

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
  sync_args.AppendString(script);
  // Deep-copy needed since ListValue only accepts unique_ptrs of Values.
  sync_args.Append(args.CreateDeepCopy());
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
  base::DictionaryValue params;
  params.SetString("expression",
                   "new Promise(x => setTimeout(() => setTimeout(x, 20), 20))");
  params.SetBoolean("awaitPromise", true);
  client_->SendCommand("Runtime.evaluate", params);
  for (auto it = events.begin(); it != events.end(); ++it) {
    base::DictionaryValue params;

    switch (it->type) {
      case kPressedMouseEventType:
        params.SetString("type", "touchStart");
        break;
      case kReleasedMouseEventType:
        params.SetString("type", "touchEnd");
        break;
      case kMovedMouseEventType:
        if (it->button == kNoneMouseButton)
          continue;
        params.SetString("type", "touchMove");
        break;
      default:
        break;
    }

    std::unique_ptr<base::ListValue> touchPoints(new base::ListValue);
    if (it->type != kReleasedMouseEventType) {
      std::unique_ptr<base::DictionaryValue> touchPoint(
          new base::DictionaryValue);
      touchPoint->SetInteger("x", it->x);
      touchPoint->SetInteger("y", it->y);
      touchPoints->Append(std::move(touchPoint));
    }
    params.SetList("touchPoints", std::move(touchPoints));
    params.SetInteger("modifiers", it->modifiers);
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
    std::string type = GetAsString(it->type);
    params.SetString("type", type);
    params.SetInteger("x", it->x);
    params.SetInteger("y", it->y);
    params.SetInteger("modifiers", it->modifiers);
    params.SetString("button", GetAsString(it->button));
    params.SetInteger("buttons", it->buttons);
    params.SetInteger("clickCount", it->click_count);
    params.SetDouble("force", it->force);
    params.SetDouble("tangentialPressure", it->tangentialPressure);
    params.SetInteger("tiltX", it->tiltX);
    params.SetInteger("tiltY", it->tiltY);
    params.SetInteger("twist", it->twist);
    params.SetString("pointerType", GetAsString(it->pointer_type));
    if (type == "mouseWheel") {
      params.SetInteger("deltaX", it->delta_x);
      params.SetInteger("deltaY", it->delta_y);
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
  params.SetString("type", type);
  std::unique_ptr<base::ListValue> point_list(new base::ListValue);
  Status status(kOk);
  if (type == "touchStart" || type == "touchMove") {
    std::unique_ptr<base::DictionaryValue> point = GenerateTouchPoint(event);
    point_list->Append(std::move(point));
  }
  params.Set("touchPoints", std::move(point_list));
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
    std::unique_ptr<base::ListValue> point_list(new base::ListValue);
    int32_t current_time =
        (base::Time::Now() - base::Time::UnixEpoch()).InMilliseconds();
    params.SetInteger("timestamp", current_time);
    std::string type = GetAsString(event.type);
    params.SetString("type", type);
    if (type == "touchCancel")
      continue;

    point_list->Append(GenerateTouchPoint(event));
    params.Set("touchPoints", std::move(point_list));

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
    params.SetString("type", GetAsString(it->type));
    if (it->modifiers & kNumLockKeyModifierMask) {
      params.SetBoolean("isKeypad", true);
      params.SetInteger("modifiers",
                        it->modifiers & ~kNumLockKeyModifierMask);
    } else {
      params.SetInteger("modifiers", it->modifiers);
    }
    params.SetString("text", it->modified_text);
    params.SetString("unmodifiedText", it->unmodified_text);
    params.SetInteger("windowsVirtualKeyCode", it->key_code);
    std::string code;
    if (it->is_from_action) {
      code = it->code;
    } else {
      ui::DomCode dom_code = ui::UsLayoutKeyboardCodeToDomCode(it->key_code);
      code = ui::KeycodeConverter::DomCodeToCodeString(dom_code);
    }

    bool is_ctrl_cmd_key_down = false;
#if defined(OS_MAC)
    if (it->modifiers & kMetaKeyModifierMask)
      is_ctrl_cmd_key_down = true;
#else
    if (it->modifiers & kControlKeyModifierMask)
      is_ctrl_cmd_key_down = true;
#endif
    if (!code.empty())
      params.SetString("code", code);
    if (!it->key.empty())
      params.SetString("key", it->key);
    else if (it->is_from_action)
      params.SetString("key", it->modified_text);

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
      command_list->AppendString(command);
      params.SetList("commands", std::move(command_list));
    }

    if (it->location != 0) {
      // The |location| parameter in DevTools protocol only accepts 1 (left
      // modifiers) and 2 (right modifiers). For location 3 (numeric keypad),
      // it is necessary to set the |isKeypad| parameter.
      if (it->location == 3)
        params.SetBoolean("isKeypad", true);
      else
        params.SetInteger("location", it->location);
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

Status WebViewImpl::GetCookies(std::unique_ptr<base::ListValue>* cookies,
                               const std::string& current_page_url) {
  base::DictionaryValue params;
  std::unique_ptr<base::DictionaryValue> result;

  if (browser_info_->browser_name != "webview") {
    base::ListValue url_list;
    url_list.AppendString(current_page_url);
    params.SetKey("urls", url_list.Clone());
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

  base::ListValue* cookies_tmp;
  if (!result->GetList("cookies", &cookies_tmp))
    return Status(kUnknownError, "DevTools didn't return cookies");
  cookies->reset(cookies_tmp->DeepCopy());
  return Status(kOk);
}

Status WebViewImpl::DeleteCookie(const std::string& name,
                                 const std::string& url,
                                 const std::string& domain,
                                 const std::string& path) {
  base::DictionaryValue params;
  params.SetString("url", url);
  std::string command;
  params.SetString("name", name);
  params.SetString("domain", domain);
  params.SetString("path", path);
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
  params.SetString("name", name);
  params.SetString("url", url);
  params.SetString("value", value);
  params.SetString("domain", domain);
  params.SetString("path", path);
  params.SetBoolean("secure", secure);
  params.SetBoolean("httpOnly", httpOnly);
  if (!sameSite.empty())
    params.SetString("sameSite", sameSite);
  if (expiry >= 0)
    params.SetDouble("expires", expiry);

  std::unique_ptr<base::DictionaryValue> result;
  Status status =
      client_->SendCommandAndGetResult("Network.setCookie", params, &result);
  if (status.IsError())
    return Status(kUnableToSetCookie);
  bool success;
  if (!result->GetBoolean("success", &success) || !success)
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
        Timeout(base::TimeDelta::FromSeconds(kWaitForNavigationStopSeconds)));
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
  std::unique_ptr<base::DictionaryValue> result;
  Timeout timeout(base::TimeDelta::FromSeconds(10));
  Status status = client_->SendCommandAndGetResultWithTimeout(
      "Page.captureScreenshot", params, &timeout, &result);
  if (status.IsError())
    return status;
  if (!result->GetString("data", screenshot))
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
  std::unique_ptr<base::DictionaryValue> result;
  Timeout timeout(base::TimeDelta::FromSeconds(10));
  Status status = client_->SendCommandAndGetResultWithTimeout(
      "Page.printToPDF", params, &timeout, &result);
  if (status.IsError()) {
    if (status.code() == kUnknownError) {
      return Status(kInvalidArgument, status);
    }
    return status;
  }
  if (!result->GetString("data", pdf))
    return Status(kUnknownError, "expected string 'data' in response");
  return Status(kOk);
}

Status WebViewImpl::GetNodeIdByElement(const std::string& frame,
                                       const base::DictionaryValue& element,
                                       int* node_id) {
  int context_id;
  Status status = GetContextIdForFrame(this, frame, &context_id);
  if (status.IsError())
    return status;
  base::ListValue args;
  args.Append(element.CreateDeepCopy());
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
                                      const base::DictionaryValue& element,
                                      const std::vector<base::FilePath>& files,
                                      const bool append) {
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
      std::unique_ptr<base::DictionaryValue> cmd_result;
      base::DictionaryValue params;
      params.SetInteger("nodeId", node_id);
      status = client_->SendCommandAndGetResult("DOM.resolveNode", params,
                                                &cmd_result);
      if (status.IsError())
        return status;
      if (!cmd_result->GetString("object.objectId", &inputRemoteObjectId))
        return Status(kUnknownError, "DevTools didn't return objectId");
    }

    // figure out how many files there are
    int numberOfFiles = 0;
    {
      std::unique_ptr<base::DictionaryValue> cmd_result;
      base::DictionaryValue params;
      params.SetString("functionDeclaration",
                       "function() { return this.files.length }");
      params.SetString("objectId", inputRemoteObjectId);
      status = client_->SendCommandAndGetResult("Runtime.callFunctionOn",
                                                params, &cmd_result);
      if (status.IsError())
        return status;
      if (!cmd_result->GetInteger("result.value", &numberOfFiles))
        return Status(kUnknownError, "DevTools didn't return value");
    }

    // Ask for each Runtime.RemoteObject and add them to the list
    for (int i = 0; i < numberOfFiles; i++) {
      std::string fileObjectId;
      {
        std::unique_ptr<base::DictionaryValue> cmd_result;
        base::DictionaryValue params;
        params.SetString(
            "functionDeclaration",
            "function() { return this.files[" + std::to_string(i) + "] }");
        params.SetString("objectId", inputRemoteObjectId);

        status = client_->SendCommandAndGetResult("Runtime.callFunctionOn",
                                                  params, &cmd_result);
        if (status.IsError())
          return status;
        if (!cmd_result->GetString("result.objectId", &fileObjectId))
          return Status(kUnknownError, "DevTools didn't return objectId");
      }

      // Now convert each RemoteObject into the full path
      {
        base::DictionaryValue params;
        params.SetString("objectId", fileObjectId);
        std::unique_ptr<base::DictionaryValue> getFileInfoResult;
        status = client_->SendCommandAndGetResult("DOM.getFileInfo", params,
                                                  &getFileInfoResult);
        if (status.IsError())
          return status;
        // Add the full path to the file_list
        std::string fullPath;
        if (!getFileInfoResult->GetString("path", &fullPath))
          return Status(kUnknownError, "DevTools didn't return path");
        file_list.AppendString(fullPath);
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
    file_list.AppendString(files[i].AsUTF8Unsafe());
  }

  base::DictionaryValue setFilesParams;
  setFilesParams.SetInteger("nodeId", node_id);
  setFilesParams.SetKey("files", file_list.Clone());
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
  std::unique_ptr<base::DictionaryValue> profile_result;

  Status status = client_->SendCommandAndGetResult(
      "Profiler.stop", params, &profile_result);

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
  params.SetInteger("x", x);
  params.SetInteger("y", y);
  params.SetInteger("tapCount", tap_count);
  if (is_long_press)
    params.SetInteger("duration", 1500);
  return client_->SendCommand("Input.synthesizeTapGesture", params);
}

Status WebViewImpl::SynthesizeScrollGesture(int x,
                                            int y,
                                            int xoffset,
                                            int yoffset) {
  base::DictionaryValue params;
  params.SetInteger("x", x);
  params.SetInteger("y", y);
  // Chrome's synthetic scroll gesture is actually a "swipe" gesture, so the
  // direction of the swipe is opposite to the scroll (i.e. a swipe up scrolls
  // down, and a swipe left scrolls right).
  params.SetInteger("xDistance", -xoffset);
  params.SetInteger("yDistance", -yoffset);
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
  async_args.AppendString("return (" + function + ").apply(null, arguments);");
  async_args.Append(args.CreateDeepCopy());
  async_args.AppendBoolean(is_user_supplied);
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
  const base::TimeDelta kOneHundredMs = base::TimeDelta::FromMilliseconds(100);

  while (true) {
    base::ListValue no_args;
    std::unique_ptr<base::Value> query_value;
    Status status = CallFunction(frame, kQueryResult, no_args, &query_value);
    if (status.IsError()) {
      if (status.code() == kNoSuchFrame)
        return Status(kJavaScriptError, kDocUnloadError);
      return status;
    }

    base::DictionaryValue* result_info = NULL;
    if (!query_value->GetAsDictionary(&result_info))
      return Status(kUnknownError, "async result info is not a dictionary");
    int status_code;
    if (!result_info->GetInteger("status", &status_code))
      return Status(kUnknownError, "async result info has no int 'status'");
    if (status_code != kOk) {
      std::string message;
      result_info->GetString("value", &message);
      return Status(static_cast<StatusCode>(status_code), message);
    }

    base::Value* value = NULL;
    if (result_info->Get("value", &value)) {
      result->reset(value->DeepCopy());
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
  return std::unique_ptr<base::Value>(cast_tracker_->sinks().DeepCopy());
}

std::unique_ptr<base::Value> WebViewImpl::GetCastIssueMessage() {
  if (!cast_tracker_)
    cast_tracker_ = std::make_unique<CastTracker>(client_.get());
  HandleReceivedEvents();
  return std::unique_ptr<base::Value>(cast_tracker_->issue().DeepCopy());
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
  params.SetString("expression", expression);
  if (context_id)
    params.SetInteger("contextId", context_id);
  params.SetBoolean("returnByValue", return_type == ReturnByValue);
  params.SetBoolean("awaitPromise", awaitPromise);
  std::unique_ptr<base::DictionaryValue> cmd_result;

  Timeout local_timeout(timeout);
  Status status = client->SendCommandAndGetResultWithTimeout(
      "Runtime.evaluate", params, &local_timeout, &cmd_result);
  if (status.IsError())
    return status;

  if (cmd_result->HasKey("exceptionDetails")) {
    std::string description = "unknown";
    cmd_result->GetString("result.description", &description);
    return Status(kUnknownError,
                  "Runtime.evaluate threw exception: " + description);
  }

  base::DictionaryValue* unscoped_result;
  if (!cmd_result->GetDictionary("result", &unscoped_result))
    return Status(kUnknownError, "evaluate missing dictionary 'result'");
  result->reset(unscoped_result->DeepCopy());
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
  if (!result->HasKey("objectId")) {
    *got_object = false;
    return Status(kOk);
  }
  if (!result->GetString("objectId", object_id))
    return Status(kUnknownError, "evaluate has invalid 'objectId'");
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
    base::Value* value;
    if (!temp_result->Get("value", &value))
      return Status(kUnknownError, "Runtime.evaluate missing 'value'");
    result->reset(value->DeepCopy());
  }
  return Status(kOk);
}

Status ParseCallFunctionResult(const base::Value& temp_result,
                               std::unique_ptr<base::Value>* result) {
  const base::DictionaryValue* dict;
  if (!temp_result.GetAsDictionary(&dict))
    return Status(kUnknownError, "call function result must be a dictionary");
  int status_code;
  if (!dict->GetInteger("status", &status_code)) {
    return Status(kUnknownError,
                  "call function result missing int 'status'");
  }
  if (status_code != kOk) {
    std::string message;
    dict->GetString("value", &message);
    return Status(static_cast<StatusCode>(status_code), message);
  }
  const base::Value* unscoped_value;
  if (!dict->Get("value", &unscoped_value)) {
    // Missing 'value' indicates the JavaScript code didn't return a value.
    return Status(kOk);
  }
  result->reset(unscoped_value->DeepCopy());
  return Status(kOk);
}

Status GetNodeIdFromFunction(DevToolsClient* client,
                             int context_id,
                             const std::string& function,
                             const base::ListValue& args,
                             bool* found_node,
                             int* node_id,
                             bool w3c_compliant) {
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

  std::unique_ptr<base::DictionaryValue> cmd_result;
  {
    base::DictionaryValue params;
    params.SetString("objectId", element_id);
    status = client->SendCommandAndGetResult(
        "DOM.requestNode", params, &cmd_result);
  }
  {
    // Release the remote object before doing anything else.
    base::DictionaryValue params;
    params.SetString("objectId", element_id);
    Status release_status =
        client->SendCommand("Runtime.releaseObject", params);
    if (release_status.IsError()) {
      LOG(ERROR) << "Failed to release remote object: "
                 << release_status.message();
    }
  }
  if (status.IsError())
    return status;

  if (!cmd_result->GetInteger("nodeId", node_id))
    return Status(kUnknownError, "DOM.requestNode missing int 'nodeId'");
  *found_node = true;
  return Status(kOk);
}



}  // namespace internal
