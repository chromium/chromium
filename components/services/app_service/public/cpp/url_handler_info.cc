// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/url_handler_info.h"

namespace apps {

UrlHandlerInfo::UrlHandlerInfo() = default;

UrlHandlerInfo::UrlHandlerInfo(const url::Origin& origin) : origin(origin) {}

UrlHandlerInfo::UrlHandlerInfo(const url::Origin& origin,
                               bool has_origin_wildcard)
    : origin(origin), has_origin_wildcard(has_origin_wildcard) {}

UrlHandlerInfo::UrlHandlerInfo(const UrlHandlerInfo&) = default;

UrlHandlerInfo& UrlHandlerInfo::operator=(const UrlHandlerInfo&) = default;

UrlHandlerInfo::UrlHandlerInfo(UrlHandlerInfo&&) = default;

UrlHandlerInfo& UrlHandlerInfo::operator=(UrlHandlerInfo&&) = default;

UrlHandlerInfo::~UrlHandlerInfo() = default;

bool operator==(const UrlHandlerInfo& handler1,
                const UrlHandlerInfo& handler2) {
  return handler1.origin == handler2.origin &&
         handler1.has_origin_wildcard == handler2.has_origin_wildcard;
}

bool operator!=(const UrlHandlerInfo& handler1,
                const UrlHandlerInfo& handler2) {
  return !(handler1 == handler2);
}

std::ostream& operator<<(std::ostream& out, const UrlHandlerInfo& handler) {
  out << "origin: " << handler.origin;
  out << "has_origin_wildcard: "
      << (handler.has_origin_wildcard ? "true" : "false");
  return out;
}

}  // namespace apps
