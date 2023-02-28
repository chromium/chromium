// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_FEATURES_H_
#define COMPONENTS_VIZ_COMMON_FEATURES_H_

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "components/viz/common/delegated_ink_prediction_configuration.h"
#include "components/viz/common/viz_common_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// See the following for guidance on adding new viz feature flags:
// https://cs.chromium.org/chromium/src/components/viz/README.md#runtime-features

namespace features {

VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kEnableOverlayPrioritization);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kDelegatedCompositing);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kRecordSkPicture);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kUseMultipleOverlays);
VIZ_COMMON_EXPORT extern const char kMaxOverlaysParam[];
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kVideoDetectorIgnoreNonVideos);
#if BUILDFLAG(IS_ANDROID)
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kDynamicColorGamut);
#endif
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kVizFrameSubmissionForWebView);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kUseRealBuffersForPageFlipTest);
#if BUILDFLAG(IS_FUCHSIA)
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kUseSkiaOutputDeviceBufferQueue);
#endif
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebRtcLogCapturePipeline);
#if BUILDFLAG(IS_WIN)
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kUseSetPresentDuration);
#endif  // BUILDFLAG(IS_WIN)
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebViewVulkanIntermediateBuffer);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kUsePlatformDelegatedInk);
#if BUILDFLAG(IS_ANDROID)
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kUseSurfaceLayerForVideoDefault);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebViewNewInvalidateHeuristic);
#endif
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kDynamicSchedulerForDraw);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kDynamicSchedulerForClients);
#if BUILDFLAG(IS_MAC)
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kMacCAOverlayQuad);
VIZ_COMMON_EXPORT extern const base::FeatureParam<int> kMacCAOverlayQuadMaxNum;
#endif

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE)
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kCanSkipRenderPassOverlay);
#endif

VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kDrawPredictedInkPoint);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kAllowUndamagedNonrootRenderPassToSkip);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kAggressiveFrameCulling);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kEagerSurfaceGarbageCollection);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kOverrideThrottledFrameRateParams);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kRendererAllocatesImages);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kBufferQueueImageSetPurgeable);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kEvictSubtree);

VIZ_COMMON_EXPORT extern const char kDraw1Point12Ms[];
VIZ_COMMON_EXPORT extern const char kDraw2Points6Ms[];
VIZ_COMMON_EXPORT extern const char kDraw1Point6Ms[];
VIZ_COMMON_EXPORT extern const char kDraw2Points3Ms[];
VIZ_COMMON_EXPORT extern const char kPredictorKalman[];
VIZ_COMMON_EXPORT extern const char kPredictorLinearResampling[];
VIZ_COMMON_EXPORT extern const char kPredictorLinear1[];
VIZ_COMMON_EXPORT extern const char kPredictorLinear2[];
VIZ_COMMON_EXPORT extern const char kPredictorLsq[];

VIZ_COMMON_EXPORT bool IsSimpleFrameRateThrottlingEnabled();
#if BUILDFLAG(IS_ANDROID)
VIZ_COMMON_EXPORT bool IsDynamicColorGamutEnabled();
#endif
VIZ_COMMON_EXPORT bool IsOverlayPrioritizationEnabled();
VIZ_COMMON_EXPORT bool IsDelegatedCompositingEnabled();
VIZ_COMMON_EXPORT bool IsSyncWindowDestructionEnabled();
VIZ_COMMON_EXPORT bool IsUsingVizFrameSubmissionForWebView();
VIZ_COMMON_EXPORT bool IsUsingPreferredIntervalForVideo();
VIZ_COMMON_EXPORT bool ShouldUseRealBuffersForPageFlipTest();
VIZ_COMMON_EXPORT bool ShouldWebRtcLogCapturePipeline();
#if BUILDFLAG(IS_WIN)
VIZ_COMMON_EXPORT bool ShouldUseSetPresentDuration();
#endif  // BUILDFLAG(IS_WIN)y
VIZ_COMMON_EXPORT absl::optional<int> ShouldDrawPredictedInkPoints();
VIZ_COMMON_EXPORT std::string InkPredictor();
VIZ_COMMON_EXPORT bool ShouldUsePlatformDelegatedInk();
VIZ_COMMON_EXPORT bool UseSurfaceLayerForVideo();
#if BUILDFLAG(IS_ANDROID)
VIZ_COMMON_EXPORT bool UseRealVideoColorSpaceForDisplay();
#endif
VIZ_COMMON_EXPORT absl::optional<double> IsDynamicSchedulerEnabledForDraw();
VIZ_COMMON_EXPORT absl::optional<double> IsDynamicSchedulerEnabledForClients();
VIZ_COMMON_EXPORT int MaxOverlaysConsidered();
VIZ_COMMON_EXPORT bool ShouldVideoDetectorIgnoreNonVideoFrames();
VIZ_COMMON_EXPORT bool ShouldOverrideThrottledFrameRateParams();
VIZ_COMMON_EXPORT bool ShouldRendererAllocateImages();

}  // namespace features

#endif  // COMPONENTS_VIZ_COMMON_FEATURES_H_
