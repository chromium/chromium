// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_UTIL_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_UTIL_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_permissions_util.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/proto/models.pb.h"

#define OPTIMIZATION_GUIDE_LOG(log_source, optimization_guide_logger, message) \
  do {                                                                         \
    if (optimization_guide_logger &&                                           \
        optimization_guide_logger->ShouldEnableDebugLogs()) {                  \
      optimization_guide_logger->OnLogMessageAdded(                            \
          base::Time::Now(), log_source, __FILE__, __LINE__, message);         \
    }                                                                          \
    if (optimization_guide::switches::IsDebugLogsEnabled())                    \
      DVLOG(0) << message;                                                     \
  } while (0)

class OptimizationGuideLogger;
class PrefService;

namespace network {
struct ResourceRequest;
}  // namespace network

namespace optimization_guide {

enum class OptimizationGuideDecision;

// Returns the equivalent string name for a `feature`. The returned string can
// be used to index persistent data (e.g., prefs, histograms etc.).
std::string_view GetStringNameForModelExecutionFeature(
    std::optional<UserVisibleFeatureKey> feature);
std::string_view GetStringNameForModelExecutionFeature(
    ModelBasedCapabilityKey feature);
std::string_view GetStringNameForModelExecutionFeature(
    proto::ModelExecutionFeature feature);

// Returns false if the host is an IP address, localhosts, or an invalid
// host that is not supported by the remote optimization guide.
bool IsHostValidToFetchFromRemoteOptimizationGuide(const std::string& host);

// Validates that the metadata stored in |any_metadata_| is of the same type
// and is parseable as |T|. Will return metadata if all checks pass.
template <class T,
          class = typename std::enable_if<
              std::is_convertible<T*, google::protobuf::MessageLite*>{}>::type>
std::optional<T> ParsedAnyMetadata(const proto::Any& any_metadata) {
  // Verify type is the same - the Any type URL should be wrapped as:
  // "type.googleapis.com/com.foo.Name".
  std::vector<std::string> any_type_parts =
      base::SplitString(any_metadata.type_url(), "./", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  if (any_type_parts.empty()) {
    return std::nullopt;
  }
  T metadata;
  std::vector<std::string> type_parts =
      base::SplitString(metadata.GetTypeName(), "./", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  if (type_parts.empty()) {
    return std::nullopt;
  }
  std::string any_type_name = any_type_parts.back();
  std::string type_name = type_parts.back();
  if (type_name != any_type_name) {
    return std::nullopt;
  }

  // Return metadata if parseable.
  if (metadata.ParseFromString(any_metadata.value())) {
    return metadata;
  }
  return std::nullopt;
}

// Returns a debug string for OptimizationGuideDecision.
std::string GetStringForOptimizationGuideDecision(
    OptimizationGuideDecision decision);

// Returns client's origin info, including platform and milestone.
proto::OriginInfo GetClientOriginInfo();

// Logs info about the common optimization guide feature flags.
void LogFeatureFlagsInfo(OptimizationGuideLogger* optimization_guide_logger,
                         bool is_off_the_record,
                         PrefService* pref_service);

// Populates the authorization header for the `resource_request` in the right
// format with the `access_token`.
void PopulateAuthorizationRequestHeader(
    network::ResourceRequest* resource_request,
    std::string_view access_token);

// Populates the api key header for the `resource_request` in the right
// format with the `api_key`.
void PopulateApiKeyRequestHeader(network::ResourceRequest* resource_request,
                                 std::string_view api_key);

// Returns whether model validator service should be started to validate various
// model executions such as, TFLite, server-side AI, on-device AI models. Used
// for integration testing purposes.
bool ShouldStartModelValidator();

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_UTIL_H_
