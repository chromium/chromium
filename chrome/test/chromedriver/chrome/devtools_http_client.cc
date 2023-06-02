// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/devtools_http_client.h"

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_client_impl.h"
#include "chrome/test/chromedriver/chrome/log.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/net/net_util.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

WebViewInfo::WebViewInfo(const std::string& id,
                         const std::string& debugger_url,
                         const std::string& url,
                         Type type)
    : id(id), debugger_url(debugger_url), url(url), type(type) {}

WebViewInfo::WebViewInfo(const WebViewInfo& other) = default;

WebViewInfo::~WebViewInfo() = default;

bool WebViewInfo::IsFrontend() const {
  return base::StartsWith(url, "devtools://", base::CompareCase::SENSITIVE);
}

bool WebViewInfo::IsInactiveBackgroundPage() const {
  return type == WebViewInfo::kBackgroundPage && debugger_url.empty();
}

WebViewsInfo::WebViewsInfo() = default;

WebViewsInfo::WebViewsInfo(const std::vector<WebViewInfo>& info)
    : views_info(info) {}

WebViewsInfo::~WebViewsInfo() = default;

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
  return nullptr;
}

DevToolsHttpClient::DevToolsHttpClient(
    const DevToolsEndpoint& endpoint,
    network::mojom::URLLoaderFactory* factory)
    : url_loader_factory_(factory), endpoint_(endpoint) {}

DevToolsHttpClient::~DevToolsHttpClient() = default;

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
    base::PlatformThread::Sleep(base::Milliseconds(50));
  }

  return browser_info_.ParseBrowserInfo(data);
}

Status DevToolsHttpClient::GetWebViewsInfo(WebViewsInfo* views_info) {
  std::string data;
  if (!FetchUrlAndLog(endpoint_.GetListUrl(), &data))
    return Status(kChromeNotReachable);

  return internal::ParseWebViewsInfo(data, views_info);
}

const BrowserInfo* DevToolsHttpClient::browser_info() {
  return &browser_info_;
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
  absl::optional<base::Value> value = base::JSONReader::Read(data);
  if (!value) {
    return Status(kUnknownError, "DevTools returned invalid JSON");
  }
  if (!value->is_list())
    return Status(kUnknownError, "DevTools did not return list");

  std::vector<WebViewInfo> temp_views_info;
  for (const base::Value& info_value : value->GetList()) {
    if (!info_value.is_dict())
      return Status(kUnknownError, "DevTools contains non-dictionary item");
    const base::Value::Dict& info = info_value.GetDict();
    const std::string* id = info.FindString("id");
    if (!id)
      return Status(kUnknownError, "DevTools did not include id");
    const std::string* type_as_string = info.FindString("type");
    if (!type_as_string)
      return Status(kUnknownError, "DevTools did not include type");
    const std::string* url = info.FindString("url");
    if (!url)
      return Status(kUnknownError, "DevTools did not include url");
    const std::string* debugger_url = info.FindString("webSocketDebuggerUrl");
    WebViewInfo::Type type;
    Status status = ParseType(*type_as_string, &type);
    if (status.IsError())
      return status;
    temp_views_info.push_back(
        WebViewInfo(*id, debugger_url ? *debugger_url : "", *url, type));
  }
  *views_info = WebViewsInfo(temp_views_info);
  return Status(kOk);
}

}  // namespace internal
