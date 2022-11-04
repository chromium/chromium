// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_DEVTOOLS_HTTP_CLIENT_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_DEVTOOLS_HTTP_CLIENT_H_

#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/test/chromedriver/chrome/browser_info.h"
#include "chrome/test/chromedriver/chrome/devtools_endpoint.h"

namespace base {
class TimeDelta;
}

namespace network {
namespace mojom {
class URLLoaderFactory;
}
}  // namespace network

class Status;

struct WebViewInfo {
  enum Type {
    kApp,
    kBackgroundPage,
    kPage,
    kWorker,
    kWebView,
    kIFrame,
    kOther,
    kServiceWorker,
    kSharedWorker,
    kExternal,
    kBrowser,
  };

  WebViewInfo(const std::string& id,
              const std::string& debugger_url,
              const std::string& url,
              Type type);
  WebViewInfo(const WebViewInfo& other);
  ~WebViewInfo();

  bool IsFrontend() const;
  bool IsInactiveBackgroundPage() const;

  std::string id;
  std::string debugger_url;
  std::string url;
  Type type;
};

class WebViewsInfo {
 public:
  WebViewsInfo();
  explicit WebViewsInfo(const std::vector<WebViewInfo>& info);
  ~WebViewsInfo();

  const WebViewInfo& Get(int index) const;
  size_t GetSize() const;
  const WebViewInfo* GetForId(const std::string& id) const;

 private:
  std::vector<WebViewInfo> views_info;
};

class DevToolsHttpClient {
 public:
  DevToolsHttpClient(const DevToolsEndpoint& endpoint,
                     network::mojom::URLLoaderFactory* factory,
                     std::unique_ptr<std::set<WebViewInfo::Type>> window_types);

  DevToolsHttpClient(const DevToolsHttpClient&) = delete;
  DevToolsHttpClient& operator=(const DevToolsHttpClient&) = delete;

  virtual ~DevToolsHttpClient();

  Status Init(const base::TimeDelta& timeout);

  Status GetWebViewsInfo(WebViewsInfo* views_info);

  const BrowserInfo* browser_info();
  bool IsBrowserWindow(const WebViewInfo& view) const;
  const DevToolsEndpoint& endpoint() const;

 private:
  virtual bool FetchUrlAndLog(const std::string& url, std::string* response);

  raw_ptr<network::mojom::URLLoaderFactory> url_loader_factory_;
  DevToolsEndpoint endpoint_;
  BrowserInfo browser_info_;
  std::unique_ptr<std::set<WebViewInfo::Type>> window_types_;
};

Status ParseType(const std::string& data, WebViewInfo::Type* type);

namespace internal {
Status ParseWebViewsInfo(const std::string& data, WebViewsInfo* views_info);
}  // namespace internal

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_DEVTOOLS_HTTP_CLIENT_H_
