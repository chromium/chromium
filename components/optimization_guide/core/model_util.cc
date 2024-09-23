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
    case proto::OPTIMIZATION_TARGET_CLIENT_SIDE_PHISHING_IMAGE_EMBEDDER:
      return "ClientSidePhishingImageEmbedder";
    case proto::
        OPTIMIZATION_TARGET_NEW_TAB_PAGE_HISTORY_CLUSTERS_MODULE_RANKING:
      return "NewTabPageHistoryClustersModuleRanking";
    case proto::OPTIMIZATION_TARGET_WEB_APP_INSTALLATION_PROMO:
      return "WebAppInstallationPromo";
    case proto::OPTIMIZATION_TARGET_TEXT_EMBEDDER:
      return "TextEmbedder";
    case proto::OPTIMIZATION_TARGET_VISUAL_SEARCH_CLASSIFICATION:
      return "VisualSearchClassification";
    case proto::OPTIMIZATION_TARGET_SEGMENTATION_BOTTOM_TOOLBAR:
      return "SegmentationBottomToolbar";
    case proto::OPTIMIZATION_TARGET_AUTOFILL_FIELD_CLASSIFICATION:
      return "AutofillFieldTypeClassification";
    case proto::OPTIMIZATION_TARGET_SEGMENTATION_IOS_MODULE_RANKER:
      return "SegmentationIosModuleRanker";
    case proto::OPTIMIZATION_TARGET_SEGMENTATION_DESKTOP_NTP_MODULE:
      return "SegmentationDesktopNtpModule";
    case proto::OPTIMIZATION_TARGET_PRELOADING_HEURISTICS:
      return "PreloadingHeuristics";
    case proto::OPTIMIZATION_TARGET_TEXT_SAFETY:
      return "TextSafety";
    case proto::OPTIMIZATION_TARGET_SEGMENTATION_ANDROID_HOME_MODULE_RANKER:
      return "SegmentationAndroidHomeModuleRanker";
    case proto::OPTIMIZATION_TARGET_COMPOSE:
      return "Compose";
    case proto::OPTIMIZATION_TARGET_PASSAGE_EMBEDDER:
      return "PassageEmbedder";
    case proto::OPTIMIZATION_TARGET_PHRASE_SEGMENTATION:
      return "PhraseSegmentation";
    case proto::OPTIMIZATION_TARGET_SEGMENTATION_COMPOSE_PROMOTION:
      return "SegmentationComposePromotion";
    case proto::OPTIMIZATION_TARGET_URL_VISIT_RESUMPTION_RANKER:
      return "URLVisitResumptionRanker";
    case proto::OPTIMIZATION_TARGET_CAMERA_BACKGROUND_SEGMENTATION:
      return "CameraBackgroundSegmentation";
    case proto::OPTIMIZATION_TARGET_MODEL_EXECUTION_FEATURE_HISTORY_SEARCH:
      return "ModelExecutionFeatureHistorySearch";
    case proto::OPTIMIZATION_TARGET_MODEL_EXECUTION_FEATURE_PROMPT_API:
      return "ModelExecutionFeaturePromptAPI";
    case proto::OPTIMIZATION_TARGET_SEGMENTATION_METRICS_CLUSTERING:
      return "SegmentationMetricsClustering";
    case proto::OPTIMIZATION_TARGET_MODEL_EXECUTION_FEATURE_SUMMARIZE:
      return "ModelExecutionFeatureSummarize";
      // Whenever a new value is added, make sure to add it to the OptTarget
      // variant list in
      // //tools/metrics/histograms/metadata/optimization/histograms.xml.
  }
  NOTREACHED_IN_MIGRATION();
  return std::string();
}

std::optional<base::FilePath> StringToFilePath(const std::string& str_path) {
  if (str_path.empty()) {
    return std::nullopt;
  }

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

std::optional<
    std::pair<std::string, std::optional<optimization_guide::proto::Any>>>
GetModelOverrideForOptimizationTarget(
    optimization_guide::proto::OptimizationTarget optimization_target) {
  auto model_override_switch_value = switches::GetModelOverride();
  if (!model_override_switch_value) {
    return std::nullopt;
  }

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
      return std::nullopt;
    }

    optimization_guide::proto::OptimizationTarget recv_optimization_target;
    if (!optimization_guide::proto::OptimizationTarget_Parse(
            override_parts[0], &recv_optimization_target)) {
      // Optimization target is invalid.
      DLOG(ERROR)
          << "Invalid optimization target provided to the Model Override";
      return std::nullopt;
    }
    if (optimization_target != recv_optimization_target) {
      continue;
    }

    std::string file_name = override_parts[1];
    base::FilePath file_path = *StringToFilePath(file_name);
    if (!file_path.IsAbsolute()) {
      DLOG(ERROR) << "Provided model file path must be absolute " << file_name;
      return std::nullopt;
    }

    if (override_parts.size() == 2) {
      std::pair<std::string, std::optional<optimization_guide::proto::Any>>
          file_path_and_metadata = std::make_pair(file_name, std::nullopt);
      return file_path_and_metadata;
    }

    std::string binary_pb;
    if (!base::Base64Decode(override_parts[2], &binary_pb)) {
      DLOG(ERROR) << "Invalid base64 encoding of the Model Override";
      return std::nullopt;
    }
    optimization_guide::proto::Any model_metadata;
    if (!model_metadata.ParseFromString(binary_pb)) {
      DLOG(ERROR) << "Invalid model metadata provided to the Model Override";
      return std::nullopt;
    }
    std::pair<std::string, std::optional<optimization_guide::proto::Any>>
        file_path_and_metadata = std::make_pair(file_name, model_metadata);
    return file_path_and_metadata;
  }
  return std::nullopt;
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

base::FilePath ConvertToRelativePath(const base::FilePath& parent,
                                     const base::FilePath& child) {
  DCHECK(parent.IsAbsolute());
  DCHECK(child.IsAbsolute());
  DCHECK(parent.IsParent(child));
  const auto parent_components = parent.GetComponents();
  const auto child_components = child.GetComponents();
  base::FilePath relative_path;
  for (size_t i = parent_components.size(); i < child_components.size(); i++) {
    relative_path = relative_path.Append(child_components[i]);
  }
  return relative_path;
}

std::string GetModelCacheKeyHash(proto::ModelCacheKey model_cache_key) {
  std::string bytes;
  model_cache_key.SerializeToString(&bytes);
  uint64_t hash = base::legacy::CityHash64(base::as_byte_span(bytes));
  // Convert the hash to hex encoding and not as base64 and other encodings,
  // since it will be used as filepath names.
  return base::HexEncode(base::byte_span_from_ref(hash));
}

void RecordPredictionModelStoreModelRemovalVersionHistogram(
    proto::OptimizationTarget optimization_target,
    PredictionModelStoreModelRemovalReason model_removal_reason) {
  base::UmaHistogramEnumeration(
      "OptimizationGuide.PredictionModelStore.ModelRemovalReason",
      model_removal_reason);
  base::UmaHistogramEnumeration(
      "OptimizationGuide.PredictionModelStore.ModelRemovalReason." +
          GetStringNameForOptimizationTarget(optimization_target),
      model_removal_reason);
}

bool IsPredictionModelVersionInKillSwitch(
    const std::map<proto::OptimizationTarget, std::set<int64_t>>&
        killswitch_model_versions,
    proto::OptimizationTarget opt_target,
    int64_t model_version) {
  auto killswitch_model_versions_it =
      killswitch_model_versions.find(opt_target);
  if (killswitch_model_versions_it == killswitch_model_versions.end()) {
    return false;
  }
  return killswitch_model_versions_it->second.find(model_version) !=
         killswitch_model_versions_it->second.end();
}

std::optional<proto::ModelInfo> ParseModelInfoFromFile(
    const base::FilePath& model_info_path) {
  std::string binary_model_info;
  if (!base::ReadFileToString(model_info_path, &binary_model_info)) {
    return std::nullopt;
  }

  proto::ModelInfo model_info;
  if (!model_info.ParseFromString(binary_model_info)) {
    return std::nullopt;
  }

  DCHECK(model_info.has_version());
  DCHECK(model_info.has_optimization_target());
  return model_info;
}

}  // namespace optimization_guide
