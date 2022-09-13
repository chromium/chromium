// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_util.h"

#include "base/containers/flat_set.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/prefs/pref_service.h"
#include "net/base/url_util.h"
#include "url/url_canon.h"

namespace {
optimization_guide::proto::Platform GetPlatform() {
#if BUILDFLAG(IS_WIN)
  return optimization_guide::proto::PLATFORM_WINDOWS;
#elif BUILDFLAG(IS_IOS)
  return optimization_guide::proto::PLATFORM_IOS;
#elif BUILDFLAG(IS_MAC)
  return optimization_guide::proto::PLATFORM_MAC;
#elif BUILDFLAG(IS_CHROMEOS)
  return optimization_guide::proto::PLATFORM_CHROMEOS;
#elif BUILDFLAG(IS_ANDROID)
  return optimization_guide::proto::PLATFORM_ANDROID;
#elif BUILDFLAG(IS_LINUX)
  return optimization_guide::proto::PLATFORM_LINUX;
#else
  return optimization_guide::proto::PLATFORM_UNKNOWN;
#endif
}
}  // namespace

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

optimization_guide::proto::OriginInfo GetClientOriginInfo() {
  optimization_guide::proto::OriginInfo origin_info;
  origin_info.set_platform(GetPlatform());
  return origin_info;
}

void LogFeatureFlagsInfo(OptimizationGuideLogger* optimization_guide_logger,
                         bool is_off_the_record,
                         PrefService* pref_service) {
  if (!optimization_guide::switches::IsDebugLogsEnabled())
    return;
  if (!optimization_guide::features::IsOptimizationHintsEnabled()) {
    OPTIMIZATION_GUIDE_LOG(
        optimization_guide_common::mojom::LogSource::SERVICE_AND_SETTINGS,
        optimization_guide_logger, "FEATURE_FLAG Hints component disabled");
  }
  if (!optimization_guide::features::IsRemoteFetchingEnabled()) {
    OPTIMIZATION_GUIDE_LOG(
        optimization_guide_common::mojom::LogSource::SERVICE_AND_SETTINGS,
        optimization_guide_logger,
        "FEATURE_FLAG remote fetching feature disabled");
  }
  if (!optimization_guide::IsUserPermittedToFetchFromRemoteOptimizationGuide(
          is_off_the_record, pref_service)) {
    OPTIMIZATION_GUIDE_LOG(
        optimization_guide_common::mojom::LogSource::SERVICE_AND_SETTINGS,
        optimization_guide_logger,
        "FEATURE_FLAG remote fetching user permission disabled");
  }
  if (!optimization_guide::features::IsPushNotificationsEnabled()) {
    OPTIMIZATION_GUIDE_LOG(
        optimization_guide_common::mojom::LogSource::SERVICE_AND_SETTINGS,
        optimization_guide_logger,
        "FEATURE_FLAG remote push notification feature disabled");
  }
  if (!optimization_guide::features::IsModelDownloadingEnabled()) {
    OPTIMIZATION_GUIDE_LOG(
        optimization_guide_common::mojom::LogSource::SERVICE_AND_SETTINGS,
        optimization_guide_logger,
        "FEATURE_FLAG model downloading feature disabled");
  }
}

}  // namespace optimization_guide
