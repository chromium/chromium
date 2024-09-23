// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_WEB_VIEW_INFO_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_WEB_VIEW_INFO_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/values.h"

class Status;

struct WebViewInfo {
  enum Type {
    kApp,
    kBackgroundPage,
    kBrowser,
    kExternal,
    kIFrame,
    kOther,
    kPage,
    kServiceWorker,
    kSharedWorker,
    kWebView,
    kWorker,
  };

  WebViewInfo(const std::string& id,
              const std::string& debugger_url,
              const std::string& url,
              Type type);
  WebViewInfo(const WebViewInfo& other);
  ~WebViewInfo();

  bool IsFrontend() const;
  bool IsInactiveBackgroundPage() const;

  static Status ParseType(const std::string& data, WebViewInfo::Type& type);

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
  Status FillFromTargetsInfo(const base::Value::List& target_infos);
  bool ContainsTargetType(WebViewInfo::Type type) const;
  const WebViewInfo* FindFirst(WebViewInfo::Type type) const;

 private:
  std::vector<WebViewInfo> views_info;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_WEB_VIEW_INFO_H_
