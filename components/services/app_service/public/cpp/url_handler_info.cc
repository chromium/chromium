// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "components/services/app_service/public/cpp/url_handler_info.h"

namespace apps {

UrlHandlerInfo::UrlHandlerInfo() = default;

UrlHandlerInfo::UrlHandlerInfo(const url::Origin& origin) : origin(origin) {}

UrlHandlerInfo::UrlHandlerInfo(const url::Origin& origin,
                               bool has_origin_wildcard)
    : origin(origin), has_origin_wildcard(has_origin_wildcard) {}

UrlHandlerInfo::UrlHandlerInfo(const url::Origin& origin,
                               bool has_origin_wildcard,
                               std::vector<std::string> paths,
                               std::vector<std::string> exclude_paths)
    : origin(origin),
      has_origin_wildcard(has_origin_wildcard),
      paths(std::move(paths)),
      exclude_paths(std::move(exclude_paths)) {}

UrlHandlerInfo::UrlHandlerInfo(const UrlHandlerInfo&) = default;

UrlHandlerInfo& UrlHandlerInfo::operator=(const UrlHandlerInfo&) = default;

UrlHandlerInfo::UrlHandlerInfo(UrlHandlerInfo&&) = default;

UrlHandlerInfo& UrlHandlerInfo::operator=(UrlHandlerInfo&&) = default;

UrlHandlerInfo::~UrlHandlerInfo() = default;

void UrlHandlerInfo::Reset() {
  origin = url::Origin();
  has_origin_wildcard = false;
  paths = {};
  exclude_paths = {};
}

base::Value UrlHandlerInfo::AsDebugValue() const {
  base::Value::Dict root;
  root.Set("origin", origin.GetDebugString());
  root.Set("has_origin_wildcard", has_origin_wildcard);

  base::Value::List& paths_json = *root.EnsureList("paths");
  for (const std::string& path : paths)
    paths_json.Append(path);

  base::Value::List& exclude_paths_json = *root.EnsureList("exclude_paths");
  for (const std::string& path : exclude_paths)
    exclude_paths_json.Append(path);

  return base::Value(std::move(root));
}

bool operator==(const UrlHandlerInfo& handler1,
                const UrlHandlerInfo& handler2) {
  return handler1.origin == handler2.origin &&
         handler1.has_origin_wildcard == handler2.has_origin_wildcard &&
         handler1.paths == handler2.paths &&
         handler1.exclude_paths == handler2.exclude_paths;
}

bool operator!=(const UrlHandlerInfo& handler1,
                const UrlHandlerInfo& handler2) {
  return !(handler1 == handler2);
}

bool operator<(const UrlHandlerInfo& handler1, const UrlHandlerInfo& handler2) {
  return std::tie(handler1.origin, handler1.has_origin_wildcard, handler1.paths,
                  handler1.exclude_paths) <
         std::tie(handler2.origin, handler2.has_origin_wildcard, handler2.paths,
                  handler2.exclude_paths);
}

}  // namespace apps
