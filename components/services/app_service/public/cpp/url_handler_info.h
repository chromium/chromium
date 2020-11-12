// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_URL_HANDLER_INFO_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_URL_HANDLER_INFO_H_

#include <ostream>
#include <vector>

#include "url/origin.h"

namespace apps {

// Contains information about a web app's URL handler information derived from
// its web app manifest.
struct UrlHandlerInfo {
  UrlHandlerInfo();
  // Copyable to support web_app::WebApp being copyable as it has a UrlHandlers
  // member variable.
  UrlHandlerInfo(const UrlHandlerInfo&);
  UrlHandlerInfo& operator=(const UrlHandlerInfo&);
  // Movable to support being contained in std::vector, which requires value
  // types to be copyable or movable.
  UrlHandlerInfo(UrlHandlerInfo&&);
  UrlHandlerInfo& operator=(UrlHandlerInfo&&);

  ~UrlHandlerInfo();

  url::Origin origin;
};

using UrlHandlers = std::vector<UrlHandlerInfo>;

bool operator==(const UrlHandlerInfo& url_handler1,
                const UrlHandlerInfo& url_handler2);

bool operator!=(const UrlHandlerInfo& url_handler1,
                const UrlHandlerInfo& url_handler2);

std::ostream& operator<<(std::ostream& out,
                         const UrlHandlerInfo& url_handler_info);
}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_URL_HANDLER_INFO_H_
