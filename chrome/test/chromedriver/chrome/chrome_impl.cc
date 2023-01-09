// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/chrome_impl.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/chrome.h"
#include "chrome/test/chromedriver/chrome/device_metrics.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/devtools_client_impl.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"
#include "chrome/test/chromedriver/chrome/devtools_http_client.h"
#include "chrome/test/chromedriver/chrome/page_load_strategy.h"
#include "chrome/test/chromedriver/chrome/page_tracker.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/web_view_impl.h"
#include "url/gurl.h"

namespace {
Status MakeFailedStatus(const std::string& desired_state,
                        const std::string& current_state) {
  return Status(kUnknownError, "failed to change window state to '" +
                                   desired_state + "', current state is '" +
                                   current_state + "'");
}
}  // namespace

ChromeImpl::~ChromeImpl() {
}

Status ChromeImpl::GetAsDesktop(ChromeDesktopImpl** desktop) {
  return Status(kUnknownError, "operation unsupported");
}

const BrowserInfo* ChromeImpl::GetBrowserInfo() const {
  return devtools_http_client_->browser_info();
}

bool ChromeImpl::HasCrashedWebView() {
  for (const auto& view : web_views_) {
    if (view->WasCrashed())
      return true;
  }
  return false;
}

Status ChromeImpl::GetWebViewIdForFirstTab(std::string* web_view_id,
                                           bool w3c_compliant) {
  WebViewsInfo views_info;
  Status status = GetWebViewsInfo(&views_info);
  if (status.IsError())
    return status;
  status = UpdateWebViews(views_info, w3c_compliant);
  if (status.IsError())
    return status;
  for (int i = views_info.GetSize() - 1; i >= 0; --i) {
    const WebViewInfo& view = views_info.Get(i);
    if (view.type == WebViewInfo::kPage) {
      *web_view_id = view.id;
      return Status(kOk);
    }
  }
  return Status(kUnknownError, "unable to discover open window in chrome");
}

Status ChromeImpl::GetWebViewIds(std::list<std::string>* web_view_ids,
                                 bool w3c_compliant) {
  WebViewsInfo views_info;
  Status status = GetWebViewsInfo(&views_info);
  if (status.IsError())
    return status;
  status = UpdateWebViews(views_info, w3c_compliant);
  if (status.IsError())
    return status;
  std::list<std::string> web_view_ids_tmp;
  for (const auto& view : web_views_)
    web_view_ids_tmp.push_back(view->GetId());
  web_view_ids->swap(web_view_ids_tmp);
  return Status(kOk);
}

Status ChromeImpl::UpdateWebViews(const WebViewsInfo& views_info,
                                  bool w3c_compliant) {
  // Check if some web views are closed (or in the case of background pages,
  // become inactive).
  auto it = web_views_.begin();
  while (it != web_views_.end()) {
    const WebViewInfo* view = views_info.GetForId((*it)->GetId());
    if (!view) {
      it = web_views_.erase(it);
    } else {
      ++it;
    }
  }

  // Check for newly-opened web views.
  for (size_t i = 0; i < views_info.GetSize(); ++i) {
    const WebViewInfo& view = views_info.Get(i);
    if (devtools_http_client_->IsBrowserWindow(view)) {
      bool found = false;
      for (const auto& web_view : web_views_) {
        if (web_view->GetId() == view.id) {
          found = true;
          break;
        }
      }
      if (!found) {
        std::unique_ptr<DevToolsClientImpl> client;
        Status status = CreateClient(view.id, &client);
        if (status.IsError()) {
          return status;
        }

        for (const auto& listener : devtools_event_listeners_)
          client->AddListener(listener.get());
        // OnConnected will fire when DevToolsClient connects later.
        CHECK(!page_load_strategy_.empty());
        if (view.type == WebViewInfo::kServiceWorker) {
          web_views_.push_back(std::make_unique<WebViewImpl>(
              view.id, w3c_compliant, nullptr,
              devtools_http_client_->browser_info(), std::move(client)));
        } else {
          web_views_.push_back(std::make_unique<WebViewImpl>(
              view.id, w3c_compliant, nullptr,
              devtools_http_client_->browser_info(), std::move(client),
              device_metrics_.get(), page_load_strategy_));
        }
        DevToolsClientImpl* parent =
            static_cast<DevToolsClientImpl*>(devtools_websocket_client_.get());
        status = web_views_.back()->AttachTo(parent);
        if (status.IsError()) {
          return status;
        }
      }
    }
  }

  return Status(kOk);
}

Status ChromeImpl::GetWebViewById(const std::string& id, WebView** web_view) {
  for (const auto& view : web_views_) {
    if (view->GetId() == id) {
      *web_view = view.get();
      return Status(kOk);
    }
  }
  return Status(kUnknownError, "web view not found");
}

Status ChromeImpl::NewWindow(const std::string& target_id,
                             WindowType type,
                             std::string* window_handle) {
  Window window;
  Status status = GetWindow(target_id, &window);
  if (status.IsError())
    return Status(kNoSuchWindow);

  base::Value::Dict params;
  params.Set("url", "about:blank");
  params.Set("newWindow", type == WindowType::kWindow);
  params.Set("background", true);
  base::Value::Dict result;
  status = devtools_websocket_client_->SendCommandAndGetResult(
      "Target.createTarget", params, &result);
  if (status.IsError())
    return status;

  const std::string* target_id_str = result.FindString("targetId");
  if (!target_id_str)
    return Status(kUnknownError, "no targetId from createTarget");
  *window_handle = *target_id_str;

  return Status(kOk);
}

Status ChromeImpl::CreateClient(const std::string& id,
                                std::unique_ptr<DevToolsClientImpl>* client) {
  Status status{kOk};
  std::string session_id;
  {
    base::Value::Dict params;
    base::Value::Dict result;
    params.Set("targetId", id);
    params.Set("flatten", true);
    status = devtools_websocket_client_->SendCommandAndGetResult(
        "Target.attachToTarget", params, &result);
    if (status.IsError()) {
      return status;
    }

    std::string* session_id_ptr = result.FindString("sessionId");

    if (session_id_ptr == nullptr) {
      return Status(kUnknownError,
                    "No sessionId in the response to Target.attachToTarget");
    }

    session_id = *session_id_ptr;
  }

  *client = std::make_unique<DevToolsClientImpl>(id, session_id);
  (*client)->SetFrontendCloserFunc(base::BindRepeating(
      &ChromeImpl::CloseFrontends, base::Unretained(this), id));
  (*client)->SetMainPage(true);

  return status;
}

Status ChromeImpl::CloseFrontends(const std::string& for_client_id) {
  WebViewsInfo views_info;
  Status status = GetWebViewsInfo(&views_info);
  if (status.IsError())
    return status;

  // Close frontends. Usually frontends are docked in the same page, although
  // some may be in tabs (undocked, chrome://inspect, the DevTools
  // discovery page, etc.). Tabs can be closed via the DevTools HTTP close
  // URL, but docked frontends can only be closed, by design, by connecting
  // to them and clicking the close button. Close the tab frontends first
  // in case one of them is debugging a docked frontend, which would prevent
  // the code from being able to connect to the docked one.
  std::list<std::string> tab_frontend_ids;
  std::list<std::string> docked_frontend_ids;
  for (size_t i = 0; i < views_info.GetSize(); ++i) {
    const WebViewInfo& view_info = views_info.Get(i);
    if (view_info.IsFrontend()) {
      if (view_info.type == WebViewInfo::kPage)
        tab_frontend_ids.push_back(view_info.id);
      else if (view_info.type == WebViewInfo::kOther)
        docked_frontend_ids.push_back(view_info.id);
      else
        return Status(kUnknownError, "unknown type of DevTools frontend");
    }
  }

  for (std::list<std::string>::const_iterator it = tab_frontend_ids.begin();
       it != tab_frontend_ids.end(); ++it) {
    status = CloseWebView(*it);
    if (status.IsError())
      return status;
  }

  for (std::list<std::string>::const_iterator it = docked_frontend_ids.begin();
       it != docked_frontend_ids.end(); ++it) {
    std::unique_ptr<DevToolsClientImpl> client;
    status = CreateClient(*it, &client);
    if (status.IsError())
      return status;
    std::unique_ptr<WebViewImpl> web_view(new WebViewImpl(
        *it, false, nullptr, devtools_http_client_->browser_info(),
        std::move(client), nullptr, page_load_strategy_));

    DevToolsClientImpl* parent =
        static_cast<DevToolsClientImpl*>(devtools_websocket_client_.get());
    status = web_view->AttachTo(parent);
    if (status.IsError()) {
      return status;
    }

    status = CloseWebView(*it);
    // Ignore disconnected error, because it may be closed already.
    if (status.IsError() && status.code() != kDisconnected)
      return status;
  }

  status = GetWebViewsInfo(&views_info);
  if (status.IsError())
    return status;

  const WebViewInfo* view_info = views_info.GetForId(for_client_id);
  if (!view_info)
    return Status(kNoSuchWindow, "window was already closed");
  return Status(kOk);
}

Status ChromeImpl::GetWindow(const std::string& target_id, Window* window) {
  base::Value::Dict params;
  params.Set("targetId", target_id);
  base::Value::Dict result;
  Status status = devtools_websocket_client_->SendCommandAndGetResult(
      "Browser.getWindowForTarget", params, &result);
  if (status.IsError())
    return status;

  return ParseWindow(base::Value(std::move(result)), window);
}

Status ChromeImpl::GetWindowRect(const std::string& target_id,
                                 WindowRect* rect) {
  Window window;
  Status status = GetWindow(target_id, &window);
  if (status.IsError())
    return status;

  rect->x = window.left;
  rect->y = window.top;
  rect->width = window.width;
  rect->height = window.height;
  return Status(kOk);
}

Status ChromeImpl::MaximizeWindow(const std::string& target_id) {
  Window window;
  Status status = GetWindow(target_id, &window);
  if (status.IsError())
    return status;

  if (window.state == "maximized")
    return Status(kOk);

  auto bounds = std::make_unique<base::Value::Dict>();
  bounds->Set("windowState", "maximized");
  return SetWindowBounds(&window, target_id, std::move(bounds));
}

Status ChromeImpl::MinimizeWindow(const std::string& target_id) {
  Window window;
  Status status = GetWindow(target_id, &window);
  if (status.IsError())
    return status;

  if (window.state == "minimized")
    return Status(kOk);

  auto bounds = std::make_unique<base::Value::Dict>();
  bounds->Set("windowState", "minimized");
  return SetWindowBounds(&window, target_id, std::move(bounds));
}

Status ChromeImpl::FullScreenWindow(const std::string& target_id) {
  Window window;
  Status status = GetWindow(target_id, &window);
  if (status.IsError())
    return status;

  if (window.state == "fullscreen")
    return Status(kOk);

  auto bounds = std::make_unique<base::Value::Dict>();
  bounds->Set("windowState", "fullscreen");
  return SetWindowBounds(&window, target_id, std::move(bounds));
}

Status ChromeImpl::SetWindowRect(const std::string& target_id,
                                 const base::Value::Dict& params) {
  Window window;
  Status status = GetWindow(target_id, &window);
  if (status.IsError())
    return status;

  auto bounds = std::make_unique<base::Value::Dict>();

  // window position
  absl::optional<int> x = params.FindInt("x");
  absl::optional<int> y = params.FindInt("y");
  if (x.has_value() && y.has_value()) {
    bounds->Set("left", *x);
    bounds->Set("top", *y);
  }
  // window size
  absl::optional<int> width = params.FindInt("width");
  absl::optional<int> height = params.FindInt("height");
  if (width.has_value() && height.has_value()) {
    bounds->Set("width", *width);
    bounds->Set("height", *height);
  }

  return SetWindowBounds(&window, target_id, std::move(bounds));
}

Status ChromeImpl::GetWindowBounds(int window_id, Window* window) {
  base::Value::Dict params;
  params.Set("windowId", window_id);
  base::Value::Dict result;
  Status status = devtools_websocket_client_->SendCommandAndGetResult(
      "Browser.getWindowBounds", params, &result);
  if (status.IsError())
    return status;

  return ParseWindowBounds(base::Value(std::move(result)), window);
}

Status ChromeImpl::SetWindowBounds(Window* window,
                                   const std::string& target_id,
                                   std::unique_ptr<base::Value::Dict> bounds) {
  Status status{kOk};
  base::Value::Dict params;
  params.Set("windowId", window->id);
  const std::string normal = "normal";
  if (window->state != normal) {
    params.SetByDottedPath("bounds.windowState", normal);
    status = devtools_websocket_client_->SendCommand("Browser.setWindowBounds",
                                                     params);
    // Return success in case of `Browser.setWindowBounds` not implemented like
    // on Android. crbug.com/1237183
    if (status.code() == kUnknownCommand)
      return Status(kOk);

    if (status.IsError())
      return status;
    base::PlatformThread::Sleep(base::Milliseconds(100));

    status = GetWindowBounds(window->id, window);
    if (status.IsError())
      return status;

    if (window->state != normal)
      return MakeFailedStatus(normal, window->state);
  }

  const std::string* desired_state = bounds->FindString("windowState");

  if (desired_state && *desired_state == "fullscreen" &&
      !GetBrowserInfo()->is_headless) {
    // Work around crbug.com/982071. This block of code is necessary to ensure
    // that document.webkitIsFullScreen and document.fullscreenElement return
    // the correct values.
    // But do not run when headless. see https://crbug.com/1049336
    WebView* web_view;
    status = GetWebViewById(target_id, &web_view);
    if (status.IsError())
      return status;

    base::Value::Dict fullscreen_params;
    fullscreen_params.Set("expression",
                          "document.documentElement.requestFullscreen()");
    fullscreen_params.Set("userGesture", true);
    fullscreen_params.Set("awaitPromise", true);
    status = web_view->SendCommand("Runtime.evaluate", fullscreen_params);
    if (status.IsError())
      return status;

    status = GetWindowBounds(window->id, window);
    if (status.IsError())
      return status;

    if (window->state == *desired_state)
      return Status(kOk);
    return MakeFailedStatus(*desired_state, window->state);
  }

  // crbug.com/946023. When setWindowBounds is run before requestFullscreen,
  // we sometimes see a devtools crash. Because the latter call will
  // set fullscreen, do not call setWindowBounds with a fullscreen request
  // unless running headless. see https://crbug.com/1049336
  params.Set("bounds", bounds->Clone());
  status = devtools_websocket_client_->SendCommand("Browser.setWindowBounds",
                                                   params);

  // Return success in case of `Browser.setWindowBounds` not implemented like
  // on Android. crbug.com/1237183
  if (status.code() == kUnknownCommand)
    return Status(kOk);

  if (status.IsError())
    return status;

  base::PlatformThread::Sleep(base::Milliseconds(100));

  if (!desired_state || desired_state->empty())
    return Status(kOk);

  status = GetWindowBounds(window->id, window);
  if (status.IsError())
    return status;

  if (window->state == *desired_state)
    return Status(kOk);

  if (*desired_state == "maximized" && window->state == "normal") {
    // Maximize window is not supported in some environment, such as Mac Chrome
    // version 70 and above, or Linux without a window manager.
    // In these cases, we simulate window maximization by setting window size
    // to equal to screen size. This is accordance with the W3C spec at
    // https://www.w3.org/TR/webdriver1/#dfn-maximize-the-window.
    // Get a WebView, then use it to send JavaScript to query screen size.
    WebView* web_view;
    status = GetWebViewById(target_id, &web_view);
    if (status.IsError())
      return status;
    std::unique_ptr<base::Value> result;
    status = web_view->EvaluateScript(
        std::string(),
        "({width: screen.availWidth, height: screen.availHeight})", false,
        &result);
    if (status.IsError())
      return Status(kUnknownError, "JavaScript code failed", status);
    const base::Value* width =
        result->FindKeyOfType("width", base::Value::Type::INTEGER);
    const base::Value* height =
        result->FindKeyOfType("height", base::Value::Type::INTEGER);
    if (width == nullptr || height == nullptr)
      return Status(kUnknownError, "unexpected JavaScript result");
    auto window_bounds = std::make_unique<base::Value::Dict>();
    window_bounds->Set("width", width->GetInt());
    window_bounds->Set("height", height->GetInt());
    window_bounds->Set("left", 0);
    window_bounds->Set("top", 0);
    params.Set("bounds", window_bounds->Clone());
    return devtools_websocket_client_->SendCommand("Browser.setWindowBounds",
                                                   params);
  }

  int retries = 0;
  // Wait and retry for 1 second
  for (; retries < 10; ++retries) {
    // SetWindowBounds again for retry
    params.Set("bounds", bounds->Clone());
    status = devtools_websocket_client_->SendCommand("Browser.setWindowBounds",
                                                     params);

    base::PlatformThread::Sleep(base::Milliseconds(100));

    status = GetWindowBounds(window->id, window);
    if (status.IsError())
      return status;
    if (window->state == *desired_state)
      return Status(kOk);
  }

  return MakeFailedStatus(*desired_state, window->state);
}

Status ChromeImpl::GetWebViewsInfo(WebViewsInfo* views_info) {
  DCHECK(views_info);
  Status status{kOk};

  base::Value::Dict params;
  base::Value::Dict result;
  status = devtools_websocket_client_->SendCommandAndGetResult(
      "Target.getTargets", params, &result);
  if (status.IsError()) {
    return status;
  }
  base::Value* target_infos = result.Find("targetInfos");
  if (!target_infos) {
    return Status(
        kUnknownError,
        "result of call to Target.getTargets does not contain targetInfos");
  }
  if (!target_infos->is_list()) {
    return Status(kUnknownError,
                  "targetInfos in Target.getTargets response is not a list");
  }
  std::vector<WebViewInfo> temp_views_info;
  for (const base::Value& info_value : target_infos->GetList()) {
    if (!info_value.is_dict())
      return Status(kUnknownError, "DevTools contains non-dictionary item");
    const base::Value::Dict* info = info_value.GetIfDict();
    DCHECK(info);
    const std::string* id = info->FindString("targetId");
    if (!id)
      return Status(kUnknownError, "DevTools did not include id");
    const std::string* type_as_string = info->FindString("type");
    if (!type_as_string)
      return Status(kUnknownError, "DevTools did not include type");
    const std::string* url = info->FindString("url");
    if (!url)
      return Status(kUnknownError, "DevTools did not include url");
    WebViewInfo::Type type;
    Status parse_status = ParseType(*type_as_string, &type);
    if (parse_status.IsError())
      return parse_status;
    temp_views_info.emplace_back(*id, std::string(), *url, type);
  }
  *views_info = WebViewsInfo(temp_views_info);
  return status;
}

Status ChromeImpl::ParseWindow(const base::Value& params, Window* window) {
  absl::optional<int> id = params.FindIntKey("windowId");
  if (!id)
    return Status(kUnknownError, "no window id in response");
  window->id = *id;

  return ParseWindowBounds(std::move(params), window);
}

Status ChromeImpl::ParseWindowBounds(const base::Value& params,
                                     Window* window) {
  const base::Value* value = params.FindKey("bounds");
  if (!value || !value->is_dict())
    return Status(kUnknownError, "no window bounds in response");

  const std::string* state = value->FindStringKey("windowState");
  if (!state)
    return Status(kUnknownError, "no window state in window bounds");
  window->state = *state;

  absl::optional<int> left = value->FindIntKey("left");
  if (!left)
    return Status(kUnknownError, "no left offset in window bounds");
  window->left = *left;

  absl::optional<int> top = value->FindIntKey("top");
  if (!top)
    return Status(kUnknownError, "no top offset in window bounds");
  window->top = *top;

  absl::optional<int> width = value->FindIntKey("width");
  if (!width)
    return Status(kUnknownError, "no width in window bounds");
  window->width = *width;

  absl::optional<int> height = value->FindIntKey("height");
  if (!height)
    return Status(kUnknownError, "no height in window bounds");
  window->height = *height;

  return Status(kOk);
}

Status ChromeImpl::CloseTarget(const std::string& id) {
  base::Value::Dict params;
  params.Set("targetId", id);
  Status status =
      devtools_websocket_client_->SendCommand("Target.closeTarget", params);

  if (status.IsError())
    return status;

  // Wait for the target window to be completely closed.
  base::TimeTicks deadline = base::TimeTicks::Now() + base::Seconds(20);
  while (base::TimeTicks::Now() < deadline) {
    WebViewsInfo views_info;
    status = GetWebViewsInfo(&views_info);
    if (status.code() == kChromeNotReachable)
      return Status(kOk);
    if (status.code() == kDisconnected)  // The closed target has gone
      return Status(kOk);
    if (status.IsError())
      return status;
    if (!views_info.GetForId(id))
      return Status(kOk);
    base::PlatformThread::Sleep(base::Milliseconds(50));
  }
  return Status(kUnknownError, "failed to close window in 20 seconds");
}

Status ChromeImpl::CloseWebView(const std::string& id) {
  Status status = CloseTarget(id);
  if (status.IsError()) {
    return status;
  }

  auto it = base::ranges::find(web_views_, id, &WebViewImpl::GetId);
  if (it != web_views_.end()) {
    web_views_.erase(it);
  }

  return Status(kOk);
}

Status ChromeImpl::ActivateWebView(const std::string& id) {
  WebView* webview = nullptr;
  GetWebViewById(id, &webview);
  if (webview && webview->IsServiceWorker())
    return Status(kOk);

  base::Value::Dict params;
  params.Set("targetId", id);
  Status status =
      devtools_websocket_client_->SendCommand("Target.activateTarget", params);
  return status;
}

Status ChromeImpl::SetAcceptInsecureCerts() {
  base::Value::Dict params;
  params.Set("ignore", true);
  return devtools_websocket_client_->SendCommand(
      "Security.setIgnoreCertificateErrors", params);
}

Status ChromeImpl::SetPermission(
    std::unique_ptr<base::Value::Dict> permission_descriptor,
    PermissionState desired_state,
    WebView* current_view) {
  // Process URL.
  std::string current_url;
  Status status = current_view->GetUrl(&current_url);
  if (status.IsError())
    current_url = "";

  std::string permission_setting;
  if (desired_state == PermissionState::kGranted)
    permission_setting = "granted";
  else if (desired_state == PermissionState::kDenied)
    permission_setting = "denied";
  else if (desired_state == PermissionState::kPrompt)
    permission_setting = "prompt";
  else
    return Status(kInvalidArgument, "unsupported PermissionState");

  base::Value::Dict args;
  args.Set("origin", current_url);
  args.Set("permission", std::move(*permission_descriptor));
  args.Set("setting", permission_setting);
  return devtools_websocket_client_->SendCommand("Browser.setPermission", args);
}

bool ChromeImpl::IsMobileEmulationEnabled() const {
  return false;
}

bool ChromeImpl::HasTouchScreen() const {
  return false;
}

std::string ChromeImpl::page_load_strategy() const {
  CHECK(!page_load_strategy_.empty());
  return page_load_strategy_;
}

Status ChromeImpl::Quit() {
  Status status = QuitImpl();
  if (status.IsOk())
    quit_ = true;
  return status;
}

DevToolsClient* ChromeImpl::Client() const {
  return devtools_websocket_client_.get();
}

ChromeImpl::ChromeImpl(std::unique_ptr<DevToolsHttpClient> http_client,
                       std::unique_ptr<DevToolsClient> websocket_client,
                       std::vector<std::unique_ptr<DevToolsEventListener>>
                           devtools_event_listeners,
                       std::unique_ptr<DeviceMetrics> device_metrics,
                       SyncWebSocketFactory socket_factory,
                       std::string page_load_strategy)
    : device_metrics_(std::move(device_metrics)),
      socket_factory_(std::move(socket_factory)),
      devtools_http_client_(std::move(http_client)),
      devtools_websocket_client_(std::move(websocket_client)),
      devtools_event_listeners_(std::move(devtools_event_listeners)),
      page_load_strategy_(page_load_strategy) {
  page_tracker_ = std::make_unique<PageTracker>(
      devtools_websocket_client_.get(), &web_views_);
}
