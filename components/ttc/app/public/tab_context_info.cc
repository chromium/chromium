// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ttc/app/public/tab_context_info.h"

#include <utility>

namespace ttc {

TabContextInfo::TabContextInfo() = default;
TabContextInfo::TabContextInfo(GURL url, std::string title)
    : url(std::move(url)), title(std::move(title)) {}
TabContextInfo::TabContextInfo(const TabContextInfo&) = default;
TabContextInfo& TabContextInfo::operator=(const TabContextInfo&) = default;
TabContextInfo::TabContextInfo(TabContextInfo&&) noexcept = default;
TabContextInfo& TabContextInfo::operator=(TabContextInfo&&) noexcept = default;
TabContextInfo::~TabContextInfo() = default;

std::ostream& operator<<(std::ostream& os, const TabContextInfo& info) {
  return os << "{url: \"" << info.url.possibly_invalid_spec() << "\", title: \""
            << info.title << "\"}";
}

}  // namespace ttc
