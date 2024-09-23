// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/resource_attribution/origin_in_browsing_instance_context.h"

#include <string>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"

namespace resource_attribution {

OriginInBrowsingInstanceContext::OriginInBrowsingInstanceContext(
    const url::Origin& origin,
    content::BrowsingInstanceId browsing_instance)
    : origin_(origin), browsing_instance_(browsing_instance) {}

OriginInBrowsingInstanceContext::~OriginInBrowsingInstanceContext() = default;

OriginInBrowsingInstanceContext::OriginInBrowsingInstanceContext(
    const OriginInBrowsingInstanceContext& other) = default;

OriginInBrowsingInstanceContext& OriginInBrowsingInstanceContext::operator=(
    const OriginInBrowsingInstanceContext& other) = default;

OriginInBrowsingInstanceContext::OriginInBrowsingInstanceContext(
    OriginInBrowsingInstanceContext&& other) = default;

OriginInBrowsingInstanceContext& OriginInBrowsingInstanceContext::operator=(
    OriginInBrowsingInstanceContext&& other) = default;

url::Origin OriginInBrowsingInstanceContext::GetOrigin() const {
  return origin_;
}

content::BrowsingInstanceId
OriginInBrowsingInstanceContext::GetBrowsingInstance() const {
  return browsing_instance_;
}

std::string OriginInBrowsingInstanceContext::ToString() const {
  return base::StrCat(
      {"OriginInBrowsingInstanceContext:", origin_.GetDebugString(), "/",
       base::NumberToString(browsing_instance_.GetUnsafeValue())});
}

}  // namespace resource_attribution
