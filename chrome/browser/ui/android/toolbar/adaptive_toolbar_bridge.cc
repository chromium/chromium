// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/toolbar/adaptive_toolbar_bridge.h"

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/scoped_java_ref.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "chrome/browser/ui/android/toolbar/adaptive_toolbar_enums.h"
#include "components/segmentation_platform/public/android/segmentation_platform_conversion_bridge.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/toolbar/jni_headers/AdaptiveToolbarBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using segmentation_platform::InputContext;

namespace {

std::map<std::string, AdaptiveToolbarButtonVariant> GetEnumLabelMapping() {
  static base::NoDestructor<std::map<std::string, AdaptiveToolbarButtonVariant>>
      enum_label_mapping(
          {{
               segmentation_platform::kAdaptiveToolbarModelLabelNewTab,
               AdaptiveToolbarButtonVariant::kNewTab,
           },
           {
               segmentation_platform::kAdaptiveToolbarModelLabelShare,
               AdaptiveToolbarButtonVariant::kShare,
           },
           {

               segmentation_platform::kAdaptiveToolbarModelLabelVoice,
               AdaptiveToolbarButtonVariant::kVoice,
           },
           {

               segmentation_platform::kAdaptiveToolbarModelLabelTranslate,
               AdaptiveToolbarButtonVariant::kTranslate,
           },
           {

               segmentation_platform::kAdaptiveToolbarModelLabelAddToBookmarks,
               AdaptiveToolbarButtonVariant::kAddToBookmarks,
           },
           {
               segmentation_platform::kAdaptiveToolbarModelLabelReadAloud,
               AdaptiveToolbarButtonVariant::kReadAloud,
           }});

  return *enum_label_mapping;
}

AdaptiveToolbarButtonVariant ActionLabelToAdaptiveToolbarButtonVariant(
    const std::string& label) {
  std::map<std::string, AdaptiveToolbarButtonVariant> label_enum_mapping =
      GetEnumLabelMapping();

  if (label_enum_mapping.contains(label)) {
    return label_enum_mapping.at(label);
  }

  return AdaptiveToolbarButtonVariant::kUnknown;
}

void RunGetSelectedSegmentCallback(
    const JavaRef<jobject>& j_callback,
    const segmentation_platform::SegmentSelectionResult& result) {
  AdaptiveToolbarButtonVariant button_variant =
      AdaptiveToolbarButtonVariant::kUnknown;
  if (result.segment.has_value()) {
    switch (result.segment.value()) {
      case segmentation_platform::proto::SegmentId::
          OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB:
        button_variant = AdaptiveToolbarButtonVariant::kNewTab;
        break;
      case segmentation_platform::proto::SegmentId::
          OPTIMIZATION_TARGET_SEGMENTATION_SHARE:
        button_variant = AdaptiveToolbarButtonVariant::kShare;
        break;
      case segmentation_platform::proto::SegmentId::
          OPTIMIZATION_TARGET_SEGMENTATION_VOICE:
        button_variant = AdaptiveToolbarButtonVariant::kVoice;
        break;
      case segmentation_platform::proto::SegmentId::OPTIMIZATION_TARGET_UNKNOWN:
        button_variant = AdaptiveToolbarButtonVariant::kUnknown;
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
  }

  ScopedJavaLocalRef<jobject> j_result =
      Java_AdaptiveToolbarBridge_createResult(
          base::android::AttachCurrentThread(), result.is_ready,
          static_cast<int32_t>(button_variant));
  base::android::RunObjectCallbackAndroid(j_callback, j_result);
}

void RunGetClassificationSingleResultCallback(
    const base::android::JavaRef<jobject>& j_callback,
    const segmentation_platform::ClassificationResult& result) {
  std::string button_to_show =
      result.ordered_labels.empty() ? "" : result.ordered_labels[0];

  bool is_ready =
      result.status == segmentation_platform::PredictionStatus::kSucceeded;
  int button_variant = static_cast<int32_t>(
      ActionLabelToAdaptiveToolbarButtonVariant(button_to_show));

  ScopedJavaLocalRef<jobject> j_result =
      Java_AdaptiveToolbarBridge_createResult(
          base::android::AttachCurrentThread(), is_ready, button_variant);
  base::android::RunObjectCallbackAndroid(j_callback, j_result);
}

void RunGetClassificationMultipleResultCallback(
    base::OnceCallback<void(bool, std::vector<int>)> callback,
    const segmentation_platform::ClassificationResult& result) {
  std::vector<int> ranked_buttons;
  bool is_ready =
      result.status == segmentation_platform::PredictionStatus::kSucceeded;

  for (std::string label : result.ordered_labels) {
    ranked_buttons.emplace_back(
        static_cast<int32_t>(ActionLabelToAdaptiveToolbarButtonVariant(label)));
  }
  if (ranked_buttons.empty()) {
    ranked_buttons.emplace_back(
        static_cast<int32_t>(AdaptiveToolbarButtonVariant::kUnknown));
  }

  std::move(callback).Run(is_ready, ranked_buttons);
}

void RunGetAnnotatedNumericResultCallback(
    base::OnceCallback<void(bool, std::vector<int>)> callback,
    const segmentation_platform::AnnotatedNumericResult& result) {
  bool is_ready =
      result.status == segmentation_platform::PredictionStatus::kSucceeded;

  std::map<std::string, AdaptiveToolbarButtonVariant> enum_label_mapping =
      GetEnumLabelMapping();

  // Map that sorts elements with largest first.
  std::multimap<float, AdaptiveToolbarButtonVariant, std::greater<>>
      sorted_button_scores;

  for (std::pair<std::string, AdaptiveToolbarButtonVariant> button :
       enum_label_mapping) {
    std::optional<float> score_for_button =
        result.GetResultForLabel(button.first);
    if (score_for_button.has_value()) {
      sorted_button_scores.emplace(score_for_button.value(), button.second);
    }
  }

  std::vector<int> sorted_buttons;
  for (std::pair<float, AdaptiveToolbarButtonVariant> score_button :
       sorted_button_scores) {
    sorted_buttons.emplace_back(static_cast<int32_t>(score_button.second));
  }
  if (sorted_buttons.empty()) {
    sorted_buttons.emplace_back(
        static_cast<int32_t>(AdaptiveToolbarButtonVariant::kUnknown));
  }

  std::move(callback).Run(is_ready, sorted_buttons);
}

void RunJavaCallbackWithRankedButtons(
    const base::android::JavaRef<jobject>& j_callback,
    bool is_ready,
    std::vector<int> ranked_buttons) {
  ScopedJavaLocalRef<jintArray> java_ranked_buttons =
      base::android::ToJavaIntArray(base::android::AttachCurrentThread(),
                                    ranked_buttons);
  ScopedJavaLocalRef<jobject> j_result =
      Java_AdaptiveToolbarBridge_createResultList(
          base::android::AttachCurrentThread(), is_ready, java_ranked_buttons);
  base::android::RunObjectCallbackAndroid(j_callback, j_result);
}

}  // namespace

void JNI_AdaptiveToolbarBridge_GetRankedSessionVariantButtons(
    JNIEnv* env,
    Profile* profile,
    jboolean j_use_raw_results,
    const JavaParamRef<jobject>& j_callback) {
  bool use_raw_results = static_cast<bool>(j_use_raw_results);
  base::OnceCallback<void(bool, std::vector<int>)> wrapped_callback =
      base::BindOnce(&RunJavaCallbackWithRankedButtons,
                     base::android::ScopedJavaGlobalRef<jobject>(j_callback));
  adaptive_toolbar::GetRankedSessionVariantButtons(profile, use_raw_results,
                                                   std::move(wrapped_callback));
}

void JNI_AdaptiveToolbarBridge_GetSessionVariantButton(
    JNIEnv* env,
    Profile* profile,
    const JavaParamRef<jobject>& j_callback) {
  if (!profile) {
    RunGetClassificationSingleResultCallback(
        j_callback, segmentation_platform::ClassificationResult(
                        segmentation_platform::PredictionStatus::kFailed));
    return;
  }

  segmentation_platform::SegmentationPlatformService*
      segmentation_platform_service = segmentation_platform::
          SegmentationPlatformServiceFactory::GetForProfile(profile);
  if (!segmentation_platform_service) {
    RunGetClassificationSingleResultCallback(
        j_callback, segmentation_platform::ClassificationResult(
                        segmentation_platform::PredictionStatus::kFailed));
    return;
  }

  bool use_multi_output = base::FeatureList::IsEnabled(
      segmentation_platform::features::
          kSegmentationPlatformAdaptiveToolbarV2Feature);
  if (use_multi_output) {
    segmentation_platform_service->GetClassificationResult(
        segmentation_platform::kAdaptiveToolbarSegmentationKey,
        segmentation_platform::PredictionOptions(),
        base::MakeRefCounted<segmentation_platform::InputContext>(),
        base::BindOnce(
            &RunGetClassificationSingleResultCallback,
            base::android::ScopedJavaGlobalRef<jobject>(j_callback)));
  } else {
    segmentation_platform_service->GetSelectedSegment(
        segmentation_platform::kAdaptiveToolbarSegmentationKey,
        base::BindOnce(
            &RunGetSelectedSegmentCallback,
            base::android::ScopedJavaGlobalRef<jobject>(j_callback)));
  }
}

namespace adaptive_toolbar {
// This method retrieves a list of toolbar buttons ranked by priority, only one
// button can be shown, but we return a list so we can try other options in case
// the top one is not available in the current UI (e.g. tablets already have a
// bookmark button, so we don't show it here).
// This list is retrieved from segmentation platform, which:
//   1) Runs an ML model which returns a score for each button.
//   2) Applies thresholds to filter out buttons with low scores.
//   3) Returns a list sorted by score.
// The current model's low score thresholds are set to use new tab as the
// default option, so the other ones get often filtered out, this is a problem
// in tablets because that button is not supported. The |use_raw_results| option
// uses a segmentation platform API that skips step 2, so we can use the
// unfiltered scores on tablets.
void GetRankedSessionVariantButtons(
    Profile* profile,
    bool use_raw_results,
    base::OnceCallback<void(bool, std::vector<int>)> callback) {
  if (!profile) {
    std::move(callback).Run(false, std::vector<int>());
    return;
  }

  segmentation_platform::SegmentationPlatformService*
      segmentation_platform_service = segmentation_platform::
          SegmentationPlatformServiceFactory::GetForProfile(profile);
  if (!segmentation_platform_service) {
    std::move(callback).Run(false, std::vector<int>());
    return;
  }

  if (use_raw_results) {
    segmentation_platform_service->GetAnnotatedNumericResult(
        segmentation_platform::kAdaptiveToolbarSegmentationKey,
        segmentation_platform::PredictionOptions(),
        base::MakeRefCounted<segmentation_platform::InputContext>(),
        base::BindOnce(&RunGetAnnotatedNumericResultCallback,
                       std::move(callback)));
  } else {
    segmentation_platform_service->GetClassificationResult(
        segmentation_platform::kAdaptiveToolbarSegmentationKey,
        segmentation_platform::PredictionOptions(),
        base::MakeRefCounted<segmentation_platform::InputContext>(),
        base::BindOnce(&RunGetClassificationMultipleResultCallback,
                       std::move(callback)));
  }
}
}  // namespace adaptive_toolbar
