// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_URL_HANDLER_INFO_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_URL_HANDLER_INFO_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/values.h"
#include "url/origin.h"

namespace apps {

// Contains information about a web app's URL handler information derived from
// its web app manifest.
struct UrlHandlerInfo {
  UrlHandlerInfo();
  explicit UrlHandlerInfo(const url::Origin& origin);
  UrlHandlerInfo(const url::Origin& origin, bool has_origin_wildcard);
  UrlHandlerInfo(const url::Origin& origin,
                 bool has_origin_wildcard,
                 std::vector<std::string> paths,
                 std::vector<std::string> exclude_paths);
  // Copyable to support web_app::WebApp being copyable as it has a UrlHandlers
  // member variable.
  UrlHandlerInfo(const UrlHandlerInfo&);
  UrlHandlerInfo& operator=(const UrlHandlerInfo&);
  // Movable to support being contained in std::vector, which requires value
  // types to be copyable or movable.
  UrlHandlerInfo(UrlHandlerInfo&&);
  UrlHandlerInfo& operator=(UrlHandlerInfo&&);

  ~UrlHandlerInfo();

  // Reset the url handler to its default state.
  REINITIALIZES_AFTER_MOVE void Reset();

  base::Value AsDebugValue() const;

  url::Origin origin;

  bool has_origin_wildcard = false;

  std::vector<std::string> paths;
  std::vector<std::string> exclude_paths;
};

using UrlHandlers = std::vector<UrlHandlerInfo>;

bool operator==(const UrlHandlerInfo& url_handler1,
                const UrlHandlerInfo& url_handler2);

bool operator!=(const UrlHandlerInfo& url_handler1,
                const UrlHandlerInfo& url_handler2);

// Allow UrlHandlerInfo to be used as a key in STL (for example, a std::set or
// std::map).
bool operator<(const UrlHandlerInfo& url_handler1,
               const UrlHandlerInfo& url_handler2);

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_URL_HANDLER_INFO_H_
