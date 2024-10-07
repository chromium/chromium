// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_FEATURES_H_
#define COMPONENTS_VIZ_COMMON_FEATURES_H_

#include <optional>
#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "components/viz/common/viz_common_export.h"

// See the following for guidance on adding new viz feature flags:
// https://cs.chromium.org/chromium/src/components/viz/README.md#runtime-features

namespace features {

#if BUILDFLAG(IS_ANDROID)
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kAndroidBcivPhoneOnly);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kAndroidBcivWithSimpleScheduler);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kAndroidBcivWithSuppression);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kAndroidBcivZeroBrowserFrames);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kAndroidBrowserControlsInViz);
#endif  // BUILDFLAG(IS_ANDROID)
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kBackdropFilterMirrorEdgeMode);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kDelegatedCompositing);

#if BUILDFLAG(IS_CHROMEOS_ASH)
VIZ_COMMON_EXPORT extern const char kDrawQuadSplit[];
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kDrawQuadSplitLimit);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

enum class DelegatedCompositingMode {
  // Enable delegated compositing.
  kFull,
#if BUILDFLAG(IS_WIN)
  // Enable partially delegated compositing. In this mode, the web contents will
  // be forced into its own render pass instead of merging into the root pass.
  // This effectively makes it so only the browser UI quads get delegated
  // compositing.
  kLimitToUi,
#endif
};
extern const VIZ_COMMON_EXPORT base::FeatureParam<DelegatedCompositingMode>
    kDelegatedCompositingModeParam;

#if BUILDFLAG(IS_WIN)
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kDCompSurfacesForDelegatedInk);
#endif
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kRenderPassDrawnRect);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kRecordSkPicture);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kUseDrmBlackFullscreenOptimization);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kUseFrameIntervalDecider);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kTemporalSkipOverlaysWithRootCopyOutputRequests);
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
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebViewVulkanIntermediateBuffer);
#if BUILDFLAG(IS_ANDROID)
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kUseSurfaceLayerForVideoDefault);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebViewEnableADPF);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebViewEnableADPFRendererMain);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kWebViewFrameRateHints);
#endif
#if BUILDFLAG(IS_APPLE)
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kCALayerNewLimit);
VIZ_COMMON_EXPORT extern const base::FeatureParam<int> kCALayerNewLimitDefault;
VIZ_COMMON_EXPORT extern const base::FeatureParam<int>
    kCALayerNewLimitManyVideos;
#endif

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_WIN)
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kCanSkipRenderPassOverlay);
#endif

#if BUILDFLAG(IS_MAC)
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kCVDisplayLinkBeginFrameSource);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kVSyncAlignedPresent);
#endif

VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kDrawPredictedInkPoint);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kAllowUndamagedNonrootRenderPassToSkip);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(
    kAllowForceMergeRenderPassWithRequireOverlayQuads);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kOnBeginFrameAcks);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kOnBeginFrameThrottleVideo);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kAdpf);
VIZ_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kADPFSocManufacturerAllowlist;
VIZ_COMMON_EXPORT extern const base::FeatureParam<std::string>
    kADPFSocManufacturerBlocklist;
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kEnableADPFScrollBoost);
VIZ_COMMON_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kADPFBoostTimeout;
VIZ_COMMON_EXPORT extern const base::FeatureParam<double>
    kADPFMidFrameBoostDurationMultiplier;
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kEnableADPFRendererMain);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kEnableADPFGpuCompositorThread);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kEnableADPFAsyncThreadsVerification);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kUseDisplaySDRMaxLuminanceNits);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kHideDelegatedFrameHostMac);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kEvictionUnlocksResources);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kSingleVideoFrameRateThrottling);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kBatchMainThreadReleaseCallbacks);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kColorConversionInRenderer);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kLastVSyncArgsKillswitch);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kBlitRequestsForViewTransition);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kVizNullHypothesis);
#if BUILDFLAG(IS_CHROMEOS_ASH)
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kCrosContentAdjustedRefreshRate);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

VIZ_COMMON_EXPORT extern const char kDraw1Point12Ms[];
VIZ_COMMON_EXPORT extern const char kDraw2Points6Ms[];
VIZ_COMMON_EXPORT extern const char kDraw1Point6Ms[];
VIZ_COMMON_EXPORT extern const char kDraw2Points3Ms[];
VIZ_COMMON_EXPORT extern const char kPredictorKalman[];
VIZ_COMMON_EXPORT extern const char kPredictorLinearResampling[];
VIZ_COMMON_EXPORT extern const char kPredictorLinear1[];
VIZ_COMMON_EXPORT extern const char kPredictorLinear2[];
VIZ_COMMON_EXPORT extern const char kPredictorLsq[];

VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kDrawImmediatelyWhenInteractive);
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kAckOnSurfaceActivationWhenInteractive);

VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kShutdownForFailedChannelCreation);

VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kSnapshotEvictedRootSurface);
VIZ_COMMON_EXPORT extern const base::FeatureParam<double>
    kSnapshotEvictedRootSurfaceScale;
VIZ_COMMON_EXPORT BASE_DECLARE_FEATURE(kBatchResourceRelease);

#if BUILDFLAG(IS_ANDROID)
VIZ_COMMON_EXPORT bool IsDynamicColorGamutEnabled();
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
VIZ_COMMON_EXPORT int DrawQuadSplitLimit();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
VIZ_COMMON_EXPORT bool IsDelegatedCompositingEnabled();
#if BUILDFLAG(IS_WIN)
VIZ_COMMON_EXPORT bool ShouldUseDCompSurfacesForDelegatedInk();
#endif
VIZ_COMMON_EXPORT bool IsUsingVizFrameSubmissionForWebView();
VIZ_COMMON_EXPORT bool IsUsingPreferredIntervalForVideo();
VIZ_COMMON_EXPORT bool ShouldWebRtcLogCapturePipeline();
VIZ_COMMON_EXPORT bool ShouldDrawPredictedInkPoints();
VIZ_COMMON_EXPORT bool UseWebViewNewInvalidateHeuristic();
VIZ_COMMON_EXPORT bool UseSurfaceLayerForVideo();
VIZ_COMMON_EXPORT int MaxOverlaysConsidered();
VIZ_COMMON_EXPORT bool ShouldOnBeginFrameThrottleVideo();
VIZ_COMMON_EXPORT bool IsOnBeginFrameAcksEnabled();
VIZ_COMMON_EXPORT bool ShouldDrawImmediatelyWhenInteractive();
VIZ_COMMON_EXPORT std::optional<double> SnapshotEvictedRootSurfaceScale();
VIZ_COMMON_EXPORT bool IsCVDisplayLinkBeginFrameSourceEnabled();
VIZ_COMMON_EXPORT bool IsVSyncAlignedPresentEnabled();
VIZ_COMMON_EXPORT int NumPendingFrameSupported();
VIZ_COMMON_EXPORT bool ShouldLogFrameQuadInfo();
VIZ_COMMON_EXPORT bool IsUsingFrameIntervalDecider();
VIZ_COMMON_EXPORT std::optional<uint64_t>
NumCooldownFramesForAckOnSurfaceActivationDuringInteraction();
VIZ_COMMON_EXPORT extern const base::FeatureParam<int>
    kNumCooldownFramesForAckOnSurfaceActivationDuringInteraction;
VIZ_COMMON_EXPORT bool ShouldAckOnSurfaceActivationWhenInteractive();
#if BUILDFLAG(IS_CHROMEOS_ASH)
VIZ_COMMON_EXPORT bool IsCrosContentAdjustedRefreshRateEnabled();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_ANDROID)
VIZ_COMMON_EXPORT bool IsBrowserControlsInVizEnabled();
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace features

#endif  // COMPONENTS_VIZ_COMMON_FEATURES_H_
