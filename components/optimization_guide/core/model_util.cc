// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_util.h"

#include "base/base64.h"
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/hash/legacy_hash.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "net/base/url_util.h"
#include "url/url_canon.h"

namespace optimization_guide {

namespace {

// The ":" character is reserved in Windows as part of an absolute file path,
// e.g.: C:\model.tflite, so we use a different separtor.
#if BUILDFLAG(IS_WIN)
const char kModelOverrideSeparator[] = "|";
#else
const char kModelOverrideSeparator[] = ":";
#endif

}  // namespace

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
    case proto::OPTIMIZATION_TARGET_PAGE_TOPICS_V2:
      return "PageTopicsV2";
    case proto::OPTIMIZATION_TARGET_SEGMENTATION_CHROME_LOW_USER_ENGAGEMENT:
      return "SegmentationChromeLowUserEngagement";
    case proto::OPTIMIZATION_TARGET_SEGMENTATION_FEED_USER:
      return "SegmentationFeedUser";
    case proto::OPTIMIZATION_TARGET_CONTEXTUAL_PAGE_ACTION_PRICE_TRACKING:
      return "ContextualPageActionPriceTracking";
    case proto::OPTIMIZATION_TARGET_TEXT_CLASSIFIER:
      return "TextClassifier";
    case proto::OPTIMIZATION_TARGET_SEGMENTATION_SHOPPING_USER:
      return "SegmentationShoppingUser";
    case proto::OPTIMIZATION_TARGET_GEOLOCATION_PERMISSION_PREDICTIONS:
      return "GeolocationPermissions";
    case proto::OPTIMIZATION_TARGET_SEGMENTATION_CHROME_START_ANDROID_V2:
      return "SegmentationChromeStartAndroidV2";
    case proto::OPTIMIZATION_TARGET_SEGMENTATION_SEARCH_USER:
      return "SegmentationSearchUser";
    case proto::OPTIMIZATION_TARGET_OMNIBOX_ON_DEVICE_TAIL_SUGGEST:
      return "OmniboxOnDeviceTailSuggest";
    case proto::OPTIMIZATION_TARGET_CLIENT_SIDE_PHISHING:
      return "ClientSidePhishing";
    case proto::OPTIMIZATION_TARGET_OMNIBOX_URL_SCORING:
      return "OmniboxUrlScoring";
    case proto::OPTIMIZATION_TARGET_SEGMENTATION_DEVICE_SWITCHER:
      return "SegmentationDeviceSwitcher";
    case proto::OPTIMIZATION_TARGET_SEGMENTATION_ADAPTIVE_TOOLBAR:
      return "SegmentationAdaptiveToolbar";
    case proto::OPTIMIZATION_TARGET_SEGMENTATION_TABLET_PRODUCTIVITY_USER:
      return "SegmentationTabletProductivityUser";
      // Whenever a new value is added, make sure to add it to the OptTarget
      // variant list in
      // //tools/metrics/histograms/metadata/optimization/histograms.xml.
  }
  NOTREACHED();
  return std::string();
}

absl::optional<base::FilePath> StringToFilePath(const std::string& str_path) {
  if (str_path.empty())
    return absl::nullopt;

#if BUILDFLAG(IS_WIN)
  return base::FilePath(base::UTF8ToWide(str_path));
#else
  return base::FilePath(str_path);
#endif
}

std::string FilePathToString(const base::FilePath& file_path) {
#if BUILDFLAG(IS_WIN)
  return base::WideToUTF8(file_path.value());
#else
  return file_path.value();
#endif
}

base::FilePath GetBaseFileNameForModels() {
  return base::FilePath(FILE_PATH_LITERAL("model.tflite"));
}

base::FilePath GetBaseFileNameForModelInfo() {
  return base::FilePath(FILE_PATH_LITERAL("model-info.pb"));
}

std::string ModelOverrideSeparator() {
  return kModelOverrideSeparator;
}

absl::optional<
    std::pair<std::string, absl::optional<optimization_guide::proto::Any>>>
GetModelOverrideForOptimizationTarget(
    optimization_guide::proto::OptimizationTarget optimization_target) {
  auto model_override_switch_value = switches::GetModelOverride();
  if (!model_override_switch_value)
    return absl::nullopt;

  std::vector<std::string> model_overrides =
      base::SplitString(*model_override_switch_value, ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const auto& model_override : model_overrides) {
    std::vector<std::string> override_parts =
        base::SplitString(model_override, kModelOverrideSeparator,
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
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
    base::FilePath file_path = *StringToFilePath(file_name);
    if (!file_path.IsAbsolute()) {
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
}

bool CheckAllPathsExist(
    const std::vector<base::FilePath>& file_paths_to_check) {
  for (const base::FilePath& file_path : file_paths_to_check) {
    if (!base::PathExists(file_path)) {
      return false;
    }
  }
  return true;
}

std::string GetModelCacheKeyHash(proto::ModelCacheKey model_cache_key) {
  std::string bytes;
  model_cache_key.SerializeToString(&bytes);
  uint64_t hash =
      base::legacy::CityHash64(base::as_bytes(base::make_span(bytes)));
  // Convert the hash to hex encoding and not as base64 and other encodings,
  // since it will be used as filepath names.
  return base::HexEncode(base::as_bytes(base::make_span(&hash, 1u)));
}

void RecordPredictionModelStoreModelRemovalVersionHistogram(
    PredictionModelStoreModelRemovalReason model_removal_reason) {
  base::UmaHistogramEnumeration(
      "OptimizationGuide.PredictionModelStore.ModelRemovalReason",
      model_removal_reason);
}

}  // namespace optimization_guide
