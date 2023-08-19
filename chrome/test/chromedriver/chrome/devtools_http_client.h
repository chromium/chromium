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
#include "chrome/test/chromedriver/chrome/web_view_info.h"

namespace base {
class TimeDelta;
}

namespace network::mojom {
class URLLoaderFactory;
}  // namespace network::mojom

class Status;

class DevToolsHttpClient {
 public:
  DevToolsHttpClient(const DevToolsEndpoint& endpoint,
                     network::mojom::URLLoaderFactory* factory);

  DevToolsHttpClient(const DevToolsHttpClient&) = delete;
  DevToolsHttpClient& operator=(const DevToolsHttpClient&) = delete;

  virtual ~DevToolsHttpClient();

  Status Init(const base::TimeDelta& timeout);

  Status GetWebViewsInfo(WebViewsInfo* views_info);

  const BrowserInfo* browser_info();

  static Status ParseWebViewsInfo(const std::string& data,
                                  WebViewsInfo& views_info);

 private:
  virtual bool FetchUrlAndLog(const std::string& url, std::string* response);

  raw_ptr<network::mojom::URLLoaderFactory> url_loader_factory_;
  DevToolsEndpoint endpoint_;
  BrowserInfo browser_info_;
  std::unique_ptr<std::set<WebViewInfo::Type>> window_types_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_DEVTOOLS_HTTP_CLIENT_H_
