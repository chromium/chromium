// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ttc/app/public/page_context_types.h"

#include <utility>

namespace ttc {

TabContextInfo::TabContextInfo() = default;
TabContextInfo::TabContextInfo(GURL url,
                               std::string title,
                               std::string mime_type)
    : url(std::move(url)),
      title(std::move(title)),
      mime_type(std::move(mime_type)) {}
TabContextInfo::TabContextInfo(const TabContextInfo&) = default;
TabContextInfo& TabContextInfo::operator=(const TabContextInfo&) = default;
TabContextInfo::TabContextInfo(TabContextInfo&&) noexcept = default;
TabContextInfo& TabContextInfo::operator=(TabContextInfo&&) noexcept = default;
TabContextInfo::~TabContextInfo() = default;

SharedPageContext::SharedPageContext() = default;
SharedPageContext::SharedPageContext(
    TabContextInfo tab,
    optimization_guide::proto::AnnotatedPageContent apc)
    : tab(std::move(tab)), apc(std::move(apc)) {}
SharedPageContext::SharedPageContext(const SharedPageContext&) = default;
SharedPageContext& SharedPageContext::operator=(const SharedPageContext&) =
    default;
SharedPageContext::SharedPageContext(SharedPageContext&&) = default;
SharedPageContext& SharedPageContext::operator=(SharedPageContext&&) = default;
SharedPageContext::~SharedPageContext() = default;

std::ostream& operator<<(std::ostream& os, const TabContextInfo& info) {
  return os << "{url: \"" << info.url.possibly_invalid_spec() << "\", title: \""
            << info.title << "\", mime_type: \"" << info.mime_type << "\"}";
}

std::ostream& operator<<(std::ostream& os, const SharedPageContext& context) {
  return os << "{tab: " << context.tab
            << ", apc_bytes: " << context.apc.ByteSizeLong() << "}";
}

std::ostream& operator<<(std::ostream& os,
                         const RestrictedPageContext& context) {
  return os << "{restriction_reason: " << static_cast<int>(context.reason)
            << "}";
}

}  // namespace ttc
