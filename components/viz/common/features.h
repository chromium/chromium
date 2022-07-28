// Copyright 2017 The Chromium Authors. All rights reserved.
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

VIZ_COMMON_EXPORT extern const base::Feature kAdpf;
VIZ_COMMON_EXPORT extern const base::FeatureParam<int> kAdpfTargetDurationMs;
VIZ_COMMON_EXPORT extern const base::Feature kEnableOverlayPrioritization;
VIZ_COMMON_EXPORT extern const base::Feature kDelegatedCompositing;
VIZ_COMMON_EXPORT extern const base::Feature kRecordSkPicture;
VIZ_COMMON_EXPORT extern const base::Feature kDisableDeJelly;
VIZ_COMMON_EXPORT extern const base::Feature kUseMultipleOverlays;
VIZ_COMMON_EXPORT extern const char kMaxOverlaysParam[];
VIZ_COMMON_EXPORT extern const base::Feature kVideoDetectorIgnoreNonVideos;
#if BUILDFLAG(IS_ANDROID)
VIZ_COMMON_EXPORT extern const base::Feature kDynamicColorGamut;
#endif
VIZ_COMMON_EXPORT extern const base::Feature kVizFrameSubmissionForWebView;
VIZ_COMMON_EXPORT extern const base::Feature kUseRealBuffersForPageFlipTest;
#if BUILDFLAG(IS_FUCHSIA)
VIZ_COMMON_EXPORT extern const base::Feature kUseSkiaOutputDeviceBufferQueue;
#endif
VIZ_COMMON_EXPORT extern const base::Feature kWebRtcLogCapturePipeline;
#if BUILDFLAG(IS_WIN)
VIZ_COMMON_EXPORT extern const base::Feature kUseSetPresentDuration;
#endif  // BUILDFLAG(IS_WIN)
VIZ_COMMON_EXPORT extern const base::Feature kWebViewVulkanIntermediateBuffer;
VIZ_COMMON_EXPORT extern const base::Feature kUsePlatformDelegatedInk;
#if BUILDFLAG(IS_ANDROID)
VIZ_COMMON_EXPORT extern const base::Feature kUseSurfaceLayerForVideoDefault;
VIZ_COMMON_EXPORT extern const base::Feature kWebViewNewInvalidateHeuristic;
#endif
VIZ_COMMON_EXPORT extern const base::Feature kSurfaceSyncThrottling;
VIZ_COMMON_EXPORT extern const base::Feature kDynamicSchedulerForDraw;
VIZ_COMMON_EXPORT extern const base::Feature kDynamicSchedulerForClients;
#if BUILDFLAG(IS_MAC)
VIZ_COMMON_EXPORT extern const base::Feature kMacCAOverlayQuad;
VIZ_COMMON_EXPORT extern const base::FeatureParam<int> kMacCAOverlayQuadMaxNum;
#endif

VIZ_COMMON_EXPORT extern const base::Feature kDrawPredictedInkPoint;
VIZ_COMMON_EXPORT extern const char kDraw1Point12Ms[];
VIZ_COMMON_EXPORT extern const char kDraw2Points6Ms[];
VIZ_COMMON_EXPORT extern const char kDraw1Point6Ms[];
VIZ_COMMON_EXPORT extern const char kDraw2Points3Ms[];
VIZ_COMMON_EXPORT extern const char kPredictorKalman[];
VIZ_COMMON_EXPORT extern const char kPredictorLinearResampling[];
VIZ_COMMON_EXPORT extern const char kPredictorLinear1[];
VIZ_COMMON_EXPORT extern const char kPredictorLinear2[];
VIZ_COMMON_EXPORT extern const char kPredictorLsq[];

VIZ_COMMON_EXPORT bool IsAdpfEnabled();
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
#endif  // BUILDFLAG(IS_WIN)
VIZ_COMMON_EXPORT absl::optional<int> ShouldDrawPredictedInkPoints();
VIZ_COMMON_EXPORT std::string InkPredictor();
VIZ_COMMON_EXPORT bool ShouldUsePlatformDelegatedInk();
VIZ_COMMON_EXPORT bool UseSurfaceLayerForVideo();
#if BUILDFLAG(IS_ANDROID)
VIZ_COMMON_EXPORT bool UseRealVideoColorSpaceForDisplay();
#endif
VIZ_COMMON_EXPORT bool IsSurfaceSyncThrottling();
VIZ_COMMON_EXPORT absl::optional<double> IsDynamicSchedulerEnabledForDraw();
VIZ_COMMON_EXPORT absl::optional<double> IsDynamicSchedulerEnabledForClients();
VIZ_COMMON_EXPORT int MaxOverlaysConsidered();
VIZ_COMMON_EXPORT bool ShouldVideoDetectorIgnoreNonVideoFrames();

}  // namespace features

#endif  // COMPONENTS_VIZ_COMMON_FEATURES_H_
