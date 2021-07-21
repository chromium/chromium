// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_util.h"

#include "base/containers/flat_set.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/variations/active_field_trials.h"
#include "net/base/url_util.h"
#include "url/url_canon.h"

namespace optimization_guide {

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

}  // namespace optimization_guide
