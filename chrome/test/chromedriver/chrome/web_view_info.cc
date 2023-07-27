// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/web_view_info.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/log.h"
#include "chrome/test/chromedriver/chrome/status.h"

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

Status WebViewInfo::ParseType(const std::string& type_as_string,
                              WebViewInfo::Type* type) {
  if (type_as_string == "app") {
    *type = WebViewInfo::kApp;
  } else if (type_as_string == "background_page") {
    *type = WebViewInfo::kBackgroundPage;
  } else if (type_as_string == "page") {
    *type = WebViewInfo::kPage;
  } else if (type_as_string == "worker") {
    *type = WebViewInfo::kWorker;
  } else if (type_as_string == "webview") {
    *type = WebViewInfo::kWebView;
  } else if (type_as_string == "iframe") {
    *type = WebViewInfo::kIFrame;
  } else if (type_as_string == "other") {
    *type = WebViewInfo::kOther;
  } else if (type_as_string == "service_worker") {
    *type = WebViewInfo::kServiceWorker;
  } else if (type_as_string == "shared_worker") {
    *type = WebViewInfo::kSharedWorker;
  } else if (type_as_string == "external") {
    *type = WebViewInfo::kExternal;
  } else if (type_as_string == "browser") {
    *type = WebViewInfo::kBrowser;
  } else {
    return Status(kUnknownError,
                  "DevTools returned unknown type:" + type_as_string);
  }
  return Status(kOk);
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
  auto it = base::ranges::find(views_info, id, &WebViewInfo::id);
  if (it == views_info.end()) {
    return nullptr;
  }
  return &*it;
}

Status WebViewsInfo::FillFromTargetsInfo(
    const base::Value::List& target_infos) {
  std::vector<WebViewInfo> temp_views_info;
  for (const base::Value& info_value : target_infos) {
    if (!info_value.is_dict()) {
      return Status(kUnknownError, "DevTools contains non-dictionary item");
    }
    const base::Value::Dict& info = info_value.GetDict();
    const std::string* id = info.FindString("id");
    if (!id) {
      id = info.FindString("targetId");
    }
    if (!id) {
      return Status(kUnknownError,
                    "DevTools did not include 'id' or 'targetId'");
    }
    const std::string* type_as_string = info.FindString("type");
    if (!type_as_string) {
      return Status(kUnknownError, "DevTools did not include 'type'");
    }
    const std::string* url = info.FindString("url");
    if (!url) {
      return Status(kUnknownError, "DevTools did not include 'url'");
    }
    const std::string* debugger_url = info.FindString("webSocketDebuggerUrl");
    WebViewInfo::Type type;
    Status status = WebViewInfo::ParseType(*type_as_string, &type);
    if (status.IsError()) {
      return status;
    }
    temp_views_info.emplace_back(*id, debugger_url ? *debugger_url : "", *url,
                                 type);
  }
  views_info.swap(temp_views_info);
  return Status(kOk);
}

bool WebViewsInfo::ContainsTargetType(WebViewInfo::Type type) const {
  return base::ranges::any_of(
      views_info,
      [searched_type = type](WebViewInfo::Type current_type) {
        return searched_type == current_type;
      },
      &WebViewInfo::type);
}

const WebViewInfo* WebViewsInfo::FindFirst(WebViewInfo::Type type) const {
  auto it = base::ranges::find(views_info, type, &WebViewInfo::type);
  return it == views_info.end() ? nullptr : &(*it);
}
