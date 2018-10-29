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

ChromeImpl::~ChromeImpl() {
}

Status ChromeImpl::GetAsDesktop(ChromeDesktopImpl** desktop) {
  return Status(kUnknownError, "operation unsupported");
}

const BrowserInfo* ChromeImpl::GetBrowserInfo() const {
  return devtools_http_client_->browser_info();
}

bool ChromeImpl::HasCrashedWebView() {
  for (auto it = web_views_.begin(); it != web_views_.end(); ++it) {
    if ((*it)->WasCrashed())
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
  for (WebViewList::const_iterator web_view_iter = web_views_.begin();
       web_view_iter != web_views_.end(); ++web_view_iter) {
    web_view_ids_tmp.push_back((*web_view_iter)->GetId());
  }
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
      for (WebViewList::const_iterator web_view_iter = web_views_.begin();
           web_view_iter != web_views_.end(); ++web_view_iter) {
        if ((*web_view_iter)->GetId() == view.id) {
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
        web_views_.push_back(make_linked_ptr(new WebViewImpl(
            view.id, w3c_compliant, devtools_http_client_->browser_info(),
            std::move(client), devtools_http_client_->device_metrics(),
            page_load_strategy_)));
      }
    }
  }
}

Status ChromeImpl::GetWebViewById(const std::string& id, WebView** web_view) {
  for (auto it = web_views_.begin(); it != web_views_.end(); ++it) {
    if ((*it)->GetId() == id) {
      *web_view = (*it).get();
      return Status(kOk);
    }
  }
  return Status(kUnknownError, "web view not found");
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

  if (window.state != "normal") {
    // restore window to normal first to allow position change.
    auto bounds = std::make_unique<base::DictionaryValue>();
    bounds->SetString("windowState", "normal");
    status = SetWindowBounds(window.id, std::move(bounds));
    if (status.IsError())
      return status;
  }

  auto bounds = std::make_unique<base::DictionaryValue>();
  bounds->SetInteger("left", x);
  bounds->SetInteger("top", y);
  return SetWindowBounds(window.id, std::move(bounds));
}

Status ChromeImpl::MaximizeWindow(const std::string& target_id) {
  Window window;
  Status status = GetWindow(target_id, &window);
  if (status.IsError())
    return status;

  if (window.state == "maximized")
    return Status(kOk);

  if (window.state != "normal") {
    // always restore window to normal first, since chrome ui doesn't allow
    // maximizing a minimized or fullscreen window.
    auto bounds = std::make_unique<base::DictionaryValue>();
    bounds->SetString("windowState", "normal");
    status = SetWindowBounds(window.id, std::move(bounds));
    if (status.IsError())
      return status;
  }

  auto bounds = std::make_unique<base::DictionaryValue>();
  bounds->SetString("windowState", "maximized");
  return SetWindowBounds(window.id, std::move(bounds));
}

Status ChromeImpl::MinimizeWindow(const std::string& target_id) {
  Window window;
  Status status = GetWindow(target_id, &window);
  if (status.IsError())
    return status;

  if (window.state == "minimized")
    return Status(kOk);

  if (window.state != "normal") {
    // restore window to normal first
    auto bounds = std::make_unique<base::DictionaryValue>();
    bounds->SetString("windowState", "normal");
    status = SetWindowBounds(window.id, std::move(bounds));
    if (status.IsError())
      return status;
  }

  auto bounds = std::make_unique<base::DictionaryValue>();
  bounds->SetString("windowState", "minimized");
  return SetWindowBounds(window.id, std::move(bounds));
}

Status ChromeImpl::FullScreenWindow(const std::string& target_id) {
  Window window;
  Status status = GetWindow(target_id, &window);
  if (status.IsError())
    return status;

  if (window.state == "fullscreen")
    return Status(kOk);

  if (window.state != "normal") {
    auto bounds = std::make_unique<base::DictionaryValue>();
    bounds->SetString("windowState", "normal");
    status = SetWindowBounds(window.id, std::move(bounds));
    if (status.IsError())
      return status;
  }

  auto bounds = std::make_unique<base::DictionaryValue>();
  bounds->SetString("windowState", "fullscreen");
  return SetWindowBounds(window.id, std::move(bounds));
}

Status ChromeImpl::SetWindowRect(const std::string& target_id,
                                        const base::DictionaryValue& params) {
  Window window;
  Status status = GetWindow(target_id, &window);
  if (status.IsError())
    return status;

  auto bounds = std::make_unique<base::DictionaryValue>();

  // fully exit fullscreen
  if (window.state != "normal") {
    auto bounds = std::make_unique<base::DictionaryValue>();
    bounds->SetString("windowState", "normal");
    status = SetWindowBounds(window.id, std::move(bounds));
    if (status.IsError())
      return status;
  }

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

  return SetWindowBounds(window.id, std::move(bounds));
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
    int window_id,
    std::unique_ptr<base::DictionaryValue> bounds) {
  Status status = devtools_websocket_client_->ConnectIfNecessary();
  if (status.IsError())
    return status;

  base::DictionaryValue params;
  params.SetInteger("windowId", window_id);
  params.Set("bounds", bounds->CreateDeepCopy());
  status = devtools_websocket_client_->SendCommand("Browser.setWindowBounds",
                                                   params);
  if (status.IsError())
    return status;

  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
  std::string state;
  if (!bounds->GetString("windowState", &state))
    return Status(kOk);

  Window window;
  status = GetWindowBounds(window_id, &window);
  if (status.IsError())
    return status;
  if (window.state != state)
    return Status(kUnknownError, "failed to change window state to " + state +
                                     ", current state is " + window.state);
  return Status(kOk);
}

Status ChromeImpl::SetWindowSize(const std::string& target_id,
                                        int width,
                                        int height) {
  Window window;

  Status status = GetWindow(target_id, &window);
  if (status.IsError())
    return status;

  if (window.state != "normal") {
    // restore window to normal first to allow size change.
    auto bounds = std::make_unique<base::DictionaryValue>();
    bounds->SetString("windowState", "normal");
    status = SetWindowBounds(window.id, std::move(bounds));
    if (status.IsError())
      return status;
  }

  auto bounds = std::make_unique<base::DictionaryValue>();
  bounds->SetInteger("width", width);
  bounds->SetInteger("height", height);
  return SetWindowBounds(window.id, std::move(bounds));
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
  // We ignore the status returned by this command - If it is an error, the
  // target likely doesn't yet support the command. In that case, we'll fall
  // back to --ignore-certificate-errors.
  // TODO(eseckler): Handle status once we remove support for
  // --ignore-certificate-errors.
  devtools_websocket_client_->SendCommand("Security.setIgnoreCertificateErrors",
                                          params);
  return Status(kOk);
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
