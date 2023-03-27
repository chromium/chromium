// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/callback_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "chrome/browser/ui/android/toolbar/adaptive_toolbar_enums.h"
#include "chrome/browser/ui/android/toolbar/jni_headers/AdaptiveToolbarBridge_jni.h"
#include "components/segmentation_platform/public/android/segmentation_platform_conversion_bridge.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using segmentation_platform::InputContext;

namespace {

AdaptiveToolbarButtonVariant ActionLabelToAdaptiveToolbarButtonVariant(
    const std::string& label) {
  AdaptiveToolbarButtonVariant button_variant =
      AdaptiveToolbarButtonVariant::kUnknown;
  if (label == segmentation_platform::kAdaptiveToolbarModelLabelNewTab) {
    button_variant = AdaptiveToolbarButtonVariant::kNewTab;
  } else if (label == segmentation_platform::kAdaptiveToolbarModelLabelShare) {
    button_variant = AdaptiveToolbarButtonVariant::kShare;
  } else if (label == segmentation_platform::kAdaptiveToolbarModelLabelVoice) {
    button_variant = AdaptiveToolbarButtonVariant::kVoice;
  } else if (label ==
             segmentation_platform::kAdaptiveToolbarModelLabelTranslate) {
    button_variant = AdaptiveToolbarButtonVariant::kTranslate;
  } else if (label ==
             segmentation_platform::kAdaptiveToolbarModelLabelAddToBookmarks) {
    button_variant = AdaptiveToolbarButtonVariant::kAddToBookmarks;
  }
  return button_variant;
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
        NOTREACHED();
    }
  }

  ScopedJavaLocalRef<jobject> j_result =
      Java_AdaptiveToolbarBridge_createResult(
          base::android::AttachCurrentThread(), result.is_ready,
          static_cast<int32_t>(button_variant));
  base::android::RunObjectCallbackAndroid(j_callback, j_result);
}

void RunGetClassificationResultCallback(
    const base::android::JavaRef<jobject>& j_callback,
    const segmentation_platform::ClassificationResult& result) {
  std::string button_to_show =
      result.ordered_labels.empty() ? "" : result.ordered_labels[0];

  bool is_ready =
      result.status != segmentation_platform::PredictionStatus::kNotReady;
  int button_variant = static_cast<int32_t>(
      ActionLabelToAdaptiveToolbarButtonVariant(button_to_show));

  ScopedJavaLocalRef<jobject> j_result =
      Java_AdaptiveToolbarBridge_createResult(
          base::android::AttachCurrentThread(), is_ready, button_variant);
  base::android::RunObjectCallbackAndroid(j_callback, j_result);
}

}  // namespace

void JNI_AdaptiveToolbarBridge_GetSessionVariantButton(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile,
    const JavaParamRef<jobject>& j_callback) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  if (!profile) {
    RunGetClassificationResultCallback(
        j_callback, segmentation_platform::ClassificationResult(
                        segmentation_platform::PredictionStatus::kFailed));
    return;
  }

  segmentation_platform::SegmentationPlatformService*
      segmentation_platform_service = segmentation_platform::
          SegmentationPlatformServiceFactory::GetForProfile(profile);
  if (!segmentation_platform_service) {
    RunGetClassificationResultCallback(
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
            &RunGetClassificationResultCallback,
            base::android::ScopedJavaGlobalRef<jobject>(j_callback)));
  } else {
    segmentation_platform_service->GetSelectedSegment(
        segmentation_platform::kAdaptiveToolbarSegmentationKey,
        base::BindOnce(
            &RunGetSelectedSegmentCallback,
            base::android::ScopedJavaGlobalRef<jobject>(j_callback)));
  }
}
