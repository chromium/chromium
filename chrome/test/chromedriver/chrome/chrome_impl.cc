// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/chrome_impl.h"

#include <stddef.h>
#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
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
  Status status = devtools_http_client_->GetWebViewsInfo(&views_info);
  if (status.IsError())
    return status;
  UpdateWebViews(views_info, w3c_compliant);
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
  Status status = devtools_http_client_->GetWebViewsInfo(&views_info);
  if (status.IsError())
    return status;
  UpdateWebViews(views_info, w3c_compliant);
  std::list<std::string> web_view_ids_tmp;
  for (const auto& view : web_views_)
    web_view_ids_tmp.push_back(view->GetId());
  web_view_ids->swap(web_view_ids_tmp);
  return Status(kOk);
}

void ChromeImpl::UpdateWebViews(const WebViewsInfo& views_info,
                                bool w3c_compliant) {
  // Check if some web views are closed (or in the case of background pages,
  // become inactive).
  auto it = web_views_.begin();
  while (it != web_views_.end()) {
    const WebViewInfo* view = views_info.GetForId((*it)->GetId());
    if (!view || view->IsInactiveBackgroundPage()) {
      it = web_views_.erase(it);
    } else {
      ++it;
    }
  }

  // Check for newly-opened web views.
  for (size_t i = 0; i < views_info.GetSize(); ++i) {
    const WebViewInfo& view = views_info.Get(i);
    if (devtools_http_client_->IsBrowserWindow(view) &&
        !view.IsInactiveBackgroundPage()) {
      bool found = false;
      for (const auto& web_view : web_views_) {
        if (web_view->GetId() == view.id) {
          found = true;
          break;
        }
      }
      if (!found) {
        std::unique_ptr<DevToolsClient> client = CreateClient(view.id);
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
      }
    }
  }
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
  Status status = devtools_websocket_client_->ConnectIfNecessary();
  if (status.IsError())
    return status;

  Window window;
  status = GetWindow(target_id, &window);
  if (status.IsError())
    return Status(kNoSuchWindow);

  base::DictionaryValue params;
  params.SetStringKey("url", "about:blank");
  params.SetBoolKey("newWindow", type == WindowType::kWindow);
  params.SetBoolKey("background", true);
  base::Value result;
  status = devtools_websocket_client_->SendCommandAndGetResult(
      "Target.createTarget", params, &result);
  if (status.IsError())
    return status;

  const std::string* target_id_str = result.FindStringKey("targetId");
  if (!target_id_str)
    return Status(kUnknownError, "no targetId from createTarget");
  *window_handle = *target_id_str;

  return Status(kOk);
}

std::unique_ptr<DevToolsClient> ChromeImpl::CreateClient(
    const std::string& id) {
  auto result = std::make_unique<DevToolsClientImpl>(
      id, "", devtools_http_client_->endpoint().GetDebuggerUrl(id),
      socket_factory_);
  result->SetFrontendCloserFunc(base::BindRepeating(
      &ChromeImpl::CloseFrontends, base::Unretained(this), id));
  return result;
}

Status ChromeImpl::CloseFrontends(const std::string& for_client_id) {
  WebViewsInfo views_info;
  Status status = devtools_http_client_->GetWebViewsInfo(&views_info);
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
    std::unique_ptr<DevToolsClient> client(new DevToolsClientImpl(
        *it, "", devtools_http_client_->endpoint().GetDebuggerUrl(*it),
        socket_factory_));
    std::unique_ptr<WebViewImpl> web_view(new WebViewImpl(
        *it, false, nullptr, devtools_http_client_->browser_info(),
        std::move(client), nullptr, page_load_strategy_));

    status = web_view->ConnectIfNecessary();
    // Ignore disconnected error, because the debugger might have closed when
    // its container page was closed above.
    if (status.IsError() && status.code() != kDisconnected)
      return status;

    status = CloseWebView(*it);
    // Ignore disconnected error, because it may be closed already.
    if (status.IsError() && status.code() != kDisconnected)
      return status;
  }

  // Wait until DevTools UI disconnects from the given web view.
  base::TimeTicks deadline = base::TimeTicks::Now() + base::Seconds(20);
  while (base::TimeTicks::Now() < deadline) {
    status = devtools_http_client_->GetWebViewsInfo(&views_info);
    if (status.IsError())
      return status;

    const WebViewInfo* view_info = views_info.GetForId(for_client_id);
    if (!view_info)
      return Status(kNoSuchWindow, "window was already closed");
    if (view_info->debugger_url.size())
      return Status(kOk);

    base::PlatformThread::Sleep(base::Milliseconds(50));
  }
  return Status(kUnknownError, "failed to close UI debuggers");
}

Status ChromeImpl::GetWindow(const std::string& target_id, Window* window) {
  Status status = devtools_websocket_client_->ConnectIfNecessary();
  if (status.IsError())
    return status;

  base::DictionaryValue params;
  params.SetStringKey("targetId", target_id);
  base::Value result;
  status = devtools_websocket_client_->SendCommandAndGetResult(
      "Browser.getWindowForTarget", params, &result);
  if (status.IsError())
    return status;

  return ParseWindow(result, window);
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

  auto bounds = std::make_unique<base::DictionaryValue>();
  bounds->SetStringKey("windowState", "maximized");
  return SetWindowBounds(&window, target_id, std::move(bounds));
}

Status ChromeImpl::MinimizeWindow(const std::string& target_id) {
  Window window;
  Status status = GetWindow(target_id, &window);
  if (status.IsError())
    return status;

  if (window.state == "minimized")
    return Status(kOk);

  auto bounds = std::make_unique<base::DictionaryValue>();
  bounds->SetStringKey("windowState", "minimized");
  return SetWindowBounds(&window, target_id, std::move(bounds));
}

Status ChromeImpl::FullScreenWindow(const std::string& target_id) {
  Window window;
  Status status = GetWindow(target_id, &window);
  if (status.IsError())
    return status;

  if (window.state == "fullscreen")
    return Status(kOk);

  auto bounds = std::make_unique<base::DictionaryValue>();
  bounds->SetStringKey("windowState", "fullscreen");
  return SetWindowBounds(&window, target_id, std::move(bounds));
}

Status ChromeImpl::SetWindowRect(const std::string& target_id,
                                        const base::DictionaryValue& params) {
  Window window;
  Status status = GetWindow(target_id, &window);
  if (status.IsError())
    return status;

  auto bounds = std::make_unique<base::DictionaryValue>();

  // window position
  absl::optional<int> x = params.FindIntKey("x");
  absl::optional<int> y = params.FindIntKey("y");
  if (x.has_value() && y.has_value()) {
    bounds->SetIntKey("left", *x);
    bounds->SetIntKey("top", *y);
  }
  // window size
  absl::optional<int> width = params.FindIntKey("width");
  absl::optional<int> height = params.FindIntKey("height");
  if (width.has_value() && height.has_value()) {
    bounds->SetIntKey("width", *width);
    bounds->SetIntKey("height", *height);
  }

  return SetWindowBounds(&window, target_id, std::move(bounds));
}

Status ChromeImpl::GetWindowBounds(int window_id, Window* window) {
  Status status = devtools_websocket_client_->ConnectIfNecessary();
  if (status.IsError())
    return status;

  base::DictionaryValue params;
  params.SetIntKey("windowId", window_id);
  base::Value result;
  status = devtools_websocket_client_->SendCommandAndGetResult(
      "Browser.getWindowBounds", params, &result);
  if (status.IsError())
    return status;

  return ParseWindowBounds(result, window);
}

Status ChromeImpl::SetWindowBounds(
    Window* window,
    const std::string& target_id,
    std::unique_ptr<base::DictionaryValue> bounds) {
  Status status = devtools_websocket_client_->ConnectIfNecessary();
  if (status.IsError())
    return status;

  base::DictionaryValue params;
  params.SetIntKey("windowId", window->id);
  const std::string normal = "normal";
  if (window->state != normal) {
    params.SetStringPath("bounds.windowState", normal);
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

  const std::string* desired_state = bounds->FindStringKey("windowState");

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

    base::DictionaryValue fullscreen_params;
    fullscreen_params.SetStringKey(
        "expression", "document.documentElement.requestFullscreen()");
    fullscreen_params.SetBoolKey("userGesture", true);
    fullscreen_params.SetBoolKey("awaitPromise", true);
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
  params.SetKey("bounds", bounds->Clone());
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
    auto window_bounds = std::make_unique<base::DictionaryValue>();
    window_bounds->SetIntKey("width", width->GetInt());
    window_bounds->SetIntKey("height", height->GetInt());
    window_bounds->SetIntKey("left", 0);
    window_bounds->SetIntKey("top", 0);
    params.SetKey("bounds", window_bounds->Clone());
    return devtools_websocket_client_->SendCommand("Browser.setWindowBounds",
                                                   params);
  }

  int retries = 0;
  // Wait and retry for 1 second
  for (; retries < 10; ++retries) {
    // SetWindowBounds again for retry
    params.SetKey("bounds", bounds->Clone());
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
  base::Value params{base::Value::Type::DICT};
  params.GetDict().Set("targetId", id);
  Status status = devtools_websocket_client_->SendCommand(
      "Target.closeTarget", base::Value::AsDictionaryValue(params));

  if (status.IsError())
    return status;

  // Wait for the target window to be completely closed.
  base::TimeTicks deadline = base::TimeTicks::Now() + base::Seconds(20);
  while (base::TimeTicks::Now() < deadline) {
    WebViewsInfo views_info;
    Status status = devtools_http_client_->GetWebViewsInfo(&views_info);
    if (status.code() == kChromeNotReachable)
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
  Status status = devtools_websocket_client_->ConnectIfNecessary();
  if (status.IsError()) {
    return status;
  }

  status = CloseTarget(id);
  if (status.IsError()) {
    return status;
  }

  auto it =
      std::find_if(web_views_.begin(), web_views_.end(),
                   [&id](const auto& view) { return view->GetId() == id; });
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

  Status status = devtools_websocket_client_->ConnectIfNecessary();
  if (status.IsError()) {
    return status;
  }

  base::Value params{base::Value::Type::DICT};
  params.GetDict().Set("targetId", id);
  status = devtools_websocket_client_->SendCommand(
      "Target.activateTarget", base::Value::AsDictionaryValue(params));
  return status;
}

Status ChromeImpl::SetAcceptInsecureCerts() {
  Status status = devtools_websocket_client_->ConnectIfNecessary();
  if (status.IsError())
    return status;

  base::DictionaryValue params;
  params.SetBoolKey("ignore", true);
  return devtools_websocket_client_->SendCommand(
      "Security.setIgnoreCertificateErrors", params);
}

Status ChromeImpl::SetPermission(
    std::unique_ptr<base::DictionaryValue> permission_descriptor,
    PermissionState desired_state,
    bool unused_one_realm,  // This is ignored. https://crbug.com/977612.
    WebView* current_view) {
  Status status = devtools_websocket_client_->ConnectIfNecessary();
  if (status.IsError())
    return status;

  // Process URL.
  std::string current_url;
  status = current_view->GetUrl(&current_url);
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

  base::DictionaryValue args;
  args.SetStringKey("origin", current_url);
  args.SetKey("permission", std::move(*permission_descriptor));
  args.SetStringKey("setting", permission_setting);
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
      page_load_strategy_(page_load_strategy) {}
