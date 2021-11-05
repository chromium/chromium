// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_util.h"

#include "base/base64.h"
#include "base/containers/flat_set.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/variations/active_field_trials.h"
#include "net/base/url_util.h"
#include "url/url_canon.h"

namespace optimization_guide {

// These names are persisted to histograms, so don't change them.
std::string GetStringNameForOptimizationTarget(
    optimization_guide::proto::OptimizationTarget optimization_target) {
  switch (optimization_target) {
    case proto::OPTIMIZATION_TARGET_UNKNOWN:
      return "Unknown";
    case proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD:
      return "PainfulPageLoad";
    case proto::OPTIMIZATION_TARGET_LANGUAGE_DETECTION:
      return "LanguageDetection";
    case proto::OPTIMIZATION_TARGET_PAGE_TOPICS:
      return "PageTopics";
    case proto::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB:
      return "SegmentationNewTab";
    case proto::OPTIMIZATION_TARGET_SEGMENTATION_SHARE:
      return "SegmentationShare";
    case proto::OPTIMIZATION_TARGET_SEGMENTATION_VOICE:
      return "SegmentationVoice";
    case proto::OPTIMIZATION_TARGET_MODEL_VALIDATION:
      return "ModelValidation";
    case proto::OPTIMIZATION_TARGET_PAGE_ENTITIES:
      return "PageEntities";
    case proto::OPTIMIZATION_TARGET_NOTIFICATION_PERMISSION_PREDICTIONS:
      return "NotificationPermissions";
    case proto::OPTIMIZATION_TARGET_SEGMENTATION_DUMMY:
      return "SegmentationDummyFeature";
    case proto::OPTIMIZATION_TARGET_SEGMENTATION_CHROME_START_ANDROID:
      return "SegmentationChromeStartAndroid";
    case proto::OPTIMIZATION_TARGET_SEGMENTATION_QUERY_TILES:
      return "SegmentationQueryTiles";
    case proto::OPTIMIZATION_TARGET_PAGE_VISIBILITY:
      return "PageVisibility";
    case proto::OPTIMIZATION_TARGET_AUTOFILL_ASSISTANT:
      return "AutofillAssistant";
    case proto::OPTIMIZATION_TARGET_PAGE_TOPICS_V2:
      return "PageTopicsV2";
      // Whenever a new value is added, make sure to add it to the OptTarget
      // variant list in
      // //tools/metrics/histograms/metadata/optimization/histograms.xml.
  }
  NOTREACHED();
  return std::string();
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

google::protobuf::RepeatedPtrField<proto::FieldTrial>
GetActiveFieldTrialsAllowedForFetch() {
  google::protobuf::RepeatedPtrField<proto::FieldTrial>
      filtered_active_field_trials;

  base::flat_set<uint32_t> allowed_field_trials_for_fetch =
      features::FieldTrialNameHashesAllowedForFetch();
  if (allowed_field_trials_for_fetch.empty())
    return filtered_active_field_trials;

  std::vector<variations::ActiveGroupId> active_field_trials;
  variations::GetFieldTrialActiveGroupIds(/*suffix=*/"", &active_field_trials);
  for (const auto& active_field_trial : active_field_trials) {
    if (static_cast<size_t>(filtered_active_field_trials.size()) ==
        allowed_field_trials_for_fetch.size()) {
      // We've found all the field trials that we are allowed to send to the
      // server.
      break;
    }

    if (allowed_field_trials_for_fetch.find(active_field_trial.name) ==
        allowed_field_trials_for_fetch.end()) {
      // Continue if we are not allowed to send the field trial to the server.
      continue;
    }

    proto::FieldTrial* ft_proto = filtered_active_field_trials.Add();
    ft_proto->set_name_hash(active_field_trial.name);
    ft_proto->set_group_hash(active_field_trial.group);
  }
  return filtered_active_field_trials;
}

absl::optional<base::FilePath> StringToFilePath(const std::string& str_path) {
  if (str_path.empty())
    return absl::nullopt;

#if defined(OS_WIN)
  return base::FilePath(base::UTF8ToWide(str_path));
#else
  return base::FilePath(str_path);
#endif
}

std::string FilePathToString(const base::FilePath& file_path) {
#if defined(OS_WIN)
  return base::WideToUTF8(file_path.value());
#else
  return file_path.value();
#endif
}

base::FilePath GetBaseFileNameForModels() {
  return base::FilePath(FILE_PATH_LITERAL("model.tflite"));
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

absl::optional<
    std::pair<std::string, absl::optional<optimization_guide::proto::Any>>>
GetModelOverrideForOptimizationTarget(
    optimization_guide::proto::OptimizationTarget optimization_target) {
#if defined(OS_WIN)
  // TODO(crbug/1227996): The parsing below is not supported on Windows because
  // ':' is used as a delimiter, but this must be used in the absolute file path
  // on Windows.
  DLOG(ERROR)
      << "--optimization-guide-model-override is not available on Windows";
  return absl::nullopt;
#else
  auto model_override_switch_value = switches::GetModelOverride();
  if (!model_override_switch_value)
    return absl::nullopt;

  std::vector<std::string> model_overrides =
      base::SplitString(*model_override_switch_value, ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const auto& model_override : model_overrides) {
    std::vector<std::string> override_parts = base::SplitString(
        model_override, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (override_parts.size() != 2 && override_parts.size() != 3) {
      // Input is malformed.
      DLOG(ERROR) << "Invalid string format provided to the Model Override";
      return absl::nullopt;
    }

    optimization_guide::proto::OptimizationTarget recv_optimization_target;
    if (!optimization_guide::proto::OptimizationTarget_Parse(
            override_parts[0], &recv_optimization_target)) {
      // Optimization target is invalid.
      DLOG(ERROR)
          << "Invalid optimization target provided to the Model Override";
      return absl::nullopt;
    }
    if (optimization_target != recv_optimization_target)
      continue;

    std::string file_name = override_parts[1];
    if (!base::FilePath(file_name).IsAbsolute()) {
      DLOG(ERROR) << "Provided model file path must be absolute " << file_name;
      return absl::nullopt;
    }

    if (override_parts.size() == 2) {
      std::pair<std::string, absl::optional<optimization_guide::proto::Any>>
          file_path_and_metadata = std::make_pair(file_name, absl::nullopt);
      return file_path_and_metadata;
    }
    std::string binary_pb;
    if (!base::Base64Decode(override_parts[2], &binary_pb)) {
      DLOG(ERROR) << "Invalid base64 encoding of the Model Override";
      return absl::nullopt;
    }
    optimization_guide::proto::Any model_metadata;
    if (!model_metadata.ParseFromString(binary_pb)) {
      DLOG(ERROR) << "Invalid model metadata provided to the Model Override";
      return absl::nullopt;
    }
    std::pair<std::string, absl::optional<optimization_guide::proto::Any>>
        file_path_and_metadata = std::make_pair(file_name, model_metadata);
    return file_path_and_metadata;
  }
  return absl::nullopt;
#endif
}

}  // namespace optimization_guide
