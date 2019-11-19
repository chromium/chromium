// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/chrome_impl.h"

#include <stddef.h>
#include <utility>

#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"
#include "chrome/test/chromedriver/chrome/devtools_http_client.h"
#include "chrome/test/chromedriver/chrome/page_load_strategy.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/web_view_impl.h"
#include "url/gurl.h"

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
  for (size_t i = 0; i < views_info.GetSize(); ++i) {
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
        std::unique_ptr<DevToolsClient> client(
            devtools_http_client_->CreateClient(view.id));
        for (const auto& listener : devtools_event_listeners_)
          client->AddListener(listener.get());
        // OnConnected will fire when DevToolsClient connects later.
        CHECK(!page_load_strategy_.empty());
        web_views_.push_back(std::make_unique<WebViewImpl>(
            view.id, w3c_compliant, devtools_http_client_->browser_info(),
            std::move(client), devtools_http_client_->device_metrics(),
            page_load_strategy_));
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
  params.SetString("url", "about:blank");
  params.SetBoolean("newWindow", type == WindowType::kWindow);
  params.SetBoolean("background", true);
  std::unique_ptr<base::DictionaryValue> result;
  status = devtools_websocket_client_->SendCommandAndGetResult(
      "Target.createTarget", params, &result);
  if (status.IsError())
    return status;

  if (!result->GetString("targetId", window_handle))
    return Status(kUnknownError, "no targetId from createTarget");

  return Status(kOk);
}

Status ChromeImpl::GetWindow(const std::string& target_id, Window* window) {
  Status status = devtools_websocket_client_->ConnectIfNecessary();
  if (status.IsError())
    return status;

  base::DictionaryValue params;
  params.SetString("targetId", target_id);
  std::unique_ptr<base::DictionaryValue> result;
  status = devtools_websocket_client_->SendCommandAndGetResult(
      "Browser.getWindowForTarget", params, &result);
  if (status.IsError())
    return status;

  return ParseWindow(std::move(result), window);
}

Status ChromeImpl::GetWindowPosition(const std::string& target_id,
                                     int* x,
                                     int* y) {
  Window window;
  Status status = GetWindow(target_id, &window);
  if (status.IsError())
    return status;

  *x = window.left;
  *y = window.top;
  return Status(kOk);
}

Status ChromeImpl::SetWindowPosition(const std::string& target_id,
                                            int x,
                                            int y) {
  Window window;
  Status status = GetWindow(target_id, &window);
  if (status.IsError())
    return status;

  auto bounds = std::make_unique<base::DictionaryValue>();
  bounds->SetInteger("left", x);
  bounds->SetInteger("top", y);
  return SetWindowBounds(&window, std::move(bounds));
}

Status ChromeImpl::MaximizeWindow(const std::string& target_id) {
  Window window;
  Status status = GetWindow(target_id, &window);
  if (status.IsError())
    return status;

  if (window.state == "maximized")
    return Status(kOk);

  auto bounds = std::make_unique<base::DictionaryValue>();
  bounds->SetString("windowState", "maximized");
  return SetWindowBounds(&window, std::move(bounds));
}

Status ChromeImpl::MinimizeWindow(const std::string& target_id) {
  Window window;
  Status status = GetWindow(target_id, &window);
  if (status.IsError())
    return status;

  if (window.state == "minimized")
    return Status(kOk);

  auto bounds = std::make_unique<base::DictionaryValue>();
  bounds->SetString("windowState", "minimized");
  return SetWindowBounds(&window, std::move(bounds));
}

Status ChromeImpl::FullScreenWindow(const std::string& target_id) {
  Window window;
  Status status = GetWindow(target_id, &window);
  if (status.IsError())
    return status;

  if (window.state == "fullscreen")
    return Status(kOk);

  auto bounds = std::make_unique<base::DictionaryValue>();
  bounds->SetString("windowState", "fullscreen");
  return SetWindowBounds(&window, std::move(bounds));
}

Status ChromeImpl::SetWindowRect(const std::string& target_id,
                                        const base::DictionaryValue& params) {
  Window window;
  Status status = GetWindow(target_id, &window);
  if (status.IsError())
    return status;

  auto bounds = std::make_unique<base::DictionaryValue>();

  // window position
  int x = 0;
  int y = 0;
  if (params.GetInteger("x", &x) && params.GetInteger("y", &y)) {
    bounds->SetInteger("left", x);
    bounds->SetInteger("top", y);
  }
  // window size
  int width = 0;
  int height = 0;
  if (params.GetInteger("width", &width) &&
      params.GetInteger("height", &height)) {
    bounds->SetInteger("width", width);
    bounds->SetInteger("height", height);
  }

  return SetWindowBounds(&window, std::move(bounds));
}

Status ChromeImpl::GetWindowSize(const std::string& target_id,
                                 int* width,
                                 int* height) {
  Window window;
  Status status = GetWindow(target_id, &window);
  if (status.IsError())
    return status;

  *width = window.width;
  *height = window.height;
  return Status(kOk);
}

Status ChromeImpl::GetWindowBounds(int window_id, Window* window) {
  Status status = devtools_websocket_client_->ConnectIfNecessary();
  if (status.IsError())
    return status;

  base::DictionaryValue params;
  params.SetInteger("windowId", window_id);
  std::unique_ptr<base::DictionaryValue> result;
  status = devtools_websocket_client_->SendCommandAndGetResult(
      "Browser.getWindowBounds", params, &result);
  if (status.IsError())
    return status;

  return ParseWindowBounds(std::move(result), window);
}

Status ChromeImpl::SetWindowBounds(
    Window* window,
    std::unique_ptr<base::DictionaryValue> bounds) {
  Status status = devtools_websocket_client_->ConnectIfNecessary();
  if (status.IsError())
    return status;

  base::DictionaryValue params;
  params.SetInteger("windowId", window->id);
  if (window->state != "normal") {
    params.SetString("bounds.windowState", "normal");
    status = devtools_websocket_client_->SendCommand("Browser.setWindowBounds",
                                                     params);
    if (status.IsError())
      return status;
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
  }

  params.Set("bounds", bounds->CreateDeepCopy());
  status = devtools_websocket_client_->SendCommand("Browser.setWindowBounds",
                                                   params);
  if (status.IsError())
    return status;

  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
  std::string state;
  if (!bounds->GetString("windowState", &state))
    return Status(kOk);

  status = GetWindowBounds(window->id, window);
  if (status.IsError())
    return status;
  if (window->state == state)
    return Status(kOk);

  if (state == "maximized" && window->state == "normal") {
    // Maximize window is not supported in some environment, such as Mac Chrome
    // version 70 and above, or Linux without a window manager.
    // In these cases, we simulate window maximization by setting window size
    // to equal to screen size. This is accordance with the W3C spec at
    // https://www.w3.org/TR/webdriver1/#dfn-maximize-the-window.
    // Get a WebView, then use it to send JavaScript to query screen size.
    if (web_views_.size() == 0)
      return Status(kUnknownError, "no WebView");
    WebView* web_view = web_views_.begin()->get();
    std::unique_ptr<base::Value> result;
    status = web_view->EvaluateScript(
        std::string(),
        "({width: screen.availWidth, height: screen.availHeight})", &result);
    if (status.IsError())
      return Status(kUnknownError, "JavaScript code failed", status);
    const base::Value* width =
        result->FindKeyOfType("width", base::Value::Type::INTEGER);
    const base::Value* height =
        result->FindKeyOfType("height", base::Value::Type::INTEGER);
    if (width == nullptr || height == nullptr)
      return Status(kUnknownError, "unexpected JavaScript result");
    auto bounds = std::make_unique<base::DictionaryValue>();
    bounds->SetInteger("width", width->GetInt());
    bounds->SetInteger("height", height->GetInt());
    bounds->SetInteger("left", 0);
    bounds->SetInteger("top", 0);
    params.Set("bounds", bounds->CreateDeepCopy());
    return devtools_websocket_client_->SendCommand("Browser.setWindowBounds",
                                                   params);
  } else {
    return Status(kUnknownError, "failed to change window state to " + state +
                                     ", current state is " + window->state);
  }
}

Status ChromeImpl::SetWindowSize(const std::string& target_id,
                                        int width,
                                        int height) {
  Window window;

  Status status = GetWindow(target_id, &window);
  if (status.IsError())
    return status;

  auto bounds = std::make_unique<base::DictionaryValue>();
  bounds->SetInteger("width", width);
  bounds->SetInteger("height", height);
  return SetWindowBounds(&window, std::move(bounds));
}

Status ChromeImpl::ParseWindow(std::unique_ptr<base::DictionaryValue> params,
                               Window* window) {
  if (!params->GetInteger("windowId", &window->id))
    return Status(kUnknownError, "no window id in response");

  return ParseWindowBounds(std::move(params), window);
}

Status ChromeImpl::ParseWindowBounds(
    std::unique_ptr<base::DictionaryValue> params,
    Window* window) {
  const base::Value* value = nullptr;
  const base::DictionaryValue* bounds_dict = nullptr;
  if (!params->Get("bounds", &value) || !value->GetAsDictionary(&bounds_dict))
    return Status(kUnknownError, "no window bounds in response");

  if (!bounds_dict->GetString("windowState", &window->state))
    return Status(kUnknownError, "no window state in window bounds");

  if (!bounds_dict->GetInteger("left", &window->left))
    return Status(kUnknownError, "no left offset in window bounds");
  if (!bounds_dict->GetInteger("top", &window->top))
    return Status(kUnknownError, "no top offset in window bounds");
  if (!bounds_dict->GetInteger("width", &window->width))
    return Status(kUnknownError, "no width in window bounds");
  if (!bounds_dict->GetInteger("height", &window->height))
    return Status(kUnknownError, "no height in window bounds");

  return Status(kOk);
}

Status ChromeImpl::CloseWebView(const std::string& id) {
  Status status = devtools_http_client_->CloseWebView(id);
  if (status.IsError())
    return status;
  for (auto iter = web_views_.begin(); iter != web_views_.end(); ++iter) {
    if ((*iter)->GetId() == id) {
      web_views_.erase(iter);
      break;
    }
  }
  return Status(kOk);
}

Status ChromeImpl::ActivateWebView(const std::string& id) {
  return devtools_http_client_->ActivateWebView(id);
}

Status ChromeImpl::SetAcceptInsecureCerts() {
  Status status = devtools_websocket_client_->ConnectIfNecessary();
  if (status.IsError())
    return status;

  base::DictionaryValue params;
  params.SetBoolean("ignore", true);
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
  args.SetString("origin", current_url);
  args.SetDictionary("permission", std::move(permission_descriptor));
  args.SetString("setting", permission_setting);
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
                       std::string page_load_strategy)
    : quit_(false),
      devtools_http_client_(std::move(http_client)),
      devtools_websocket_client_(std::move(websocket_client)),
      devtools_event_listeners_(std::move(devtools_event_listeners)),
      page_load_strategy_(page_load_strategy) {}
