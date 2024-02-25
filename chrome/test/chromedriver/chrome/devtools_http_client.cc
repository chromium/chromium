// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/devtools_http_client.h"

#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chrome/test/chromedriver/chrome/devtools_client_impl.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/net/net_util.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

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

  return ParseWebViewsInfo(data, *views_info);
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

Status DevToolsHttpClient::ParseWebViewsInfo(const std::string& data,
                                             WebViewsInfo& views_info) {
  std::optional<base::Value> value = base::JSONReader::Read(data);
  if (!value) {
    return Status(kUnknownError, "DevTools returned invalid JSON");
  }
  if (!value->is_list()) {
    return Status(kUnknownError, "DevTools did not return list");
  }

  return views_info.FillFromTargetsInfo(value->GetList());
}
