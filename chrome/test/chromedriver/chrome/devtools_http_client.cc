// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/devtools_http_client.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/device_metrics.h"
#include "chrome/test/chromedriver/chrome/devtools_client_impl.h"
#include "chrome/test/chromedriver/chrome/log.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/web_view_impl.h"
#include "chrome/test/chromedriver/net/net_util.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

WebViewInfo::WebViewInfo(const std::string& id,
                         const std::string& debugger_url,
                         const std::string& url,
                         Type type)
    : id(id), debugger_url(debugger_url), url(url), type(type) {}

WebViewInfo::WebViewInfo(const WebViewInfo& other) = default;

WebViewInfo::~WebViewInfo() {}

bool WebViewInfo::IsFrontend() const {
  return base::StartsWith(url, "devtools://", base::CompareCase::SENSITIVE);
}

bool WebViewInfo::IsInactiveBackgroundPage() const {
  return type == WebViewInfo::kBackgroundPage && debugger_url.empty();
}

WebViewsInfo::WebViewsInfo() {}

WebViewsInfo::WebViewsInfo(const std::vector<WebViewInfo>& info)
    : views_info(info) {}

WebViewsInfo::~WebViewsInfo() {}

const WebViewInfo& WebViewsInfo::Get(int index) const {
  return views_info[index];
}

size_t WebViewsInfo::GetSize() const {
  return views_info.size();
}

const WebViewInfo* WebViewsInfo::GetForId(const std::string& id) const {
  for (size_t i = 0; i < views_info.size(); ++i) {
    if (views_info[i].id == id)
      return &views_info[i];
  }
  return NULL;
}

DevToolsHttpClient::DevToolsHttpClient(
    const DevToolsEndpoint& endpoint,
    network::mojom::URLLoaderFactory* factory,
    const SyncWebSocketFactory& socket_factory,
    std::unique_ptr<DeviceMetrics> device_metrics,
    std::unique_ptr<std::set<WebViewInfo::Type>> window_types,
    std::string page_load_strategy)
    : url_loader_factory_(factory),
      socket_factory_(socket_factory),
      endpoint_(endpoint),
      device_metrics_(std::move(device_metrics)),
      window_types_(std::move(window_types)),
      page_load_strategy_(page_load_strategy) {
  window_types_->insert(WebViewInfo::kPage);
  window_types_->insert(WebViewInfo::kApp);
}

DevToolsHttpClient::~DevToolsHttpClient() {}

Status DevToolsHttpClient::Init(const base::TimeDelta& timeout) {
  if (!endpoint_.IsValid()) {
    return Status(kChromeNotReachable);
  }

  browser_info_.debugger_endpoint = endpoint_;

  base::TimeTicks deadline = base::TimeTicks::Now() + timeout;
  std::string version_url = endpoint_.GetVersionUrl();
  std::string data;

  while (!FetchUrlAndLog(version_url, &data) || data.empty()) {
    if (base::TimeTicks::Now() > deadline)
      return Status(kChromeNotReachable);
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(50));
  }

  return ParseBrowserInfo(data, &browser_info_);
}

Status DevToolsHttpClient::GetWebViewsInfo(WebViewsInfo* views_info) {
  std::string data;
  if (!FetchUrlAndLog(endpoint_.GetListUrl(), &data))
    return Status(kChromeNotReachable);

  return internal::ParseWebViewsInfo(data, views_info);
}

std::unique_ptr<DevToolsClient> DevToolsHttpClient::CreateClient(
    const std::string& id) {
  return std::unique_ptr<DevToolsClient>(new DevToolsClientImpl(
      socket_factory_, endpoint_.GetDebuggerUrl(id), id,
      base::BindRepeating(&DevToolsHttpClient::CloseFrontends,
                          base::Unretained(this), id)));
}

Status DevToolsHttpClient::CloseWebView(const std::string& id) {
  std::string data;
  if (!FetchUrlAndLog(endpoint_.GetCloseUrl(id), &data)) {
    return Status(kOk);  // Closing the last web view leads chrome to quit.
  }

  // Wait for the target window to be completely closed.
  base::TimeTicks deadline =
      base::TimeTicks::Now() + base::TimeDelta::FromSeconds(20);
  while (base::TimeTicks::Now() < deadline) {
    WebViewsInfo views_info;
    Status status = GetWebViewsInfo(&views_info);
    if (status.code() == kChromeNotReachable)
      return Status(kOk);
    if (status.IsError())
      return status;
    if (!views_info.GetForId(id))
      return Status(kOk);
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(50));
  }
  return Status(kUnknownError, "failed to close window in 20 seconds");
}

Status DevToolsHttpClient::ActivateWebView(const std::string& id) {
  std::string data;
  if (!FetchUrlAndLog(endpoint_.GetActivateUrl(id), &data))
    return Status(kUnknownError, "cannot activate web view");
  return Status(kOk);
}

const BrowserInfo* DevToolsHttpClient::browser_info() {
  return &browser_info_;
}

const DeviceMetrics* DevToolsHttpClient::device_metrics() {
  return device_metrics_.get();
}

bool DevToolsHttpClient::IsBrowserWindow(const WebViewInfo& view) const {
  return base::Contains(*window_types_, view.type) ||
         (view.type == WebViewInfo::kOther && view.url == "chrome://print/");
}

Status DevToolsHttpClient::CloseFrontends(const std::string& for_client_id) {
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
    std::unique_ptr<DevToolsClient> client(new DevToolsClientImpl(
        socket_factory_, endpoint_.GetDebuggerUrl(*it), *it));
    std::unique_ptr<WebViewImpl> web_view(
        new WebViewImpl(*it, false, nullptr, &browser_info_, std::move(client),
                        nullptr, page_load_strategy_));

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
  base::TimeTicks deadline =
      base::TimeTicks::Now() + base::TimeDelta::FromSeconds(20);
  while (base::TimeTicks::Now() < deadline) {
    status = GetWebViewsInfo(&views_info);
    if (status.IsError())
      return status;

    const WebViewInfo* view_info = views_info.GetForId(for_client_id);
    if (!view_info)
      return Status(kNoSuchWindow, "window was already closed");
    if (view_info->debugger_url.size())
      return Status(kOk);

    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(50));
  }
  return Status(kUnknownError, "failed to close UI debuggers");
}

bool DevToolsHttpClient::FetchUrlAndLog(const std::string& url,
                                        std::string* response) {
  VLOG(1) << "DevTools HTTP Request: " << url;
  bool ok = FetchUrl(url, url_loader_factory_, response);
  if (ok) {
    VLOG(1) << "DevTools HTTP Response: " << *response;
  } else {
    VLOG(1) << "DevTools HTTP Request failed";
  }
  return ok;
}

Status ParseType(const std::string& type_as_string, WebViewInfo::Type* type) {
  if (type_as_string == "app")
    *type = WebViewInfo::kApp;
  else if (type_as_string == "background_page")
    *type = WebViewInfo::kBackgroundPage;
  else if (type_as_string == "page")
    *type = WebViewInfo::kPage;
  else if (type_as_string == "worker")
    *type = WebViewInfo::kWorker;
  else if (type_as_string == "webview")
    *type = WebViewInfo::kWebView;
  else if (type_as_string == "iframe")
    *type = WebViewInfo::kIFrame;
  else if (type_as_string == "other")
    *type = WebViewInfo::kOther;
  else if (type_as_string == "service_worker")
    *type = WebViewInfo::kServiceWorker;
  else if (type_as_string == "shared_worker")
    *type = WebViewInfo::kSharedWorker;
  else if (type_as_string == "external")
    *type = WebViewInfo::kExternal;
  else if (type_as_string == "browser")
    *type = WebViewInfo::kBrowser;
  else
    return Status(kUnknownError,
                  "DevTools returned unknown type:" + type_as_string);
  return Status(kOk);
}

namespace internal {

Status ParseWebViewsInfo(const std::string& data, WebViewsInfo* views_info) {
  std::unique_ptr<base::Value> value = base::JSONReader::ReadDeprecated(data);
  if (!value.get())
    return Status(kUnknownError, "DevTools returned invalid JSON");
  base::ListValue* list;
  if (!value->GetAsList(&list))
    return Status(kUnknownError, "DevTools did not return list");

  std::vector<WebViewInfo> temp_views_info;
  for (size_t i = 0; i < list->GetSize(); ++i) {
    base::DictionaryValue* info;
    if (!list->GetDictionary(i, &info))
      return Status(kUnknownError, "DevTools contains non-dictionary item");
    std::string id;
    if (!info->GetString("id", &id))
      return Status(kUnknownError, "DevTools did not include id");
    std::string type_as_string;
    if (!info->GetString("type", &type_as_string))
      return Status(kUnknownError, "DevTools did not include type");
    std::string url;
    if (!info->GetString("url", &url))
      return Status(kUnknownError, "DevTools did not include url");
    std::string debugger_url;
    info->GetString("webSocketDebuggerUrl", &debugger_url);
    WebViewInfo::Type type;
    Status status = ParseType(type_as_string, &type);
    if (status.IsError())
      return status;
    temp_views_info.push_back(WebViewInfo(id, debugger_url, url, type));
  }
  *views_info = WebViewsInfo(temp_views_info);
  return Status(kOk);
}

}  // namespace internal
