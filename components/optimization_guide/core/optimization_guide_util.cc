// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_util.h"

#include "base/containers/flat_set.h"
#include "base/notreached.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "net/base/url_util.h"
#include "url/url_canon.h"

namespace optimization_guide {

bool IsHostValidToFetchFromRemoteOptimizationGuide(const std::string& host) {
  if (net::HostStringIsLocalhost(host))
    return false;
  url::CanonHostInfo host_info;
  std::string canonicalized_host(net::CanonicalizeHost(host, &host_info));
  if (host_info.IsIPAddress() ||
      !net::IsCanonicalizedHostCompliant(canonicalized_host)) {
    return false;
  }
  return true;
}

std::string GetStringForOptimizationGuideDecision(
    OptimizationGuideDecision decision) {
  switch (decision) {
    case OptimizationGuideDecision::kUnknown:
      return "Unknown";
    case OptimizationGuideDecision::kTrue:
      return "True";
    case OptimizationGuideDecision::kFalse:
      return "False";
  }
  NOTREACHED();
  return std::string();
}

}  // namespace optimization_guide
