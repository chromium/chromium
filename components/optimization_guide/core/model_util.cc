// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_util.h"

#include "base/base64.h"
#include "base/containers/flat_set.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
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
    case proto::OPTIMIZATION_TARGET_SEGMENTATION_CHROME_LOW_USER_ENGAGEMENT:
      return "SegmentationChromeLowUserEngagement";
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

}  // namespace optimization_guide
