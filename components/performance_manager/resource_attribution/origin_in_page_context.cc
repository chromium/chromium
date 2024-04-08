// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/resource_attribution/origin_in_page_context.h"

#include <string>

#include "base/strings/strcat.h"
#include "components/performance_manager/public/resource_attribution/page_context.h"
#include "url/origin.h"

namespace resource_attribution {

OriginInPageContext::OriginInPageContext(const url::Origin& origin,
                                         const PageContext& page_context)
    : origin_(origin), page_context_(page_context) {}

OriginInPageContext::~OriginInPageContext() = default;

OriginInPageContext::OriginInPageContext(const OriginInPageContext& other) =
    default;

OriginInPageContext& OriginInPageContext::operator=(
    const OriginInPageContext& other) = default;

OriginInPageContext::OriginInPageContext(OriginInPageContext&& other) = default;

OriginInPageContext& OriginInPageContext::operator=(
    OriginInPageContext&& other) = default;

url::Origin OriginInPageContext::GetOrigin() const {
  return origin_;
}

PageContext OriginInPageContext::GetPageContext() const {
  return page_context_;
}

std::string OriginInPageContext::ToString() const {
  return base::StrCat({"OriginInPageContext:", origin_.GetDebugString(), "/",
                       page_context_.ToString()});
}

}  // namespace resource_attribution
