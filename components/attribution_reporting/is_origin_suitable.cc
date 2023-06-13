// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/is_origin_suitable.h"

#include <string>

#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace attribution_reporting {

bool IsOriginSuitable(const url::Origin& origin) {
  const std::string& scheme = origin.scheme();
  return (scheme == url::kHttpScheme || scheme == url::kHttpsScheme) &&
         network::IsOriginPotentiallyTrustworthy(origin);
}

}  // namespace attribution_reporting
