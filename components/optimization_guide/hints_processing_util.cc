// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/hints_processing_util.h"

#include <string>

#include "base/containers/flat_set.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/stringprintf.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/store_update_data.h"
#include "components/optimization_guide/url_pattern_with_wildcards.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace optimization_guide {

// The returned string is used to record histograms for the optimization type.
// Also add the string to OptimizationGuide.OptimizationTypes histogram suffix
// in histograms.xml.
std::string GetStringNameForOptimizationType(
    proto::OptimizationType optimization_type) {
  switch (optimization_type) {
    case proto::OptimizationType::TYPE_UNSPECIFIED:
      return "Unspecified";
    case proto::OptimizationType::NOSCRIPT:
      return "NoScript";
    case proto::OptimizationType::RESOURCE_LOADING:
      return "ResourceLoading";
    case proto::OptimizationType::LITE_PAGE_REDIRECT:
      return "LitePageRedirect";
    case proto::OptimizationType::OPTIMIZATION_NONE:
      return "None";
    case proto::OptimizationType::DEFER_ALL_SCRIPT:
      return "DeferAllScript";
    case proto::OptimizationType::PERFORMANCE_HINTS:
      return "PerformanceHints";
    case proto::OptimizationType::LITE_PAGE:
      return "LitePage";
    case proto::OptimizationType::COMPRESS_PUBLIC_IMAGES:
      return "CompressPublicImages";
    case proto::OptimizationType::LOADING_PREDICTOR:
      return "LoadingPredictor";
    case proto::OptimizationType::FAST_HOST_HINTS:
      return "FastHostHints";
    case proto::OptimizationType::DELAY_ASYNC_SCRIPT_EXECUTION:
      return "DelayAsyncScriptExecution";
    case proto::OptimizationType::DELAY_COMPETING_LOW_PRIORITY_REQUESTS:
      return "DelayCompetingLowPriorityRequests";
    case proto::OptimizationType::LITE_VIDEO:
      return "LiteVideo";
  }
  NOTREACHED();
  return std::string();
}

const proto::PageHint* FindPageHintForURL(const GURL& gurl,
                                          const proto::Hint* hint) {
  if (!hint) {
    return nullptr;
  }

  for (const auto& page_hint : hint->page_hints()) {
    if (page_hint.page_pattern().empty()) {
      continue;
    }
    URLPatternWithWildcards url_pattern(page_hint.page_pattern());
    if (url_pattern.Matches(gurl.spec())) {
      // Return the first matching page hint.
      return &page_hint;
    }
  }
  return nullptr;
}

std::string HashHostForDictionary(const std::string& host) {
  return base::StringPrintf("%x", base::PersistentHash(host));
}

net::EffectiveConnectionType ConvertProtoEffectiveConnectionType(
    proto::EffectiveConnectionType proto_ect) {
  switch (proto_ect) {
    case proto::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_UNKNOWN:
      return net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
    case proto::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_OFFLINE:
      return net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_OFFLINE;
    case proto::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G:
      return net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G;
    case proto::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_2G:
      return net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_2G;
    case proto::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_3G:
      return net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_3G;
    case proto::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_4G:
      return net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_4G;
  }
}

bool IsValidURLForURLKeyedHint(const GURL& url) {
  if (!url.has_host())
    return false;
  if (net::IsLocalhost(url))
    return false;
  if (url.HostIsIPAddress())
    return false;
  if (!url.SchemeIsHTTPOrHTTPS())
    return false;
  if (url.has_username() || url.has_password())
    return false;
  return true;
}

}  // namespace optimization_guide
