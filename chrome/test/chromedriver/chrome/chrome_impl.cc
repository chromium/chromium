// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/chrome_impl.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/browser_info.h"
#include "chrome/test/chromedriver/chrome/chrome.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/devtools_client_impl.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"
#include "chrome/test/chromedriver/chrome/devtools_http_client.h"
#include "chrome/test/chromedriver/chrome/page_tracker.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/target_utils.h"
#include "chrome/test/chromedriver/chrome/web_view_impl.h"

namespace {
Status MakeFailedStatus(const std::string& desired_state,
                        const std::string& current_state) {
  return Status(kUnknownError, "failed to change window state to '" +
                                   desired_state + "', current state is '" +
                                   current_state + "'");
}

}  // namespace

namespace internal {

WindowBounds::WindowBounds() = default;

WindowBounds::~WindowBounds() = default;

base::Value::Dict WindowBounds::ToDict() const {
  base::Value::Dict dict;
  if (position.has_value()) {
    dict.Set("left", position->left);
    dict.Set("top", position->top);
  }
  if (size.has_value()) {
    dict.Set("width", size->width);
    dict.Set("height", size->height);
  }
  if (state.has_value()) {
    dict.Set("windowState", *state);
  }
  return dict;
}

bool WindowBounds::Matches(const Window& window) const {
  bool ok = true;
  if (position.has_value()) {
    ok = position->left == window.left && position->top == window.top;
  }
  if (ok && size.has_value()) {
    ok = size->width == window.width && size->height == window.height;
  }
  if (ok && state.has_value()) {
    ok = *state == window.state;
  }
  return ok;
}

}  // namespace internal

ChromeImpl::~ChromeImpl() = default;

Status ChromeImpl::GetAsDesktop(ChromeDesktopImpl** desktop) {
  return Status(kUnknownError, "operation unsupported");
}

const BrowserInfo* ChromeImpl::GetBrowserInfo() const {
  return &browser_info_;
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
  Status status = target_utils::GetWebViewsInfo(*devtools_websocket_client_,
                                                nullptr, views_info);
  if (status.IsError())
    return status;
  do {
    status = UpdateWebViews(views_info, w3c_compliant);
  } while (status.code() == kTargetDetached);
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

Status ChromeImpl::GetWebViewCount(size_t* web_view_count, bool w3c_compliant) {
  WebViewsInfo views_info;
  Status status = target_utils::GetWebViewsInfo(*devtools_websocket_client_,
                                                nullptr, views_info);
  if (status.IsError()) {
    return status;
  }

  *web_view_count = views_info.GetSize();

  return Status(kOk);
}

Status ChromeImpl::GetWebViewIds(std::list<std::string>* web_view_ids,
                                 bool w3c_compliant) {
  WebViewsInfo views_info;
  Status status = target_utils::GetWebViewsInfo(*devtools_websocket_client_,
                                                nullptr, views_info);
  if (status.IsError()) {
    return status;
  }
  do {
    status = UpdateWebViews(views_info, w3c_compliant);
  } while (status.code() == kTargetDetached);
  if (status.IsError()) {
    return status;
  }
  std::list<std::string> web_view_ids_tmp;
  for (const auto& view : web_views_)
    web_view_ids_tmp.push_back(view->GetId());
  web_view_ids->swap(web_view_ids_tmp);
  return Status(kOk);
}

bool ChromeImpl::IsBrowserWindow(const WebViewInfo& view) const {
  return base::Contains(window_types_, view.type);
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
    if (IsBrowserWindow(view)) {
      bool found = false;
      for (const auto& web_view : web_views_) {
        if (web_view->GetId() == view.id) {
          found = true;
          break;
        }
      }
      if (!found) {
        std::unique_ptr<DevToolsClient> client;
        Status status = target_utils::AttachToPageTarget(
            *devtools_websocket_client_, view.id, nullptr, client);
        // This web view may have closed itself between when it was returned by
        // `Target.getTargets` and when `chromedriver` attempted to attach to
        // it. In that case, ignore this web view. See crbug.com/1506833 for an
        // example of this race.
        if (status.code() == kNoSuchWindow) {
          continue;
        } else if (status.IsError()) {
          return status;
        }

        for (const auto& listener : devtools_event_listeners_)
          client->AddListener(listener.get());
        // OnConnected will fire when DevToolsClient connects later.
        CHECK(!page_load_strategy_.empty());
        if (view.type == WebViewInfo::kServiceWorker) {
          web_views_.push_back(WebViewImpl::CreateServiceWorkerWebView(
              view.id, w3c_compliant, &browser_info_, std::move(client)));
        } else {
          web_views_.push_back(WebViewImpl::CreateTopLevelWebView(
              view.id, w3c_compliant, &browser_info_, std::move(client),
              mobile_device_, page_load_strategy_, autoaccept_beforeunload_));
        }
        status = web_views_.back()->AttachTo(devtools_websocket_client_.get());
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
                             bool is_background,
                             std::string* window_handle) {
  internal::Window window;
  Status status = GetWindow(target_id, window);
  if (status.IsError())
    return Status(kNoSuchWindow);

  base::Value::Dict params;
  params.Set("url", "about:blank");
  params.Set("newWindow", type == WindowType::kWindow);
  params.Set("background", is_background);
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

Status ChromeImpl::GetWindow(const std::string& target_id,
                             internal::Window& window) {
  base::Value::Dict params;
  params.Set("targetId", target_id);
  base::Value::Dict result;
  Status status = devtools_websocket_client_->SendCommandAndGetResult(
      "Browser.getWindowForTarget", params, &result);
  if (status.IsError())
    return status;

  return ParseWindow(std::move(result), window);
}

Status ChromeImpl::GetWindowRect(const std::string& target_id,
                                 WindowRect* rect) {
  internal::Window window;
  Status status = GetWindow(target_id, window);
  if (status.IsError())
    return status;

  rect->x = window.left;
  rect->y = window.top;
  rect->width = window.width;
  rect->height = window.height;
  return Status(kOk);
}

Status ChromeImpl::MaximizeWindow(const std::string& target_id) {
  internal::Window window;
  Status status = GetWindow(target_id, window);
  if (status.IsError())
    return status;

  if (window.state == "maximized")
    return Status(kOk);

  internal::WindowBounds bounds;
  bounds.state = "maximized";
  return SetWindowBounds(window, target_id, bounds);
}

Status ChromeImpl::MinimizeWindow(const std::string& target_id) {
  internal::Window window;
  Status status = GetWindow(target_id, window);
  if (status.IsError())
    return status;

  if (window.state == "minimized")
    return Status(kOk);

  internal::WindowBounds bounds;
  bounds.state = "minimized";
  return SetWindowBounds(window, target_id, bounds);
}

Status ChromeImpl::FullScreenWindow(const std::string& target_id) {
  internal::Window window;
  Status status = GetWindow(target_id, window);
  if (status.IsError())
    return status;

  if (window.state == "fullscreen")
    return Status(kOk);

  internal::WindowBounds bounds;
  bounds.state = "fullscreen";
  return SetWindowBounds(window, target_id, bounds);
}

Status ChromeImpl::SetWindowRect(const std::string& target_id,
                                 const base::Value::Dict& params) {
  internal::Window window;
  Status status = GetWindow(target_id, window);
  if (status.IsError())
    return status;

  // window position
  internal::WindowBounds bounds;
  std::optional<int> x = params.FindInt("x");
  std::optional<int> y = params.FindInt("y");
  if (x.has_value() && y.has_value()) {
    bounds.position = {*x, *y};
  }
  // window size
  std::optional<int> width = params.FindInt("width");
  std::optional<int> height = params.FindInt("height");
  if (width.has_value() && height.has_value()) {
    bounds.size = {*width, *height};
  }

  return SetWindowBounds(window, target_id, bounds);
}

Status ChromeImpl::GetWindowBounds(int window_id, internal::Window& window) {
  base::Value::Dict params;
  params.Set("windowId", window_id);
  base::Value::Dict result;
  Status status = devtools_websocket_client_->SendCommandAndGetResult(
      "Browser.getWindowBounds", params, &result);
  if (status.IsError())
    return status;

  return ParseWindowBounds(std::move(result), window);
}

Status ChromeImpl::SetWindowBounds(
    internal::Window window,
    const std::string& target_id,
    const internal::WindowBounds& window_bounds) {
  Status status{kOk};
  base::Value::Dict params;
  params.Set("windowId", window.id);
  const std::string normal = "normal";
  if (window.state != normal) {
    params.SetByDottedPath("bounds.windowState", normal);
    status = devtools_websocket_client_->SendCommand("Browser.setWindowBounds",
                                                     params);
    // Return success in case of `Browser.setWindowBounds` not implemented like
    // on Android. crbug.com/1237183
    if (status.code() == kUnknownCommand)
      return Status(kOk);

    if (status.IsError())
      return status;

    status = GetWindowBounds(window.id, window);
    if (status.IsError())
      return status;

    Timeout timeout(base::Milliseconds(300));
    int waiting_time_ms = 10;
    while (window.state != normal) {
      // Waiting until the window state change animation is over.
      // This can take up to 5 iterations because 10 + 20 + 40 + 80 + 160 > 300
      base::PlatformThread::Sleep(base::Milliseconds(waiting_time_ms));
      waiting_time_ms *= 2;
      status = GetWindowBounds(window.id, window);
      if (status.IsError()) {
        return status;
      }
      // It can happen that under high load the timeout expires before the first
      // iteration. Therefore it is better to check its expiration in the loop
      // body instead of the loop condition.
      if (timeout.IsExpired()) {
        break;
      }
    }

    if (window.state != normal) {
      return MakeFailedStatus(normal, window.state);
    }
  }

  // The code above makes sure that the window is in the normal state
  DCHECK(window.state == normal);

  std::optional<std::string> desired_state = window_bounds.state;

  if (desired_state && *desired_state == "fullscreen" &&
      !GetBrowserInfo()->is_headless_shell) {
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

    status = GetWindowBounds(window.id, window);
    if (status.IsError())
      return status;

    if (window.state == *desired_state) {
      return Status(kOk);
    }
    return MakeFailedStatus(*desired_state, window.state);
  }

  // The code transitioning to "fullscreen" always returns. If this point is
  // reached then the "fullscreen" block was not executed and therefore the
  // state must remain the same as before the "fullscreen" block.
  DCHECK(window.state == normal);

  // crbug.com/946023. When setWindowBounds is run before requestFullscreen,
  // we sometimes see a devtools crash. Because the latter call will
  // set fullscreen, do not call setWindowBounds with a fullscreen request
  // unless running headless. see https://crbug.com/1049336
  params.Set("bounds", window_bounds.ToDict());
  status = devtools_websocket_client_->SendCommand("Browser.setWindowBounds",
                                                   params);

  // From this point the assertion window->state == normal does not have to hold

  // Return success in case of `Browser.setWindowBounds` not implemented like
  // on Android. crbug.com/1237183
  if (status.code() == kUnknownCommand)
    return Status(kOk);

  if (status.IsError())
    return status;

  Timeout timeout(base::Milliseconds(300));
  int waiting_time_ms = 10;
  while (!window_bounds.Matches(window)) {
    // Waiting until the window state change animation is over.
    // This can take up to 5 iterations because 10 + 20 + 40 + 80 + 160 > 300
    base::PlatformThread::Sleep(base::Milliseconds(waiting_time_ms));
    waiting_time_ms *= 2;
    status = GetWindowBounds(window.id, window);
    if (status.IsError()) {
      return status;
    }
    // It can happen that under high load the timeout expires before the first
    // iteration. Therefore it is better to check its expiration in the loop
    // body instead of the loop condition.
    if (timeout.IsExpired()) {
      break;
    }
  }

  if (!desired_state || desired_state->empty())
    return Status(kOk);

  status = GetWindowBounds(window.id, window);
  if (status.IsError())
    return status;

  if (window.state == *desired_state) {
    return Status(kOk);
  }

  if (*desired_state == "maximized" && window.state == "normal") {
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
    const std::optional<int> width = result->GetDict().FindInt("width");
    const std::optional<int> height = result->GetDict().FindInt("height");
    if (!width || !height) {
      return Status(kUnknownError, "unexpected JavaScript result");
    }
    base::Value::Dict bounds;
    bounds.Set("width", width.value());
    bounds.Set("height", height.value());
    bounds.Set("left", 0);
    bounds.Set("top", 0);
    params.Set("bounds", std::move(bounds));
    return devtools_websocket_client_->SendCommand("Browser.setWindowBounds",
                                                   params);
  }

  int retries = 0;
  // Wait and retry for 1 second
  for (; retries < 10; ++retries) {
    // SetWindowBounds again for retry
    params.Set("bounds", window_bounds.ToDict());
    status = devtools_websocket_client_->SendCommand("Browser.setWindowBounds",
                                                     params);

    base::PlatformThread::Sleep(base::Milliseconds(100));

    status = GetWindowBounds(window.id, window);
    if (status.IsError())
      return status;
    if (window.state == *desired_state) {
      return Status(kOk);
    }
  }

  return MakeFailedStatus(*desired_state, window.state);
}

Status ChromeImpl::ParseWindow(const base::Value::Dict& params,
                               internal::Window& window) {
  std::optional<int> id = params.FindInt("windowId");
  if (!id)
    return Status(kUnknownError, "no window id in response");
  window.id = *id;

  return ParseWindowBounds(std::move(params), window);
}

Status ChromeImpl::ParseWindowBounds(const base::Value::Dict& params,
                                     internal::Window& window) {
  const base::Value::Dict* value = params.FindDict("bounds");
  if (!value) {
    return Status(kUnknownError, "no window bounds in response");
  }

  const std::string* state = value->FindString("windowState");
  if (!state)
    return Status(kUnknownError, "no window state in window bounds");
  window.state = *state;

  std::optional<int> left = value->FindInt("left");
  if (!left)
    return Status(kUnknownError, "no left offset in window bounds");
  window.left = *left;

  std::optional<int> top = value->FindInt("top");
  if (!top)
    return Status(kUnknownError, "no top offset in window bounds");
  window.top = *top;

  std::optional<int> width = value->FindInt("width");
  if (!width)
    return Status(kUnknownError, "no width in window bounds");
  window.width = *width;

  std::optional<int> height = value->FindInt("height");
  if (!height)
    return Status(kUnknownError, "no height in window bounds");
  window.height = *height;

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
  Timeout timeout(base::Seconds(20));
  while (!timeout.IsExpired()) {
    WebViewsInfo views_info;
    status = target_utils::GetWebViewsInfo(*devtools_websocket_client_,
                                           &timeout, views_info);
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

ChromeImpl::ChromeImpl(BrowserInfo browser_info,
                       std::set<WebViewInfo::Type> window_types,
                       std::unique_ptr<DevToolsClient> websocket_client,
                       std::vector<std::unique_ptr<DevToolsEventListener>>
                           devtools_event_listeners,
                       std::optional<MobileDevice> mobile_device,
                       std::string page_load_strategy,
                       bool autoaccept_beforeunload)
    : mobile_device_(std::move(mobile_device)),
      browser_info_(std::move(browser_info)),
      window_types_(std::move(window_types)),
      devtools_websocket_client_(std::move(websocket_client)),
      autoaccept_beforeunload_(autoaccept_beforeunload),
      devtools_event_listeners_(std::move(devtools_event_listeners)),
      page_load_strategy_(page_load_strategy) {
  window_types_.insert(WebViewInfo::kPage);
  window_types_.insert(WebViewInfo::kApp);
  page_tracker_ = std::make_unique<PageTracker>(
      devtools_websocket_client_.get(), &web_views_);
}
