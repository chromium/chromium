// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_util.h"

#include "base/containers/flat_set.h"
#include "base/hash/hash.h"
#include "base/i18n/time_formatting.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/prefs/pref_service.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "url/url_canon.h"

namespace {

constexpr char kAuthHeaderBearer[] = "Bearer ";
constexpr char kApiKeyHeader[] = "X-Goog-Api-Key";

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
  return optimization_guide::proto::PLATFORM_UNDEFINED;
#endif
}

// Generates a new client id and stores it in prefs.
int64_t GenerateAndStoreClientId(PrefService* pref_service) {
  int64_t client_id = 0;

  // If no value is stored in prefs, GetInt64 returns 0, so we need to use a
  // non-zero ID to differentiate the case where no ID is set versus the ID is
  // 0. We offset by a positive number to return a non-zero client-id.
  int64_t number;
  base::RandBytes(&number, sizeof(number));
  client_id = number;
  if (client_id == 0) {
    // Reassign client_id to a non-zero number.
    client_id = base::RandInt(1, 10000);
  }

  pref_service->SetInt64(
      optimization_guide::prefs::kModelQualityLogggingClientId, client_id);
  return client_id;
}

std::string TimeToYYYYMMDDString(base::Time ts) {
  // Converts a Time object to a YYYY-MM-DD string.
  return base::UnlocalizedTimeFormatWithPattern(ts, "yyyyMMdd",
                                                icu::TimeZone::getGMT());
}

}  // namespace

namespace optimization_guide {

std::string_view GetStringNameForModelExecutionFeature(
    proto::ModelExecutionFeature feature) {
  switch (feature) {
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_WALLPAPER_SEARCH:
      return "WallpaperSearch";
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_TAB_ORGANIZATION:
      return "TabOrganization";
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_COMPOSE:
      return "Compose";
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_TEST:
      return "Test";
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_UNSPECIFIED:
      return "Unknown";
      // Must be in sync with the ModelExecutionFeature variant in
      // optimization/histograms.xml for metric recording. The output may also
      // be used for storing other persistent data (e.g., prefs).
  }
}

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

void PopulateAuthorizationRequestHeader(
    network::ResourceRequest* resource_request,
    std::string_view access_token) {
  CHECK(!access_token.empty());
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StrCat({kAuthHeaderBearer, access_token}));
}

void PopulateApiKeyRequestHeader(network::ResourceRequest* resource_request,
                                 std::string_view api_key) {
  CHECK(!api_key.empty());
  resource_request->headers.SetHeader(kApiKeyHeader, api_key);
}

int64_t GetOrCreateModelQualityClientId(proto::ModelExecutionFeature feature,
                                        PrefService* pref_service) {
  if (!pref_service) {
    return 0;
  }
  int64_t client_id = pref_service->GetInt64(
      optimization_guide::prefs::kModelQualityLogggingClientId);
  if (!client_id) {
    client_id = GenerateAndStoreClientId(pref_service);
    pref_service->SetInt64(
        optimization_guide::prefs::kModelQualityLogggingClientId, client_id);
  }

  // Hash the client id with the date so that it changes everyday for every
  // feature.
  std::string date = TimeToYYYYMMDDString(base::Time::Now());
  return base::FastHash(
      base::NumberToString(client_id + static_cast<int>(feature)) + date);
}

}  // namespace optimization_guide
