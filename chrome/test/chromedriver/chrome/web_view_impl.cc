// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/web_view_impl.h"

#include <stddef.h>

#include <algorithm>
#include <cstring>
#include <map>
#include <memory>
#include <queue>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/test/chromedriver/chrome/bidi_tracker.h"
#include "chrome/test/chromedriver/chrome/browser_info.h"
#include "chrome/test/chromedriver/chrome/cast_tracker.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/devtools_client_impl.h"
#include "chrome/test/chromedriver/chrome/download_directory_override_manager.h"
#include "chrome/test/chromedriver/chrome/fedcm_tracker.h"
#include "chrome/test/chromedriver/chrome/frame_tracker.h"
#include "chrome/test/chromedriver/chrome/geolocation_override_manager.h"
#include "chrome/test/chromedriver/chrome/heap_snapshot_taker.h"
#include "chrome/test/chromedriver/chrome/js.h"
#include "chrome/test/chromedriver/chrome/mobile_emulation_override_manager.h"
#include "chrome/test/chromedriver/chrome/navigation_tracker.h"
#include "chrome/test/chromedriver/chrome/network_conditions_override_manager.h"
#include "chrome/test/chromedriver/chrome/non_blocking_navigation_tracker.h"
#include "chrome/test/chromedriver/chrome/page_load_strategy.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/ui_events.h"
#include "chrome/test/chromedriver/chrome/web_view.h"
#include "chrome/test/chromedriver/net/timeout.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"

namespace {

const int kWaitForNavigationStopSeconds = 10;
const char kElementKey[] = "ELEMENT";
const char kElementKeyW3C[] = "element-6066-11e4-a52e-4f735466cecf";
const char kShadowRootKey[] = "shadow-6066-11e4-a52e-4f735466cecf";
const char kFrameKey[] = "frame-075b-4da1-b6ba-e579c2d3230a";
const char kWindowKey[] = "window-fcc6-11e5-b4f8-330a88ab9d7f";

const char kElementNotFoundMessage[] = "element not found";
const char kShadowRootNotFoundMessage[] = "shadow root not found";
const char kStaleElementMessage[] = "stale element not found";
const char kDetachedShadowRootMessage[] = "detached shadow root not found";

const char* GetDefaultMessage(StatusCode code) {
  static const char kUnknownCodeMessage[] = "";
  switch (code) {
    case kNoSuchElement:
      return kElementNotFoundMessage;
    case kNoSuchShadowRoot:
      return kShadowRootNotFoundMessage;
    case kStaleElementReference:
      return kStaleElementMessage;
    case kDetachedShadowRoot:
      return kDetachedShadowRootMessage;
    default:
      return kUnknownCodeMessage;
  }
}

std::optional<std::string> GetElementIdKey(const base::Value::Dict& element,
                                           bool w3c_compliant) {
  std::string_view element_key = w3c_compliant ? kElementKeyW3C : kElementKey;
  std::vector<std::string_view> keys = {
      element_key,
      kShadowRootKey,
      kWindowKey,
      kFrameKey,
  };
  for (const std::string_view& key : keys) {
    if (element.contains(key)) {
      return std::string(key);
    }
  }
  return std::nullopt;
}

Status SplitElementId(const std::string& element_id,
                      StatusCode no_such_element,
                      std::string& frame_id,
                      std::string& loader_id,
                      int& backend_node_id) {
  if (!base::MatchPattern(element_id, "f.*.d.*.e.*")) {
    return Status{no_such_element, "the element id string is malformed"};
  }

  std::vector<std::string> components = base::SplitString(
      element_id, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (components.size() != 6) {
    return Status{no_such_element, "too many components in element id"};
  }

  std::string backend_node_id_str = components[5];
  if (!base::StringToInt(backend_node_id_str, &backend_node_id)) {
    return Status{no_such_element, "backendNodeId is not integer"};
  }

  frame_id = components[1];
  loader_id = components[3];
  return Status{kOk};
}

Status GetContextIdForFrame(WebViewImpl* web_view,
                            const std::string& frame,
                            std::string* context_id) {
  DCHECK(context_id);

  if (frame.empty()) {
    context_id->clear();
    return Status(kOk);
  }
  Status status =
      web_view->GetFrameTracker()->GetContextIdForFrame(frame, context_id);
  if (status.IsError())
    return status;
  return Status(kOk);
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
  }
}

base::Value::Dict GenerateTouchPoint(const TouchEvent& event) {
  base::Value::Dict point;
  point.Set("x", event.x);
  point.Set("y", event.y);
  point.Set("radiusX", event.radiusX);
  point.Set("radiusY", event.radiusY);
  point.Set("rotationAngle", event.rotationAngle);
  point.Set("force", event.force);
  point.Set("tangentialPressure", event.tangentialPressure);
  point.Set("tiltX", event.tiltX);
  point.Set("tiltY", event.tiltY);
  point.Set("twist", event.twist);
  point.Set("id", event.id);
  return point;
}

class ObjectGroup {
 public:
  explicit ObjectGroup(DevToolsClient* client)
      : client_(client),
        object_group_name_(base::Uuid::GenerateRandomV4().AsLowercaseString()) {
  }

  ~ObjectGroup() {
    if (is_empty_) {
      return;
    }
    base::Value::Dict params;
    params.Set("objectGroup", object_group_name_);
    client_->SendCommandAndIgnoreResponse("Runtime.releaseObjectGroup", params);
  }

  bool IsEmpty() const { return is_empty_; }

  void SetEmpty(bool value) { is_empty_ = value; }

  const std::string& name() const { return object_group_name_; }

 private:
  raw_ptr<DevToolsClient> client_;
  std::string object_group_name_;
  bool is_empty_ = false;
};

Status DescribeNode(DevToolsClient* client,
                    int backend_node_id,
                    int depth,
                    bool pierce,
                    base::Value* result_node) {
  DCHECK(result_node);
  base::Value::Dict params;
  base::Value::Dict cmd_result;
  params.Set("backendNodeId", backend_node_id);
  params.Set("depth", depth);
  params.Set("pierce", pierce);
  Status status =
      client->SendCommandAndGetResult("DOM.describeNode", params, &cmd_result);
  if (status.IsError()) {
    return status;
  }

  base::Value* node = cmd_result.Find("node");
  if (!node || !node->is_dict()) {
    return Status(kUnknownError, "DOM.describeNode missing dictionary 'node'");
  }

  *result_node = std::move(*node);

  return status;
}

Status GetFrameIdForBackendNodeId(DevToolsClient* client,
                                  int backend_node_id,
                                  bool* found_node,
                                  std::string* frame_id) {
  DCHECK(frame_id);
  DCHECK(found_node);

  Status status{kOk};

  base::Value node;
  status = DescribeNode(client, backend_node_id, 0, false, &node);
  if (status.IsError()) {
    return status;
  }

  std::string* maybe_frame_id = node.GetIfDict()->FindString("frameId");
  if (maybe_frame_id) {
    *frame_id = *maybe_frame_id;
    *found_node = true;
    return Status(kOk);
  }

  return Status(kOk);
}

Status ResolveWeakReferences(base::Value::List& nodes) {
  Status status{kOk};
  std::map<int, int> ref_to_idx;
  // Mapping
  for (int k = 0; static_cast<size_t>(k) < nodes.size(); ++k) {
    if (!nodes[k].is_dict()) {
      continue;
    }
    const base::Value::Dict& node = nodes[k].GetDict();
    std::optional<int> weak_node_ref =
        node.FindIntByDottedPath("weakLocalObjectReference");
    if (!weak_node_ref) {
      continue;
    }
    std::optional<int> maybe_backend_node_id =
        node.FindIntByDottedPath("value.backendNodeId");
    if (!maybe_backend_node_id) {
      continue;
    }
    ref_to_idx[weak_node_ref.value()] = k;
  }
  // Resolving
  for (int k = 0; static_cast<size_t>(k) < nodes.size(); ++k) {
    if (!nodes[k].is_dict()) {
      continue;
    }
    const base::Value::Dict& node = nodes[k].GetDict();
    std::optional<int> weak_node_ref =
        node.FindIntByDottedPath("weakLocalObjectReference");
    if (!weak_node_ref) {
      continue;
    }
    std::optional<int> maybe_backend_node_id =
        node.FindIntByDottedPath("value.backendNodeId");
    if (maybe_backend_node_id) {
      continue;
    }
    auto it = ref_to_idx.find(*weak_node_ref);
    if (it == ref_to_idx.end()) {
      return Status{
          kUnknownError,
          base::StringPrintf("Unable to resolve weakLocalObjectReference=%d",
                             *weak_node_ref)};
    }
    nodes[k] = nodes[it->second].Clone();
  }
  return status;
}

class BidiTrackerGuard {
 public:
  explicit BidiTrackerGuard(DevToolsClient& client) : client_(client) {
    client_->AddListener(&bidi_tracker_);
  }

  BidiTracker& Tracker() { return bidi_tracker_; }

  const BidiTracker& Tracker() const { return bidi_tracker_; }

  ~BidiTrackerGuard() { client_->RemoveListener(&bidi_tracker_); }

 private:
  base::raw_ref<DevToolsClient> client_;
  BidiTracker bidi_tracker_;
};

Status ResolveNode(DevToolsClient& client,
                   int backend_node_id,
                   const std::string& object_group_name,
                   const Timeout* timeout,
                   std::string& object_id) {
  base::Value::Dict params;
  base::Value::Dict resolve_result;
  params.Set("backendNodeId", backend_node_id);
  // TODO(crbug.com/42323411): add support of uniqueContextId to
  // DOM.resolveNode params.Set("uniqueContextId", context_id);
  if (!object_group_name.empty()) {
    params.Set("objectGroup", object_group_name);
  }
  Status status = client.SendCommandAndGetResultWithTimeout(
      "DOM.resolveNode", params, timeout, &resolve_result);
  if (status.IsError()) {
    return status;
  }
  std::string* maybe_object_id =
      resolve_result.FindStringByDottedPath("object.objectId");
  if (!maybe_object_id) {
    return Status{
        kUnknownError,
        "object.objectId is missing in the response to DOM.resolveNode"};
  }
  object_id = *maybe_object_id;
  return status;
}

Status WrapIfTargetDetached(Status status, StatusCode new_code) {
  if (status.code() == kTargetDetached) {
    return Status{new_code, status};
  }
  return status;
}

}  // namespace

std::unique_ptr<WebViewImpl> WebViewImpl::CreateServiceWorkerWebView(
    const std::string& id,
    const bool w3c_compliant,
    const BrowserInfo* browser_info,
    std::unique_ptr<DevToolsClient> client) {
  return std::unique_ptr<WebViewImpl>(new WebViewImpl(
      id, w3c_compliant, nullptr, browser_info, std::move(client)));
}

std::unique_ptr<WebViewImpl> WebViewImpl::CreateTopLevelWebView(
    const std::string& id,
    const bool w3c_compliant,
    const BrowserInfo* browser_info,
    std::unique_ptr<DevToolsClient> client,
    std::optional<MobileDevice> mobile_device,
    std::string page_load_strategy,
    bool autoaccept_beforeunload) {
  return std::make_unique<WebViewImpl>(
      id, w3c_compliant, nullptr, browser_info, std::move(client),
      std::move(mobile_device), page_load_strategy, autoaccept_beforeunload);
}

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
      frame_tracker_(nullptr),
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
                         std::optional<MobileDevice> mobile_device,
                         std::string page_load_strategy,
                         bool autoaccept_beforeunload)
    : id_(id),
      w3c_compliant_(w3c_compliant),
      browser_info_(browser_info),
      is_locked_(false),
      is_detached_(false),
      parent_(parent),
      client_(std::move(client)),
      frame_tracker_(new FrameTracker(client_.get(), this)),
      mobile_emulation_override_manager_(
          new MobileEmulationOverrideManager(client_.get(),
                                             std::move(mobile_device),
                                             browser_info->major_version)),
      geolocation_override_manager_(
          new GeolocationOverrideManager(client_.get())),
      network_conditions_override_manager_(
          new NetworkConditionsOverrideManager(client_.get())),
      heap_snapshot_taker_(new HeapSnapshotTaker(client_.get())),
      is_service_worker_(false),
      autoaccept_beforeunload_(autoaccept_beforeunload) {
  client_->SetAutoAcceptBeforeunload(autoaccept_beforeunload_);
  // Downloading in headless mode requires the setting of
  // Browser.setDownloadBehavior. This is handled by the
  // DownloadDirectoryOverrideManager, which is only instantiated
  // in headless chrome.
  if (browser_info->is_headless_shell) {
    download_directory_override_manager_ =
        std::make_unique<DownloadDirectoryOverrideManager>(client_.get());
  }
  // Child WebViews should not have their own navigation_tracker, but defer
  // all related calls to their parent. All WebViews must have either parent_
  // or navigation_tracker_
  if (parent_ == nullptr) {
    navigation_tracker_ = CreatePageLoadStrategy(page_load_strategy);
  }
  client_->SetOwner(this);
}

WebViewImpl::~WebViewImpl() = default;

std::unique_ptr<PageLoadStrategy> WebViewImpl::CreatePageLoadStrategy(
    const std::string& strategy) {
  if (strategy == PageLoadStrategy::kNone) {
    return std::make_unique<NonBlockingNavigationTracker>();
  } else if (strategy == PageLoadStrategy::kNormal) {
    return std::make_unique<NavigationTracker>(client_.get(), this, false);
  } else if (strategy == PageLoadStrategy::kEager) {
    return std::make_unique<NavigationTracker>(client_.get(), this, true);
  } else {
    NOTREACHED() << "invalid strategy '" << strategy << "'";
  }
}

WebView* WebViewImpl::GetTargetForFrame(const std::string& frame) {
  return frame.empty() ? this : GetFrameTracker()->GetTargetForFrame(frame);
}

bool WebViewImpl::IsServiceWorker() const {
  return is_service_worker_;
}

std::unique_ptr<WebViewImpl> WebViewImpl::CreateChild(
    const std::string& session_id,
    const std::string& target_id) const {
  // While there may be a deep hierarchy of WebViewImpl instances, the
  // hierarchy for DevToolsClientImpl is flat - there's a root which
  // sends/receives over the socket, and all child sessions are considered
  // its children (one level deep at most).
  std::unique_ptr<DevToolsClientImpl> child_client =
      std::make_unique<DevToolsClientImpl>(session_id, session_id);
  std::unique_ptr<WebViewImpl> child = std::make_unique<WebViewImpl>(
      target_id, w3c_compliant_, this, browser_info_, std::move(child_client),
      std::nullopt, "", autoaccept_beforeunload_);
  const WebViewImpl* root_view = this;
  while (root_view->parent_ != nullptr) {
    root_view = root_view->parent_;
  }
  PageLoadStrategy* navigation_tracker = root_view->navigation_tracker_.get();
  if (navigation_tracker && !navigation_tracker->IsNonBlocking()) {
    // Find Navigation Tracker for the top of the WebViewImpl hierarchy
    child->client_->AddListener(navigation_tracker);
  }
  return child;
}

std::string WebViewImpl::GetId() {
  return id_;
}

std::string WebViewImpl::GetSessionId() {
  return client_->SessionId();
}

bool WebViewImpl::WasCrashed() {
  return client_->WasCrashed();
}

Status WebViewImpl::AttachTo(DevToolsClient* root_client) {
  // Add this target holder to extend the lifetime of webview object.
  WebViewImplHolder target_holder(this);
  return client_->AttachTo(root_client);
}

Status WebViewImpl::AttachChildView(WebViewImpl* child) {
  DevToolsClient* root_client = client_.get();
  while (root_client->GetParentClient() != nullptr) {
    root_client = root_client->GetParentClient();
  }
  return child->AttachTo(root_client);
}

Status WebViewImpl::HandleEventsUntil(const ConditionalFunc& conditional_func,
                                      const Timeout& timeout) {
  return client_->HandleEventsUntil(conditional_func, timeout);
}

Status WebViewImpl::HandleReceivedEvents() {
  return client_->HandleReceivedEvents();
}

Status WebViewImpl::GetUrl(std::string* url) {
  base::Value::Dict params;
  base::Value::Dict result;
  Status status = client_->SendCommandAndGetResult("Page.getNavigationHistory",
                                                   params, &result);
  if (status.IsError())
    return status;
  std::optional<int> current_index = result.FindInt("currentIndex");
  if (!current_index)
    return Status(kUnknownError, "navigation history missing currentIndex");
  base::Value::List* entries = result.FindList("entries");
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
  if (base::StartsWith(url,
                       "javascript:", base::CompareCase::INSENSITIVE_ASCII)) {
    return Status(kUnknownError, "unsupported protocol");
  }
  base::Value::Dict params;
  params.Set("url", url);
  if (IsNonBlocking()) {
    // With non-blocking navigation tracker, the previous navigation might
    // still be in progress, and this can cause the new navigate command to be
    // ignored on Chrome v63 and above. Stop previous navigation first.
    client_->SendCommand("Page.stopLoading", base::Value::Dict());
    // Use SendCommandAndIgnoreResponse to ensure no blocking occurs.
    return client_->SendCommandAndIgnoreResponse("Page.navigate", params);
  }
  return client_->SendCommandWithTimeout("Page.navigate", params, timeout);
}

Status WebViewImpl::Reload(const Timeout* timeout) {
  base::Value::Dict params;
  params.Set("ignoreCache", false);
  return client_->SendCommandWithTimeout("Page.reload", params, timeout);
}

Status WebViewImpl::Freeze(const Timeout* timeout) {
  base::Value::Dict params;
  params.Set("state", "frozen");
  return client_->SendCommandWithTimeout("Page.setWebLifecycleState", params,
                                         timeout);
}

Status WebViewImpl::Resume(const Timeout* timeout) {
  base::Value::Dict params;
  params.Set("state", "active");
  return client_->SendCommandWithTimeout("Page.setWebLifecycleState", params,
                                         timeout);
}

Status WebViewImpl::StartBidiServer(std::string bidi_mapper_script) {
  return client_->StartBidiServer(std::move(bidi_mapper_script));
}

Status WebViewImpl::PostBidiCommand(base::Value::Dict command) {
  return client_->PostBidiCommand(std::move(command));
}

Status WebViewImpl::SendBidiCommand(base::Value::Dict command,
                                    const Timeout& timeout,
                                    base::Value::Dict& response) {
  WebViewImplHolder target_holder(this);
  Status status{kOk};
  BidiTrackerGuard bidi_tracker_guard(*client_);

  base::Value* maybe_cmd_id = command.Find("id");
  if (maybe_cmd_id == nullptr) {
    return Status{kUnknownError, "BiDi command has no 'id' of type js-uint"};
  }
  base::Value expected_id = maybe_cmd_id->Clone();

  std::string* maybe_channel = command.FindString("channel");
  if (maybe_channel == nullptr || !maybe_channel->starts_with("/")) {
    return Status{kUnknownError,
                  "BiDi command does not contain a non-empty string 'channel' "
                  "with a leading '/'"};
  }
  bidi_tracker_guard.Tracker().SetChannelSuffix(*maybe_channel);

  base::Value::Dict tmp;
  auto on_bidi_message = [](base::Value::Dict& destination,
                            base::Value::Dict payload) {
    destination = std::move(payload);
    return Status{kOk};
  };
  bidi_tracker_guard.Tracker().SetBidiCallback(
      base::BindRepeating(on_bidi_message, std::ref(tmp)));
  status = client_->PostBidiCommand(std::move(command));
  if (status.IsError()) {
    return status;
  }
  auto response_is_received = [](const base::Value& expected_id,
                                 const base::Value::Dict& destionation,
                                 bool* is_condition_met) {
    const base::Value* maybe_response_id = destionation.Find("id");
    *is_condition_met =
        (maybe_response_id != nullptr) && (expected_id == *maybe_response_id);
    return Status{kOk};
  };
  status = client_->HandleEventsUntil(
      base::BindRepeating(response_is_received, std::cref(expected_id),
                          std::cref(tmp)),
      timeout);
  if (status.IsError()) {
    return status;
  }
  response = std::move(tmp);
  return status;
}

Status WebViewImpl::SendCommand(const std::string& cmd,
                                const base::Value::Dict& params) {
  return client_->SendCommand(cmd, params);
}

Status WebViewImpl::SendCommandFromWebSocket(const std::string& cmd,
                                             const base::Value::Dict& params,
                                             const int client_cmd_id) {
  return client_->SendCommandFromWebSocket(cmd, params, client_cmd_id);
}

Status WebViewImpl::SendCommandAndGetResult(
    const std::string& cmd,
    const base::Value::Dict& params,
    std::unique_ptr<base::Value>* value) {
  base::Value::Dict result;
  Status status = client_->SendCommandAndGetResult(cmd, params, &result);
  if (status.IsError())
    return status;
  *value = std::make_unique<base::Value>(std::move(result));
  return Status(kOk);
}

Status WebViewImpl::TraverseHistory(int delta, const Timeout* timeout) {
  base::Value::Dict params;
  base::Value::Dict result;
  Status status = client_->SendCommandAndGetResult("Page.getNavigationHistory",
                                                   params, &result);
  if (status.IsError())
    return status;

  std::optional<int> current_index = result.FindInt("currentIndex");
  if (!current_index)
    return Status(kUnknownError, "DevTools didn't return currentIndex");

  base::Value::List* entries = result.FindList("entries");
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
  std::optional<int> entry_id = entry.GetDict().FindInt("id");
  if (!entry_id)
    return Status(kUnknownError, "history entry does not have an id");
  params.Set("entryId", *entry_id);

  return client_->SendCommandWithTimeout("Page.navigateToHistoryEntry", params,
                                         timeout);
}

Status WebViewImpl::GetLoaderId(const std::string& frame_id,
                                const Timeout& timeout,
                                std::string& loader_id) {
  Status status{kOk};

  base::Value::Dict frame_tree_result;
  status = client_->SendCommandAndGetResultWithTimeout(
      "Page.getFrameTree", base::Value::Dict(), &timeout, &frame_tree_result);
  if (status.IsError()) {
    return status;
  }

  base::Value::Dict* maybe_frame_tree = frame_tree_result.FindDict("frameTree");
  if (!maybe_frame_tree) {
    return Status{kUnknownError,
                  "no frameTree in the response to Page.getFrameTree"};
  }
  std::queue<base::Value::Dict*> q;
  for (q.push(maybe_frame_tree); !q.empty(); q.pop()) {
    base::Value::Dict* frame_tree = q.front();
    std::string* current_frame_id =
        frame_tree->FindStringByDottedPath("frame.id");
    if (!current_frame_id) {
      return Status{
          kUnknownError,
          "no frame.id in one of the nodes of the Page.getFrameTree response"};
    }
    std::string* current_loader_id =
        frame_tree->FindStringByDottedPath("frame.loaderId");
    if (!current_loader_id) {
      return Status{kUnknownError,
                    "no frame.loaderId in one of the nodes of the "
                    "Page.getFrameTree response"};
    }
    if (current_loader_id->empty()) {
      // There is probably an ongoing navigation. Giving up.
      return Status{kAbortedByNavigation,
                    "no loaderId found for the current frame"};
    }

    if (frame_id == *current_frame_id) {
      loader_id = std::move(*current_loader_id);
      break;
    }

    base::Value::List* child_frames = frame_tree->FindList("childFrames");
    if (!child_frames) {
      continue;
    }

    for (base::Value& item : (*child_frames)) {
      if (!item.is_dict()) {
        return Status{kUnknownError,
                      "child frame is not a dictionary in one of the nodes of "
                      "the Page.getFrameTree response"};
      }
      q.push(item.GetIfDict());
    }
  }
  return status;
}

Status WebViewImpl::CallFunctionWithTimeoutInternal(
    std::string frame,
    std::string function,
    base::Value::List args,
    const base::TimeDelta& timeout,
    bool include_shadow_root,
    std::unique_ptr<base::Value>* result) {
  Status status{kOk};

  std::string frame_id = frame.empty() ? id_ : frame;

  Timeout local_timeout(timeout);
  std::string loader_id;

  // The code below tries to detect if any navigation has happened during its
  // execution. The navigation is detected if either loaderId or
  // context_id has changed.

  status = GetLoaderId(frame_id, local_timeout, loader_id);
  if (status.IsError()) {
    return WrapIfTargetDetached(status, kAbortedByNavigation);
  }
  std::string context_id;
  status = GetFrameTracker()->GetContextIdForFrame(frame_id, &context_id);
  if (status.IsError()) {
    return WrapIfTargetDetached(status, kAbortedByNavigation);
  }

  ObjectGroup object_group(client_.get());

  base::Value::List nodes;
  // Resolving the references in the execution context obtained earlier.
  status =
      ResolveElementReferencesInPlace(frame_id, context_id, object_group.name(),
                                      loader_id, local_timeout, args, nodes);
  object_group.SetEmpty(nodes.empty());
  // kNoSuchElement is handled in special way:
  // If loader id has changed then the node was not resolved due to the
  // navigation.
  // Otherwise the user has sent us a node id that refers a non-existent node.
  if (status.IsError() && status.code() != kNoSuchElement) {
    return WrapIfTargetDetached(status, kAbortedByNavigation);
  }

  std::string new_loader_id;
  Status new_status = GetLoaderId(frame_id, local_timeout, new_loader_id);
  if (new_status.IsError()) {
    return WrapIfTargetDetached(new_status, kAbortedByNavigation);
  }
  if (new_loader_id != loader_id) {
    // A navigation has happened while resolving references. Giving up.
    return Status{kAbortedByNavigation,
                  "loader has changed while resolving nodes"};
  }
  // ResolveElementReferences returned kNoSuchElement.
  // The loader did not change therefore the node indeed does not exist.
  if (status.IsError()) {
    return status;
  }

  std::string new_context_id;
  status = GetFrameTracker()->GetContextIdForFrame(frame_id, &new_context_id);
  if (status.IsError()) {
    return WrapIfTargetDetached(status, kAbortedByNavigation);
  }
  if (context_id != new_context_id) {
    return Status{kAbortedByNavigation,
                  "context has changed while resolving nodes"};
  }

  // All BackendNodeId's have been resolved in the same context and using the
  // same loader. The remote call will succeed if the execution context does not
  // change in the mean time. This is detected by the remote code implementing
  // Runtime.callFunctionOn.

  std::string json;
  base::JSONWriter::Write(args, &json);
  std::string w3c = w3c_compliant_ ? "true" : "false";
  // TODO(zachconrad): Second null should be array of shadow host ids.
  std::string wrapper_function = base::StringPrintf(
      "function(){ return (%s).apply(null, [%s, %s, %s, arguments]); }",
      kCallFunctionScript, function.c_str(), json.c_str(), w3c.c_str());

  base::Value::Dict params;
  params.Set("functionDeclaration", wrapper_function);
  if (!context_id.empty()) {
    params.Set("uniqueContextId", context_id);
  }
  params.Set("arguments", std::move(nodes));
  params.Set("awaitPromise", true);
  if (!object_group.IsEmpty()) {
    params.Set("objectGroup", object_group.name());
  }

  base::Value::Dict serialization_options;
  serialization_options.Set("serialization", "deep");

  params.Set("serializationOptions", std::move(serialization_options));

  base::Value::Dict cmd_result;

  status = client_->SendCommandAndGetResultWithTimeout(
      "Runtime.callFunctionOn", params, &local_timeout, &cmd_result);
  if (status.IsError()) {
    return status;
  }

  if (cmd_result.contains("exceptionDetails")) {
    std::string description = "unknown";
    if (const std::string* maybe_description =
            cmd_result.FindStringByDottedPath("result.description")) {
      description = *maybe_description;
    }
    return Status(kUnknownError,
                  "Runtime.callFunctionOn threw exception: " + description);
  }

  base::Value::List* maybe_received_list =
      cmd_result.FindListByDottedPath("result.deepSerializedValue.value");
  if (!maybe_received_list || maybe_received_list->empty()) {
    return Status(kUnknownError,
                  "result.deepSerializedValue.value list is missing or empty "
                  "in Runtime.callFunctionOn response");
  }
  base::Value::List& received_list = *maybe_received_list;

  if (!received_list[0].is_dict()) {
    return Status(kUnknownError,
                  "first element in result.deepSerializedValue.value list must "
                  "be a dictionary");
  }
  std::string* serialized_value =
      received_list[0].GetDict().FindString("value");
  if (!serialized_value) {
    return Status(kUnknownError,
                  "first element in result.deepSerializedValue.value list must "
                  "contain a string");
  }
  std::optional<base::Value> maybe_call_result =
      base::JSONReader::Read(*serialized_value, base::JSON_PARSE_RFC);
  if (!maybe_call_result) {
    return Status{kUnknownError,
                  "cannot deserialize the result value received from "
                  "Runtime.callFunctionOn"};
  }
  received_list.erase(received_list.begin());
  if (!maybe_call_result->is_dict()) {
    return Status{
        kUnknownError,
        "deserialized Runtime.callFunctionOn result is not a dictionary"};
  }
  base::Value::Dict& call_result = maybe_call_result->GetDict();

  std::optional<int> status_code = call_result.FindInt("status");
  if (!status_code) {
    return Status(kUnknownError, "call function result missing int 'status'");
  }
  if (*status_code != kOk) {
    const std::string* message = call_result.FindString("value");
    return Status(static_cast<StatusCode>(*status_code),
                  message ? *message : "");
  }
  base::Value* call_result_value = call_result.Find("value");
  if (call_result_value == nullptr) {
    // Missing 'value' indicates the JavaScript code didn't return a value.
    return Status(kOk);
  }
  status = ResolveWeakReferences(received_list);
  if (!status.IsOk()) {
    return status;
  }
  status = CreateElementReferences(frame_id, received_list, include_shadow_root,
                                   *call_result_value);
  if (!status.IsOk()) {
    return status;
  }

  *result = std::make_unique<base::Value>(std::move(*call_result_value));
  return status;
}

Status WebViewImpl::EvaluateScript(const std::string& frame,
                                   const std::string& expression,
                                   const bool await_promise,
                                   std::unique_ptr<base::Value>* result) {
  WebViewImplHolder target_holder(this);
  Status status{kOk};

  WebView* target = GetTargetForFrame(frame);
  if (target != nullptr && target != this) {
    if (target->IsDetached())
      return Status(kTargetDetached);
    return target->EvaluateScript(frame, expression, await_promise, result);
  }

  std::string context_id;
  status = GetContextIdForFrame(this, frame, &context_id);
  if (status.IsError())
    return status;
  // If the target associated with the current view or its ancestor is detached
  // during the script execution we don't want deleting the current WebView
  // because we are executing the code in its method.
  // Instead we lock the WebView with target holder and only label the view as
  // detached.
  const base::TimeDelta& timeout = base::TimeDelta::Max();
  return internal::EvaluateScriptAndGetValue(
      client_.get(), context_id, expression, timeout, await_promise, result);
}

Status WebViewImpl::CallFunctionWithTimeout(
    const std::string& frame,
    const std::string& function,
    const base::Value::List& args,
    const base::TimeDelta& timeout,
    const CallFunctionOptions& options,
    std::unique_ptr<base::Value>* result) {
  WebViewImplHolder target_holder(this);
  Status status{kOk};

  WebView* target = GetTargetForFrame(frame);
  if (target != nullptr && target != this) {
    if (target->IsDetached()) {
      return Status(kTargetDetached);
    }
    return target->CallFunctionWithTimeout(frame, function, args, timeout,
                                           options, result);
  }

  return CallFunctionWithTimeoutInternal(frame, std::move(function),
                                         args.Clone(), timeout,
                                         options.include_shadow_root, result);
}

Status WebViewImpl::CallFunction(const std::string& frame,
                                 const std::string& function,
                                 const base::Value::List& args,
                                 std::unique_ptr<base::Value>* result) {
  // Timeout set to Max is treated as no timeout.

  CallFunctionOptions options;
  options.include_shadow_root = false;
  return CallFunctionWithTimeout(frame, function, args, base::TimeDelta::Max(),
                                 options, result);
}

Status WebViewImpl::CallUserSyncScript(const std::string& frame,
                                       const std::string& script,
                                       const base::Value::List& args,
                                       const base::TimeDelta& timeout,
                                       std::unique_ptr<base::Value>* result) {
  WebViewImplHolder target_holder(this);
  Status status{kOk};

  WebView* target = GetTargetForFrame(frame);
  if (target != nullptr && target != this) {
    if (target->IsDetached()) {
      return Status(kTargetDetached);
    }
    return target->CallUserSyncScript(frame, script, args, timeout, result);
  }

  base::Value::List sync_args;
  sync_args.Append(script);
  sync_args.Append(args.Clone());

  return CallFunctionWithTimeoutInternal(
      frame, kExecuteScriptScript, sync_args.Clone(), timeout, false, result);
}

Status WebViewImpl::CallUserAsyncFunction(
    const std::string& frame,
    const std::string& function,
    const base::Value::List& args,
    const base::TimeDelta& timeout,
    std::unique_ptr<base::Value>* result) {
  return CallAsyncFunctionInternal(frame, function, args, timeout, result);
}

// TODO (crbug.com/chromedriver/4364): Simplify this function
Status WebViewImpl::GetFrameByFunction(const std::string& frame,
                                       const std::string& function,
                                       const base::Value::List& args,
                                       std::string* out_frame) {
  WebViewImplHolder target_holder(this);
  Status status{kOk};

  WebView* target = GetTargetForFrame(frame);
  if (target != nullptr && target != this) {
    if (target->IsDetached())
      return Status(kTargetDetached);
    return target->GetFrameByFunction(frame, function, args, out_frame);
  }

  std::unique_ptr<base::Value> result;
  status = CallFunctionWithTimeoutInternal(
      frame, function, args.Clone(), base::TimeDelta::Max(), false, &result);

  if (status.IsError()) {
    return status;
  }

  if (!result->is_dict()) {
    return Status{kNoSuchFrame};
  }

  const base::Value::Dict& element_dict = result->GetDict();

  const std::string element_key = w3c_compliant_ ? kElementKeyW3C : kElementKey;
  const std::string* element_id = element_dict.FindString(element_key);
  if (element_id == nullptr) {
    element_id = element_dict.FindString(kShadowRootKey);
  }
  if (element_id == nullptr) {
    return Status{kNoSuchFrame, "invalid element id"};
  }

  std::string extracted_frame_id;
  std::string extracted_loader_id;
  int backend_node_id;
  status = SplitElementId(*element_id, kNoSuchFrame, extracted_frame_id,
                          extracted_loader_id, backend_node_id);
  if (status.IsError()) {
    return status;
  }

  bool found_node = false;
  status = GetFrameIdForBackendNodeId(client_.get(), backend_node_id,
                                      &found_node, out_frame);
  if (status.IsError()) {
    return status;
  }

  if (!found_node) {
    return Status(kNoSuchFrame);
  }

  return status;
}

Status WebViewImpl::DispatchTouchEventsForMouseEvents(
    const std::vector<MouseEvent>& events,
    const std::string& frame) {
  // Touch events are filtered by the compositor if there are no touch listeners
  // on the page. Wait two frames for the compositor to sync with the main
  // thread to get consistent behavior.
  base::Value::Dict promise_params;
  promise_params.Set(
      "expression",
      "new Promise(x => setTimeout(() => setTimeout(x, 20), 20))");
  promise_params.Set("awaitPromise", true);
  client_->SendCommand("Runtime.evaluate", promise_params);
  for (const MouseEvent& event : events) {
    base::Value::Dict params;

    switch (event.type) {
      case kPressedMouseEventType:
        params.Set("type", "touchStart");
        break;
      case kReleasedMouseEventType:
        params.Set("type", "touchEnd");
        break;
      case kMovedMouseEventType:
        if (event.button == kNoneMouseButton) {
          continue;
        }
        params.Set("type", "touchMove");
        break;
      default:
        break;
    }

    base::Value::List touch_points;
    if (event.type != kReleasedMouseEventType) {
      base::Value::Dict touch_point;
      touch_point.Set("x", event.x);
      touch_point.Set("y", event.y);
      touch_points.Append(std::move(touch_point));
    }
    params.Set("touchPoints", std::move(touch_points));
    params.Set("modifiers", event.modifiers);
    Status status = client_->SendCommand("Input.dispatchTouchEvent", params);
    if (status.IsError())
      return status;
  }
  return Status(kOk);
}

Status WebViewImpl::DispatchMouseEvents(const std::vector<MouseEvent>& events,
                                        const std::string& frame,
                                        bool async_dispatch_events) {
  WebViewImplHolder target_holder(this);
  if (mobile_emulation_override_manager_->IsEmulatingTouch())
    return DispatchTouchEventsForMouseEvents(events, frame);

  Status status(kOk);
  for (auto it = events.begin(); it != events.end(); ++it) {
    base::Value::Dict params;
    std::string type = GetAsString(it->type);
    params.Set("type", type);
    params.Set("x", it->x);
    params.Set("y", it->y);
    params.Set("modifiers", it->modifiers);
    params.Set("button", GetAsString(it->button));
    params.Set("buttons", it->buttons);
    params.Set("clickCount", it->click_count);
    params.Set("force", it->force);
    params.Set("tangentialPressure", it->tangentialPressure);
    params.Set("tiltX", it->tiltX);
    params.Set("tiltY", it->tiltY);
    params.Set("twist", it->twist);
    params.Set("pointerType", GetAsString(it->pointer_type));
    if (type == "mouseWheel") {
      params.Set("deltaX", it->delta_x);
      params.Set("deltaY", it->delta_y);
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
  base::Value::Dict params;
  std::string type = GetAsString(event.type);
  params.Set("type", type);
  base::Value::List point_list;
  Status status(kOk);
  if (type == "touchStart" || type == "touchMove") {
    base::Value::Dict point = GenerateTouchPoint(event);
    point_list.Append(std::move(point));
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

  base::Value::Dict params;
  Status status(kOk);
  size_t touch_count = 1;
  for (const TouchEvent& event : events) {
    base::Value::List point_list;
    double current_time = base::Time::Now().InSecondsFSinceUnixEpoch();
    params.Set("timestamp", current_time);
    std::string type = GetAsString(event.type);
    params.Set("type", type);
    if (type == "touchCancel")
      continue;

    point_list.Append(GenerateTouchPoint(event));
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
    base::Value::Dict params;
    params.Set("type", GetAsString(it->type));
    if (it->modifiers & kNumLockKeyModifierMask) {
      params.Set("isKeypad", true);
      params.Set("modifiers", it->modifiers & ~kNumLockKeyModifierMask);
    } else {
      params.Set("modifiers", it->modifiers);
    }
    params.Set("text", it->modified_text);
    params.Set("unmodifiedText", it->unmodified_text);
    params.Set("windowsVirtualKeyCode", it->key_code);
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
      params.Set("code", code);
    if (!it->key.empty())
      params.Set("key", it->key);
    else if (it->is_from_action)
      params.Set("key", it->modified_text);

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

      base::Value::List command_list;
      command_list.Append(command);
      params.Set("commands", std::move(command_list));
    }

    if (it->location != 0) {
      // The |location| parameter in DevTools protocol only accepts 1 (left
      // modifiers) and 2 (right modifiers). For location 3 (numeric keypad),
      // it is necessary to set the |isKeypad| parameter.
      if (it->location == 3)
        params.Set("isKeypad", true);
      else
        params.Set("location", it->location);
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
  base::Value::Dict params;
  base::Value::Dict result;

  if (browser_info_->browser_name != "webview") {
    base::Value::List url_list;
    url_list.Append(current_page_url);
    params.Set("urls", std::move(url_list));
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

  base::Value::List* const cookies_tmp = result.FindList("cookies");
  if (!cookies_tmp)
    return Status(kUnknownError, "DevTools didn't return cookies");
  *cookies = base::Value(std::move(*cookies_tmp));
  return Status(kOk);
}

Status WebViewImpl::DeleteCookie(const std::string& name,
                                 const std::string& url,
                                 const std::string& domain,
                                 const std::string& path) {
  base::Value::Dict params;
  params.Set("url", url);
  std::string command;
  params.Set("name", name);
  params.Set("domain", domain);
  params.Set("path", path);
  command = "Network.deleteCookies";
  return client_->SendCommand(command, params);
}

Status WebViewImpl::AddCookie(const std::string& name,
                              const std::string& url,
                              const std::string& value,
                              const std::string& domain,
                              const std::string& path,
                              const std::string& same_site,
                              bool secure,
                              bool http_only,
                              double expiry) {
  base::Value::Dict params;
  params.Set("name", name);
  params.Set("url", url);
  params.Set("value", value);
  params.Set("domain", domain);
  params.Set("path", path);
  params.Set("secure", secure);
  params.Set("httpOnly", http_only);
  if (!same_site.empty())
    params.Set("sameSite", same_site);
  if (expiry >= 0)
    params.Set("expires", expiry);

  base::Value::Dict result;
  Status status =
      client_->SendCommandAndGetResult("Network.setCookie", params, &result);
  if (status.IsError())
    return Status(kUnableToSetCookie);
  if (!result.FindBool("success").value_or(false))
    return Status(kUnableToSetCookie);
  return Status(kOk);
}

Status WebViewImpl::WaitForPendingNavigations(const std::string& frame_id,
                                              const Timeout& timeout,
                                              bool stop_load_on_timeout) {
  // This function should not be called for child WebViews
  if (parent_ != nullptr)
    return Status(kUnknownError,
                  "Call WaitForPendingNavigations only on the parent WebView");
  VLOG(0) << "Waiting for pending navigations...";
  const auto not_pending_navigation = base::BindRepeating(
      &WebViewImpl::IsNotPendingNavigation, base::Unretained(this),
      frame_id.empty() ? id_ : frame_id, base::Unretained(&timeout));
  // If the target associated with the current view or its ancestor is detached
  // while we are waiting for the pending navigation we don't want deleting the
  // current WebView because we are executing the code in its method. Instead we
  // lock the WebView with target holder and only label the view as detached.
  WebViewImplHolder target_holder(this);
  bool keep_waiting = true;
  Status status{kOk};
  while (keep_waiting) {
    status = client_->HandleEventsUntil(not_pending_navigation, timeout);
    keep_waiting = status.code() == kNoSuchExecutionContext ||
                   status.code() == kAbortedByNavigation;
  }
  if (status.code() == kTimeout && stop_load_on_timeout) {
    VLOG(0) << "Timed out. Stopping navigation...";
    navigation_tracker_->set_timed_out(true);
    client_->SendCommand("Page.stopLoading", base::Value::Dict());
    // We don't consider |timeout| here to make sure the navigation actually
    // stops and we cleanup properly after a command that caused a navigation
    // that timed out.  Otherwise we might have to wait for that before
    // executing the next command, and it will be counted towards its timeout.
    Status new_status{kOk};
    keep_waiting = true;
    while (keep_waiting) {
      new_status = client_->HandleEventsUntil(
          not_pending_navigation,
          Timeout(base::Seconds(kWaitForNavigationStopSeconds)));
      keep_waiting = status.code() == kNoSuchExecutionContext ||
                     status.code() == kAbortedByNavigation;
    }
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
  return parent_->IsPendingNavigation(timeout, is_pending);
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
  if (download_directory_override_manager_) {
    return download_directory_override_manager_
        ->OverrideDownloadDirectoryWhenConnected(download_directory);
  }
  return Status(kOk);
}

Status WebViewImpl::CaptureScreenshot(std::string* screenshot,
                                      const base::Value::Dict& params) {
  base::Value::Dict result;
  Timeout timeout(base::Seconds(10));
  Status status = client_->SendCommandAndGetResultWithTimeout(
      "Page.captureScreenshot", params, &timeout, &result);
  if (status.IsError())
    return status;
  std::string* data = result.FindString("data");
  if (!data)
    return Status(kUnknownError, "expected string 'data' in response");
  *screenshot = std::move(*data);
  return Status(kOk);
}

Status WebViewImpl::PrintToPDF(const base::Value::Dict& params,
                               std::string* pdf) {
  base::Value::Dict result;
  Timeout timeout(base::Seconds(10));
  Status status = client_->SendCommandAndGetResultWithTimeout(
      "Page.printToPDF", params, &timeout, &result);
  if (status.IsError()) {
    if (status.code() == kUnknownError) {
      return Status(kInvalidArgument, status);
    }
    return status;
  }
  std::string* data = result.FindString("data");
  if (!data)
    return Status(kUnknownError, "expected string 'data' in response");
  *pdf = std::move(*data);
  return Status(kOk);
}

Status WebViewImpl::GetBackendNodeIdByElement(const std::string& frame,
                                              const base::Value& element,
                                              int* backend_node_id) {
  Status status{kOk};

  WebView* target = GetTargetForFrame(frame);
  if (target == nullptr || target->IsDetached()) {
    return Status(kNoSuchWindow, "frame not found");
  }

  if (!element.is_dict())
    return Status(kUnknownError, "'element' is not a dictionary");

  const base::Value::Dict& element_dict = element.GetDict();

  const std::string element_key = w3c_compliant_ ? kElementKeyW3C : kElementKey;
  StatusCode no_such_element = kNoSuchElement;
  StatusCode stale_element = kStaleElementReference;
  const std::string* element_id = element_dict.FindString(element_key);
  if (element_id == nullptr) {
    no_such_element = kNoSuchShadowRoot;
    stale_element = kDetachedShadowRoot;
    element_id = element_dict.FindString(kShadowRootKey);
  }
  if (element_id == nullptr) {
    if (element_dict.contains(element_key) ||
        element_dict.contains(kShadowRootKey)) {
      return Status{kInvalidArgument, "invalid element id"};
    }
    return Status{kNoSuchElement,
                  "element id does not refer an element or a shadow root"};
  }

  std::string extracted_frame_id;
  std::string extracted_loader_id;
  int extracted_backend_node_id;
  status = SplitElementId(*element_id, no_such_element, extracted_frame_id,
                          extracted_loader_id, extracted_backend_node_id);
  if (status.IsError()) {
    return status;
  }

  std::string frame_id = frame.empty() ? id_ : frame;
  if (frame_id != extracted_frame_id) {
    return Status{no_such_element, GetDefaultMessage(no_such_element)};
  }
  Timeout local_timeout(base::TimeDelta::Max());
  std::string loader_id;
  status = GetLoaderId(frame_id, local_timeout, loader_id);
  if (status.IsError()) {
    return status;
  }
  if (loader_id != extracted_loader_id) {
    return Status{stale_element, GetDefaultMessage(stale_element)};
  }

  *backend_node_id = extracted_backend_node_id;
  return status;
}

Status WebViewImpl::SetFileInputFiles(const std::string& frame,
                                      const base::Value& element,
                                      const std::vector<base::FilePath>& files,
                                      const bool append) {
  WebViewImplHolder target_holder(this);
  Status status{kOk};

  if (!element.is_dict())
    return Status(kUnknownError, "'element' is not a dictionary");

  WebView* target = GetTargetForFrame(frame);
  if (target != nullptr && target != this) {
    if (target->IsDetached())
      return Status(kTargetDetached);
    return target->SetFileInputFiles(frame, element, files, append);
  }

  int backend_node_id;
  status = GetBackendNodeIdByElement(frame, element, &backend_node_id);
  if (status.IsError())
    return status;

  base::Value::List file_list;
  // if the append flag is true, we need to retrieve the files that
  // already exist in the element and add them too.
  // Additionally, we need to add the old files first so that it looks
  // like we're appending files.
  if (append) {
    // Convert the node_id to a Runtime.RemoteObject
    std::string inner_remote_object_id;
    status = ResolveNode(*client_, backend_node_id, "", nullptr,
                         inner_remote_object_id);
    if (status.IsError()) {
      return status;
    }

    // figure out how many files there are
    std::optional<int> number_of_files;
    {
      base::Value::Dict cmd_result;
      base::Value::Dict params;
      params.Set("functionDeclaration",
                 "function() { return this.files.length }");
      params.Set("objectId", inner_remote_object_id);
      status = client_->SendCommandAndGetResult("Runtime.callFunctionOn",
                                                params, &cmd_result);
      if (status.IsError())
        return status;
      number_of_files = cmd_result.FindIntByDottedPath("result.value");
      if (!number_of_files)
        return Status(kUnknownError, "DevTools didn't return value");
    }

    // Ask for each Runtime.RemoteObject and add them to the list
    for (int i = 0; i < *number_of_files; i++) {
      std::string file_object_id;
      {
        base::Value::Dict cmd_result;
        base::Value::Dict params;
        params.Set("functionDeclaration", "function() { return this.files[" +
                                              base::NumberToString(i) + "] }");
        params.Set("objectId", inner_remote_object_id);

        status = client_->SendCommandAndGetResult("Runtime.callFunctionOn",
                                                  params, &cmd_result);
        if (status.IsError())
          return status;
        std::string* object_id =
            cmd_result.FindStringByDottedPath("result.objectId");
        if (!object_id)
          return Status(kUnknownError, "DevTools didn't return objectId");
        file_object_id = std::move(*object_id);
      }

      // Now convert each RemoteObject into the full path
      {
        base::Value::Dict params;
        params.Set("objectId", file_object_id);
        base::Value::Dict get_file_info_result;
        status = client_->SendCommandAndGetResult("DOM.getFileInfo", params,
                                                  &get_file_info_result);
        if (status.IsError())
          return status;
        // Add the full path to the file_list
        std::string* full_path = get_file_info_result.FindString("path");
        if (!full_path)
          return Status(kUnknownError, "DevTools didn't return path");
        file_list.Append(std::move(*full_path));
      }
    }
  }

  // Now add the new files
  for (const base::FilePath& file_path : files) {
    if (!file_path.IsAbsolute()) {
      return Status(kUnknownError,
                    "path is not absolute: " + file_path.AsUTF8Unsafe());
    }
    if (file_path.ReferencesParent()) {
      return Status(kUnknownError,
                    "path is not canonical: " + file_path.AsUTF8Unsafe());
    }
    file_list.Append(file_path.AsUTF8Unsafe());
  }

  base::Value::Dict set_files_params;
  set_files_params.Set("backendNodeId", backend_node_id);
  set_files_params.Set("files", std::move(file_list));
  return client_->SendCommand("DOM.setFileInputFiles", set_files_params);
}

Status WebViewImpl::TakeHeapSnapshot(std::unique_ptr<base::Value>* snapshot) {
  return heap_snapshot_taker_->TakeSnapshot(snapshot);
}

Status WebViewImpl::InitProfileInternal() {
  base::Value::Dict params;

  return client_->SendCommand("Profiler.enable", params);
}

Status WebViewImpl::StopProfileInternal() {
  base::Value::Dict params;
  Status status_debug = client_->SendCommand("Debugger.disable", params);
  Status status_profiler = client_->SendCommand("Profiler.disable", params);

  if (status_debug.IsError())
    return status_debug;
  if (status_profiler.IsError())
    return status_profiler;

  return Status(kOk);
}

Status WebViewImpl::StartProfile() {
  Status status_init = InitProfileInternal();

  if (status_init.IsError())
    return status_init;

  base::Value::Dict params;
  return client_->SendCommand("Profiler.start", params);
}

Status WebViewImpl::EndProfile(std::unique_ptr<base::Value>* profile_data) {
  base::Value::Dict params;
  base::Value::Dict profile_result;

  Status status = client_->SendCommandAndGetResult("Profiler.stop", params,
                                                   &profile_result);

  if (status.IsError()) {
    Status disable_profile_status = StopProfileInternal();
    if (disable_profile_status.IsError())
      return disable_profile_status;
    return status;
  }

  *profile_data = std::make_unique<base::Value>(std::move(profile_result));
  return status;
}

Status WebViewImpl::SynthesizeTapGesture(int x,
                                         int y,
                                         int tap_count,
                                         bool is_long_press) {
  base::Value::Dict params;
  params.Set("x", x);
  params.Set("y", y);
  params.Set("tapCount", tap_count);
  if (is_long_press)
    params.Set("duration", 1500);
  return client_->SendCommand("Input.synthesizeTapGesture", params);
}

Status WebViewImpl::SynthesizeScrollGesture(int x,
                                            int y,
                                            int xoffset,
                                            int yoffset) {
  base::Value::Dict params;
  params.Set("x", x);
  params.Set("y", y);
  // Chrome's synthetic scroll gesture is actually a "swipe" gesture, so the
  // direction of the swipe is opposite to the scroll (i.e. a swipe up scrolls
  // down, and a swipe left scrolls right).
  params.Set("xDistance", -xoffset);
  params.Set("yDistance", -yoffset);
  return client_->SendCommand("Input.synthesizeScrollGesture", params);
}

Status WebViewImpl::CallAsyncFunctionInternal(
    const std::string& frame,
    const std::string& function,
    const base::Value::List& args,
    const base::TimeDelta& timeout,
    std::unique_ptr<base::Value>* result) {
  base::Value::List async_args;
  async_args.Append("return (" + function + ").apply(null, arguments);");
  async_args.Append(args.Clone());
  /*is_user_supplied=*/
  async_args.Append(true);
  /*timeout=*/
  async_args.Append(timeout.InMicrosecondsF());
  std::unique_ptr<base::Value> tmp;
  Timeout local_timeout(timeout);
  std::unique_ptr<base::Value> query_value;
  CallFunctionOptions options;
  options.include_shadow_root = false;
  Status status =
      CallFunctionWithTimeout(frame, kExecuteAsyncScriptScript, async_args,
                              timeout, options, &query_value);
  if (status.IsError()) {
    return status;
  }

  base::Value::Dict* result_info = query_value->GetIfDict();
  if (!result_info) {
    return Status(kUnknownError, "async result info is not a dictionary");
  }
  std::optional<int> status_code = result_info->FindInt("status");
  if (!status_code) {
    return Status(kUnknownError, "async result info has no int 'status'");
  }
  if (*status_code != kOk) {
    const std::string* message = result_info->FindString("value");
    return Status(static_cast<StatusCode>(*status_code),
                  message ? *message : "");
  }
  base::Value* value = result_info->Find("value");
  if (!value) {
    return Status{kJavaScriptError,
                  "no value field in Runtime.callFunctionOn result"};
  }
  *result = base::Value::ToUniquePtrValue(value->Clone());
  return Status(kOk);
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
  bool is_pending = false;
  Status status =
      navigation_tracker_->IsPendingNavigation(timeout, &is_pending);
  if (status.IsError())
    return status;
  // An alert may block the pending navigation.
  if (client_->IsDialogOpen()) {
    std::string alert_text;
    status = client_->GetDialogMessage(alert_text);
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
  return parent_->IsNonBlocking();
}

Status WebViewImpl::GetFedCmTracker(FedCmTracker** out_tracker) {
  if (!fedcm_tracker_) {
    fedcm_tracker_ = std::make_unique<FedCmTracker>(client_.get());
    Status status = fedcm_tracker_->Enable(client_.get());
    if (!status.IsOk()) {
      fedcm_tracker_.reset();
      return status;
    }
  }
  *out_tracker = fedcm_tracker_.get();
  return Status(kOk);
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

namespace {

Status ResolveElementReferenceInPlace(DevToolsClient& client,
                                      const std::string& expected_frame_id,
                                      const std::string& context_id,
                                      const std::string& object_group_name,
                                      const std::string& expected_loader_id,
                                      const std::string& key,
                                      const Timeout& timeout,
                                      base::Value::Dict& arg_dict,
                                      base::Value::List& nodes) {
  Status status{kOk};
  const StatusCode no_such_element =
      key == kShadowRootKey ? kNoSuchShadowRoot : kNoSuchElement;
  const StatusCode stale_element =
      key == kShadowRootKey ? kDetachedShadowRoot : kStaleElementReference;

  const std::string* element_id = arg_dict.FindString(key);
  if (element_id == nullptr) {
    return Status{kInvalidArgument, "invalid element id"};
  }

  std::string frame_id;
  std::string loader_id;
  int backend_node_id;
  status = SplitElementId(*element_id, no_such_element, frame_id, loader_id,
                          backend_node_id);
  if (status.IsError()) {
    return status;
  }

  // The following two conditionals mimic a weak map without storing any
  // returned references. If the reference was indeed returned in this or in a
  // previous navigation of the current frame then its frame id must coincide
  // with the current frame id. Otherwise the reference is unknown for this
  // frame.
  if (frame_id != expected_frame_id) {
    return Status{no_such_element, GetDefaultMessage(no_such_element)};
  }
  // Any reference returned in the current navigation must have a matching
  // loader id. Otherwise the reference is stale.
  if (loader_id != expected_loader_id) {
    return Status{stale_element, GetDefaultMessage(stale_element)};
  }

  std::string object_id;
  status = ResolveNode(client, backend_node_id, object_group_name, &timeout,
                       object_id);
  if (status.IsError()) {
    if (status.code() == kNoSuchElement) {
      // If the node with given backend node id is not found then it was removed
      // and therefore the reference is stale.
      return Status{stale_element, GetDefaultMessage(stale_element)};
    }
    return status;
  }

  arg_dict.Set(std::move(key), static_cast<int>(nodes.size()));

  base::Value::Dict node;
  node.Set("objectId", std::move(object_id));
  nodes.Append(std::move(node));
  return status;
}

Status ResolveWindowReferenceInPlace(DevToolsClient& client,
                                     const std::string& expected_frame_id,
                                     const std::string& context_id,
                                     const std::string& object_group_name,
                                     const Timeout& timeout,
                                     base::Value::Dict& arg_dict,
                                     base::Value::List& nodes) {
  Status status{kOk};

  const std::string* element_id = arg_dict.FindString(kWindowKey);
  if (element_id == nullptr) {
    return Status{kInvalidArgument, "invalid element id"};
  }

  const std::string& frame_id = *element_id;

  if (frame_id != expected_frame_id) {
    // (crbug.com/372153090) References on dynamic windows are not yet
    // supported
    return Status(kNoSuchWindow, "Window with such id was not found");
  }

  base::Value::Dict params;
  params.Set("objectGroup", object_group_name);
  params.Set("expression", "window");
  params.Set("awaitPromise", true);
  if (!context_id.empty()) {
    params.Set("uniqueContextId", context_id);
  }
  base::Value::Dict evaluate_result;
  status = client.SendCommandAndGetResultWithTimeout(
      "Runtime.evaluate", params, &timeout, &evaluate_result);
  if (status.IsError()) {
    return status;
  }
  std::string* object_id =
      evaluate_result.FindStringByDottedPath("result.objectId");
  if (!object_id) {
    return Status{
        kUnknownError,
        "object.objectId is missing in the response to Runtime.evaluate"};
  }

  arg_dict.Set(kWindowKey, static_cast<int>(nodes.size()));

  base::Value::Dict node;
  node.Set("objectId", std::move(*object_id));
  nodes.Append(std::move(node));
  return status;
}

Status ResolveFrameReferenceInPlace(DevToolsClient& client,
                                    const std::string& expected_frame_id,
                                    const std::string& context_id,
                                    const std::string& object_group_name,
                                    const Timeout& timeout,
                                    base::Value::Dict& arg_dict,
                                    base::Value::List& nodes) {
  Status status{kOk};

  const std::string* element_id = arg_dict.FindString(kFrameKey);
  if (element_id == nullptr) {
    return Status{kInvalidArgument, "invalid element id"};
  }

  const std::string& frame_id = *element_id;

  base::Value::Dict params;
  base::Value::Dict get_frame_owner_result;
  params.Set("frameId", frame_id);
  status = client.SendCommandAndGetResultWithTimeout(
      "DOM.getFrameOwner", params, &timeout, &get_frame_owner_result);
  if (status.IsError()) {
    return status;
  }
  std::optional<int> maybe_backend_node_id =
      get_frame_owner_result.FindInt("backendNodeId");
  if (!maybe_backend_node_id.has_value()) {
    return Status{kUnknownError, "Unable to find backendNodeId for the frame"};
  }
  int backend_node_id = maybe_backend_node_id.value();

  std::string object_id;
  status = ResolveNode(client, backend_node_id, object_group_name, &timeout,
                       object_id);
  if (status.IsError()) {
    if (status.code() == kNoSuchElement) {
      // If the node with given backend node id is not found then it was removed
      // and therefore the reference is stale.
      status =
          Status{kNoSuchFrame, "frame was deleted while being dereferenced"};
    }
    return status;
  }

  arg_dict.Set(kFrameKey, static_cast<int>(nodes.size()));

  base::Value::Dict node;
  node.Set("objectId", std::move(object_id));
  nodes.Append(std::move(node));
  return status;
}

}  // namespace

Status WebViewImpl::ResolveElementReferencesInPlace(
    const std::string& expected_frame_id,
    const std::string& context_id,
    const std::string& object_group_name,
    const std::string& expected_loader_id,
    const Timeout& timeout,
    base::Value::Dict& arg_dict,
    base::Value::List& nodes) {
  if (arg_dict.contains(kElementKeyW3C)) {
    return ResolveElementReferenceInPlace(
        *client_, expected_frame_id, context_id, object_group_name,
        expected_loader_id, kElementKeyW3C, timeout, arg_dict, nodes);
  }
  if (arg_dict.contains(kElementKey)) {
    return ResolveElementReferenceInPlace(
        *client_, expected_frame_id, context_id, object_group_name,
        expected_loader_id, kElementKey, timeout, arg_dict, nodes);
  }
  if (arg_dict.contains(kShadowRootKey)) {
    return ResolveElementReferenceInPlace(
        *client_, expected_frame_id, context_id, object_group_name,
        expected_loader_id, kShadowRootKey, timeout, arg_dict, nodes);
  }
  if (arg_dict.contains(kWindowKey)) {
    return ResolveWindowReferenceInPlace(*client_, expected_frame_id,
                                         context_id, object_group_name, timeout,
                                         arg_dict, nodes);
  }
  if (arg_dict.contains(kFrameKey)) {
    return ResolveFrameReferenceInPlace(*client_, expected_frame_id, context_id,
                                        object_group_name, timeout, arg_dict,
                                        nodes);
  }

  Status status{kOk};
  for (auto it = arg_dict.begin(); status.IsOk() && it != arg_dict.end();
       ++it) {
    status = ResolveElementReferencesInPlace(
        expected_frame_id, context_id, object_group_name, expected_loader_id,
        timeout, it->second, nodes);
  }
  return status;
}

Status WebViewImpl::ResolveElementReferencesInPlace(
    const std::string& expected_frame_id,
    const std::string& context_id,
    const std::string& object_group_name,
    const std::string& expected_loader_id,
    const Timeout& timeout,
    base::Value::List& arg_list,
    base::Value::List& nodes) {
  Status status{kOk};
  for (auto it = arg_list.begin(); status.IsOk() && it != arg_list.end();
       ++it) {
    status = ResolveElementReferencesInPlace(
        expected_frame_id, context_id, object_group_name, expected_loader_id,
        timeout, *it, nodes);
  }
  return status;
}

Status WebViewImpl::ResolveElementReferencesInPlace(
    const std::string& expected_frame_id,
    const std::string& context_id,
    const std::string& object_group_name,
    const std::string& expected_loader_id,
    const Timeout& timeout,
    base::Value& arg,
    base::Value::List& nodes) {
  if (arg.is_list()) {
    return ResolveElementReferencesInPlace(
        expected_frame_id, context_id, object_group_name, expected_loader_id,
        timeout, arg.GetList(), nodes);
  }
  if (arg.is_dict()) {
    return ResolveElementReferencesInPlace(
        expected_frame_id, context_id, object_group_name, expected_loader_id,
        timeout, arg.GetDict(), nodes);
  }
  return Status{kOk};
}

Status WebViewImpl::CreateElementReferences(const std::string& frame_id,
                                            const base::Value::List& nodes,
                                            bool include_shadow_root,
                                            base::Value& res) {
  Status status{kOk};
  if (res.is_list()) {
    base::Value::List& list = res.GetList();
    for (base::Value& elem : list) {
      status =
          CreateElementReferences(frame_id, nodes, include_shadow_root, elem);
      if (status.IsError()) {
        return status;
      }
    }
    return status;
  }

  if (!res.is_dict()) {
    return status;
  }

  base::Value::Dict& dict = res.GetDict();
  std::optional<std::string> maybe_key = GetElementIdKey(dict, w3c_compliant_);

  if (!maybe_key) {
    for (auto p : dict) {
      status = CreateElementReferences(frame_id, nodes, include_shadow_root,
                                       p.second);
      if (status.IsError()) {
        return status;
      }
    }
    return status;
  }

  std::optional<int> maybe_node_idx = dict.FindInt(*maybe_key);
  if (!maybe_node_idx) {
    return Status{kUnknownError, "node index is missing"};
  }
  if (*maybe_node_idx < 0 ||
      static_cast<size_t>(*maybe_node_idx) >= nodes.size()) {
    return Status{kUnknownError, "node index is out of range"};
  }
  if (!nodes[*maybe_node_idx].is_dict()) {
    return Status{kUnknownError, "serialized node is not a dictionary"};
  }
  const base::Value::Dict& node = nodes[*maybe_node_idx].GetDict();

  if (*maybe_key == kWindowKey) {
    const std::string* context = node.FindStringByDottedPath("value.context");
    if (!context) {
      return Status{kUnknownError, "context is missing in a node"};
    }
    dict.Remove(*maybe_key);
    if (*context == client_->GetId() ||
        GetTargetForFrame(*context) == nullptr) {
      // The reference either points to the top level window/tab or to some
      // dynamically created window.
      dict.Set(kWindowKey, *context);
    } else {
      dict.Set(kFrameKey, *context);
    }
    return status;
  }

  std::optional<int> maybe_backend_node_id =
      node.FindIntByDottedPath("value.backendNodeId");
  if (!maybe_backend_node_id) {
    return Status{kUnknownError, "backendNodeId is missing in the node"};
  }
  const std::string* maybe_loader_id =
      node.FindStringByDottedPath("value.loaderId");
  if (maybe_loader_id == nullptr) {
    return Status{kUnknownError, "loaderId is missing in the node"};
  }

  std::string shared_id =
      base::StringPrintf("f.%s.d.%s.e.%d", frame_id.c_str(),
                         maybe_loader_id->c_str(), *maybe_backend_node_id);

  dict.Set(*maybe_key, std::move(shared_id));

  const base::Value::Dict* shadow_root =
      node.FindDictByDottedPath("value.shadowRoot");
  if (include_shadow_root && shadow_root) {
    maybe_backend_node_id =
        shadow_root->FindIntByDottedPath("value.backendNodeId");
    if (!maybe_backend_node_id) {
      return Status{kUnknownError,
                    "backendNodeId is missing in the node's shadowRoot"};
    }
    maybe_loader_id = shadow_root->FindStringByDottedPath("value.loaderId");
    if (maybe_loader_id == nullptr) {
      return Status{kUnknownError,
                    "loaderId is missing in the node's shadowRoot"};
    }
    shared_id =
        base::StringPrintf("f.%s.d.%s.e.%d", frame_id.c_str(),
                           maybe_loader_id->c_str(), *maybe_backend_node_id);
    base::Value::Dict shadow_root_dict;
    shadow_root_dict.Set(kShadowRootKey, std::move(shared_id));
    dict.Set("shadowRoot", std::move(shadow_root_dict));
  }

  return status;
}

bool WebViewImpl::IsDialogOpen() const {
  return client_->IsDialogOpen();
}

Status WebViewImpl::GetDialogMessage(std::string& message) const {
  return client_->GetDialogMessage(message);
}

Status WebViewImpl::GetTypeOfDialog(std::string& type) const {
  return client_->GetTypeOfDialog(type);
}

Status WebViewImpl::HandleDialog(bool accept,
                                 const std::optional<std::string>& text) {
  return client_->HandleDialog(accept, text);
}

WebView* WebViewImpl::FindContainerForFrame(const std::string& frame_id) {
  return GetTargetForFrame(frame_id);
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
    if (!web_view->IsDetached()) {
      web_view->Unlock();
    } else if (web_view->GetParent() != nullptr) {
      item.web_view = nullptr;
      web_view->GetParent()->GetFrameTracker()->DeleteTargetForFrame(
          web_view->GetId());
    }
  }
}

namespace internal {

Status EvaluateScript(DevToolsClient* client,
                      const std::string& context_id,
                      const std::string& expression,
                      const base::TimeDelta& timeout,
                      const bool await_promise,
                      base::Value::Dict& result) {
  Status status{kOk};
  base::Value::Dict params;
  params.Set("expression", expression);
  if (!context_id.empty()) {
    params.Set("uniqueContextId", context_id);
  }
  params.Set("returnByValue", true);
  params.Set("awaitPromise", await_promise);
  base::Value::Dict cmd_result;

  Timeout local_timeout(timeout);
  status = client->SendCommandAndGetResultWithTimeout(
      "Runtime.evaluate", params, &local_timeout, &cmd_result);
  if (status.IsError())
    return status;

  if (cmd_result.contains("exceptionDetails")) {
    std::string description = "unknown";
    if (const std::string* maybe_description =
            cmd_result.FindStringByDottedPath("result.description")) {
      description = *maybe_description;
    }
    return Status(kUnknownError,
                  "Runtime.evaluate threw exception: " + description);
  }

  base::Value::Dict* unscoped_result = cmd_result.FindDict("result");
  if (!unscoped_result)
    return Status(kUnknownError, "evaluate missing dictionary 'result'");
  result = std::move(*unscoped_result);

  return status;
}

Status EvaluateScriptAndGetValue(DevToolsClient* client,
                                 const std::string& context_id,
                                 const std::string& expression,
                                 const base::TimeDelta& timeout,
                                 const bool await_promise,
                                 std::unique_ptr<base::Value>* result) {
  base::Value::Dict temp_result;
  Status status = EvaluateScript(client, context_id, expression, timeout,
                                 await_promise, temp_result);
  if (status.IsError())
    return status;

  std::string* type = temp_result.FindString("type");
  if (!type)
    return Status(kUnknownError, "Runtime.evaluate missing string 'type'");

  if (*type == "undefined") {
    *result = std::make_unique<base::Value>();
  } else {
    std::optional<base::Value> value = temp_result.Extract("value");
    if (!value)
      return Status(kUnknownError, "Runtime.evaluate missing 'value'");
    *result = base::Value::ToUniquePtrValue(std::move(*value));
  }
  return Status(kOk);
}

Status ParseCallFunctionResult(const base::Value& temp_result,
                               std::unique_ptr<base::Value>* result) {
  const base::Value::Dict* dict = temp_result.GetIfDict();
  if (!dict)
    return Status(kUnknownError, "call function result must be a dictionary");
  std::optional<int> status_code = dict->FindInt("status");
  if (!status_code) {
    return Status(kUnknownError,
                  "call function result missing int 'status'");
  }
  if (*status_code != kOk) {
    const std::string* message = dict->FindString("value");
    return Status(static_cast<StatusCode>(*status_code),
                  message ? *message : "");
  }
  const base::Value* unscoped_value = dict->Find("value");
  if (unscoped_value == nullptr) {
    // Missing 'value' indicates the JavaScript code didn't return a value.
    return Status(kOk);
  }
  *result = base::Value::ToUniquePtrValue(unscoped_value->Clone());
  return Status(kOk);
}

}  // namespace internal
