// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/features.h"

#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/common/delegated_ink_prediction_configuration.h"
#include "components/viz/common/switches.h"
#include "components/viz/common/viz_utils.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_switches.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace {

// FieldTrialParams for `DynamicSchedulerForDraw` and
// `kDynamicSchedulerForClients`.
const char kDynamicSchedulerPercentile[] = "percentile";

}  // namespace

namespace features {

BASE_FEATURE(kUseMultipleOverlays,
             "UseMultipleOverlays",
#if BUILDFLAG(IS_CHROMEOS_ASH)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);
const char kMaxOverlaysParam[] = "max_overlays";

BASE_FEATURE(kDelegatedCompositing,
             "DelegatedCompositing",
#if BUILDFLAG(IS_CHROMEOS_LACROS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kVideoDetectorIgnoreNonVideos,
             "VideoDetectorIgnoreNonVideos",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// When wide color gamut content from the web is encountered, promote our
// display to wide color gamut if supported.
BASE_FEATURE(kDynamicColorGamut,
             "DynamicColorGamut",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Submit CompositorFrame from SynchronousLayerTreeFrameSink directly to viz in
// WebView.
BASE_FEATURE(kVizFrameSubmissionForWebView,
             "VizFrameSubmissionForWebView",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether we should use the real buffers corresponding to overlay candidates in
// order to do a pageflip test rather than allocating test buffers.
BASE_FEATURE(kUseRealBuffersForPageFlipTest,
             "UseRealBuffersForPageFlipTest",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_FUCHSIA)
// Enables SkiaOutputDeviceBufferQueue instead of Vulkan swapchain on Fuchsia.
BASE_FEATURE(kUseSkiaOutputDeviceBufferQueue,
             "UseSkiaOutputDeviceBufferQueue",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Whether we should log extra debug information to webrtc native log.
BASE_FEATURE(kWebRtcLogCapturePipeline,
             "WebRtcLogCapturePipeline",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN)
// Enables swap chains to call SetPresentDuration to request DWM/OS to reduce
// vsync.
BASE_FEATURE(kUseSetPresentDuration,
             "UseSetPresentDuration",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)

// Enables platform supported delegated ink trails instead of Skia backed
// delegated ink trails.
BASE_FEATURE(kUsePlatformDelegatedInk,
             "UsePlatformDelegatedInk",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Used to debug Android WebView Vulkan composite. Composite to an intermediate
// buffer and draw the intermediate buffer to the secondary command buffer.
BASE_FEATURE(kWebViewVulkanIntermediateBuffer,
             "WebViewVulkanIntermediateBuffer",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// Hardcoded as disabled for WebView to have a different default for
// UseSurfaceLayerForVideo from chrome.
BASE_FEATURE(kUseSurfaceLayerForVideoDefault,
             "UseSurfaceLayerForVideoDefault",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWebViewNewInvalidateHeuristic,
             "WebViewNewInvalidateHeuristic",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kDrawPredictedInkPoint,
             "DrawPredictedInkPoint",
             base::FEATURE_DISABLED_BY_DEFAULT);
const char kDraw1Point12Ms[] = "1-pt-12ms";
const char kDraw2Points6Ms[] = "2-pt-6ms";
const char kDraw1Point6Ms[] = "1-pt-6ms";
const char kDraw2Points3Ms[] = "2-pt-3ms";
const char kPredictorKalman[] = "kalman";
const char kPredictorLinearResampling[] = "linear-resampling";
const char kPredictorLinear1[] = "linear-1";
const char kPredictorLinear2[] = "linear-2";
const char kPredictorLsq[] = "lsq";

// Used by Viz to parameterize adjustments to scheduler deadlines.
BASE_FEATURE(kDynamicSchedulerForDraw,
             "DynamicSchedulerForDraw",
             base::FEATURE_DISABLED_BY_DEFAULT);
// User to parameterize adjustments to clients' deadlines.
BASE_FEATURE(kDynamicSchedulerForClients,
             "DynamicSchedulerForClients",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_APPLE)
// Increase the max CALayer number allowed for CoreAnimation.
// * If this feature is disabled, then the default limit is 128 quads,
//   unless there are 5 or more video elements present, in which case
//   the limit is 300.
// * If this feature is enabled, then these limits are 512, and can be
// overridden by the "default" and "many-videos"
//   feature parameters.
BASE_FEATURE(kCALayerNewLimit,
             "CALayerNewLimit",
             base::FEATURE_ENABLED_BY_DEFAULT);
// Set FeatureParam default to -1. CALayerOverlayProcessor choose the default in
// ca_layer_overlay.cc When it's < 0.
const base::FeatureParam<int> kCALayerNewLimitDefault{&kCALayerNewLimit,
                                                      "default", -1};
const base::FeatureParam<int> kCALayerNewLimitManyVideos{&kCALayerNewLimit,
                                                         "many-videos", -1};
#endif

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE)
BASE_FEATURE(kCanSkipRenderPassOverlay,
             "CanSkipRenderPassOverlay",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_MAC)
BASE_FEATURE(kCVDisplayLinkBeginFrameSource,
             "CVDisplayLinkBeginFrameSource",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Allow SkiaRenderer to skip drawing render passes that contain a single
// RenderPassDrawQuad.
BASE_FEATURE(kAllowBypassRenderPassQuads,
             "AllowBypassRenderPassQuads",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAllowUndamagedNonrootRenderPassToSkip,
             "AllowUndamagedNonrootRenderPassToSkip",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether to:
// - Perform periodic inactive frame culling.
// - Cull *all* frames in case of critical memory pressure, rather than keeping
//   one.
BASE_FEATURE(kAggressiveFrameCulling,
             "AggressiveFrameCulling",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, do not rely on surface garbage collection to happen
// periodically, but trigger it eagerly, to avoid missing calls.
BASE_FEATURE(kEagerSurfaceGarbageCollection,
             "EagerSurfaceGarbageCollection",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Only applies when a caller has requested a custom BeginFrame rate via the
// Throttle() API in frame_sink_manager.mojom. If enabled, parameters related
// to the BeginFrame rate are overridden in viz to reflect the throttled rate
// before being circulated in the system. The most notable are the interval and
// deadline in BeginFrameArgs. If disabled, these parameters reflect the default
// vsync rate (the behavior at the time this feature was created.)
BASE_FEATURE(kOverrideThrottledFrameRateParams,
             "OverrideThrottledFrameRateParams",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Used to gate calling SetPurgeable on OutputPresenter::Image from
// SkiaOutputDeviceBufferQueue.
BASE_FEATURE(kBufferQueueImageSetPurgeable,
             "BufferQueueImageSetPurgeable",
             base::FEATURE_ENABLED_BY_DEFAULT);

// On platforms using SkiaOutputDeviceBufferQueue, when this is true
// SkiaRenderer will allocate and maintain a buffer queue of images for the root
// render pass, instead of SkiaOutputDeviceBufferQueue itself.
BASE_FEATURE(kRendererAllocatesImages,
             "RendererAllocatesImages",
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// On all platforms when attempting to evict a FrameTree, the active
// viz::Surface can be not included. This feature ensures that the we always add
// the active viz::Surface to the eviction list.
//
// Furthermore, by default on Android, when a client is being evicted, it only
// evicts itself. This differs from Destkop platforms which evict the entire
// FrameTree along with the topmost viz::Surface. When this feature is enabled,
// Android will begin also evicting the entire FrameTree.
BASE_FEATURE(kEvictSubtree, "EvictSubtree", base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, CompositorFrameSinkClient::OnBeginFrame is also treated as the
// DidReceiveCompositorFrameAck. Both in providing the Ack for the previous
// frame, and in returning resources. While enabled the separate Ack and
// ReclaimResources signals will not be sent.
BASE_FEATURE(kOnBeginFrameAcks,
             "OnBeginFrameAcks",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, and kOnBeginFrameAcks is also enabled, then if we issue an
// CompositorFrameSinkClient::OnBeginFrame, while we are pending an Ack. If the
// Ack arrives before the next OnBeginFrame we will send it immediately, instead
// of batching it. This is to support a frame submission/draw that occurs right
// near the OnBeginFrame boundary.
BASE_FEATURE(kOnBeginFrameAllowLateAcks,
             "OnBeginFrameAllowLateAcks",
             base::FEATURE_DISABLED_BY_DEFAULT);

// if enabled, Any CompositorFrameSink of type video that defines a preferred
// framerate that is below the display framerate will throttle OnBeginFrame
// callbacks to match the preferred framerate.
BASE_FEATURE(kOnBeginFrameThrottleVideo,
             "OnBeginFrameThrottleVideo",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSharedBitmapToSharedImage,
             "SharedBitmapToSharedImage",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Used to enable the HintSession::Mode::BOOST mode. BOOST mode try to force
// the ADPF(Android Dynamic Performance Framework) to give Chrome more CPU
// resources during a scroll.
BASE_FEATURE(kEnableADPFScrollBoost,
             "EnableADPFScrollBoost",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Specifies how long after the boost mode is set, it will expire.
const base::FeatureParam<base::TimeDelta> kADPFBoostTimeout{
    &kEnableADPFScrollBoost, "adpf_boost_mode_timeout",
    base::Milliseconds(200)};

// If enabled, Chrome uses ADPF(Android Dynamic Performance Framework) to
// request more CPU resources in the middle of a frame production if the frame
// is taking longer than expected.
BASE_FEATURE(kEnableADPFMidFrameBoost,
             "EnableADPFMidFrameBoost",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allows delegating transforms over Wayland when it is also supported by Ash.
BASE_FEATURE(kDelegateTransforms,
             "DelegateTransforms",
#if BUILDFLAG(IS_CHROMEOS_LACROS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// The deadline for requesting a boost in the middle of a frame production is
// this multiplier * ADPF target_duration.
const base::FeatureParam<double> kADPFMidFrameBoostDurationMultiplier{
    &kEnableADPFMidFrameBoost, "adpf_mid_frame_boost_multiplier", 1.0};

// If enabled, Chrome includes the Renderer Main thread(s) into the
// ADPF(Android Dynamic Performance Framework) hint session.
BASE_FEATURE(kEnableADPFRendererMain,
             "EnableADPFRendererMain",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, surface activation and draw do not block on dependencies.
BASE_FEATURE(kDrawImmediatelyWhenInteractive,
             "DrawImmediatelyWhenInteractive",
#if BUILDFLAG(IS_IOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Invalidate the `viz::LocalSurfaceId` on the browser side when the page is
// navigated away. This flag serves as the kill-switch for the uncaught edge
// cases in production.
BASE_FEATURE(kInvalidateLocalSurfaceIdPreCommit,
             "InvalidateLocalSurfaceIdPreCommit",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsDelegatedCompositingEnabled() {
  return base::FeatureList::IsEnabled(kDelegatedCompositing);
}

bool ShouldDelegateTransforms() {
  return base::FeatureList::IsEnabled(features::kDelegateTransforms);
}

#if BUILDFLAG(IS_ANDROID)
bool IsDynamicColorGamutEnabled() {
  if (viz::AlwaysUseWideColorGamut())
    return false;
  auto* build_info = base::android::BuildInfo::GetInstance();
  if (build_info->sdk_int() < base::android::SDK_VERSION_Q)
    return false;
  return base::FeatureList::IsEnabled(kDynamicColorGamut);
}
#endif

bool IsUsingVizFrameSubmissionForWebView() {
  return base::FeatureList::IsEnabled(kVizFrameSubmissionForWebView);
}

bool ShouldUseRealBuffersForPageFlipTest() {
  return base::FeatureList::IsEnabled(kUseRealBuffersForPageFlipTest);
}

bool ShouldWebRtcLogCapturePipeline() {
  return base::FeatureList::IsEnabled(kWebRtcLogCapturePipeline);
}

#if BUILDFLAG(IS_WIN)
bool ShouldUseSetPresentDuration() {
  return base::FeatureList::IsEnabled(kUseSetPresentDuration);
}
#endif  // BUILDFLAG(IS_WIN)

absl::optional<int> ShouldDrawPredictedInkPoints() {
  if (!base::FeatureList::IsEnabled(kDrawPredictedInkPoint))
    return absl::nullopt;

  std::string predicted_points = GetFieldTrialParamValueByFeature(
      kDrawPredictedInkPoint, "predicted_points");
  if (predicted_points == kDraw1Point12Ms)
    return viz::PredictionConfig::k1Point12Ms;
  else if (predicted_points == kDraw2Points6Ms)
    return viz::PredictionConfig::k2Points6Ms;
  else if (predicted_points == kDraw1Point6Ms)
    return viz::PredictionConfig::k1Point6Ms;
  else if (predicted_points == kDraw2Points3Ms)
    return viz::PredictionConfig::k2Points3Ms;

  NOTREACHED();
  return absl::nullopt;
}

std::string InkPredictor() {
  if (!base::FeatureList::IsEnabled(kDrawPredictedInkPoint))
    return "";

  return GetFieldTrialParamValueByFeature(kDrawPredictedInkPoint, "predictor");
}

bool ShouldUsePlatformDelegatedInk() {
  return base::FeatureList::IsEnabled(kUsePlatformDelegatedInk);
}

bool UseSurfaceLayerForVideo() {
#if BUILDFLAG(IS_ANDROID)
  // SurfaceLayer video should work fine with new heuristic.
  if (base::FeatureList::IsEnabled(kWebViewNewInvalidateHeuristic))
    return true;

  // Allow enabling UseSurfaceLayerForVideo if webview is using surface control.
  if (::features::IsAndroidSurfaceControlEnabled()) {
    return true;
  }
  return base::FeatureList::IsEnabled(kUseSurfaceLayerForVideoDefault);
#else
  return true;
#endif
}

// Used by Viz to determine if viz::DisplayScheduler should dynamically adjust
// its frame deadline. Returns the percentile of historic draw times to base the
// deadline on. Or absl::nullopt if the feature is disabled.
absl::optional<double> IsDynamicSchedulerEnabledForDraw() {
  if (!base::FeatureList::IsEnabled(kDynamicSchedulerForDraw))
    return absl::nullopt;
  double result = base::GetFieldTrialParamByFeatureAsDouble(
      kDynamicSchedulerForDraw, kDynamicSchedulerPercentile, -1.0);
  if (result < 0.0)
    return absl::nullopt;
  return result;
}

// Used by Viz to determine if the frame deadlines provided to CC should be
// dynamically adjusted. Returns the percentile of historic draw times to base
// the deadline on. Or absl::nullopt if the feature is disabled.
absl::optional<double> IsDynamicSchedulerEnabledForClients() {
  if (!base::FeatureList::IsEnabled(kDynamicSchedulerForClients))
    return absl::nullopt;
  double result = base::GetFieldTrialParamByFeatureAsDouble(
      kDynamicSchedulerForClients, kDynamicSchedulerPercentile, -1.0);
  if (result < 0.0)
    return absl::nullopt;
  return result;
}

int MaxOverlaysConsidered() {
  if (!base::FeatureList::IsEnabled(kUseMultipleOverlays)) {
    return 1;
  }

  return base::GetFieldTrialParamByFeatureAsInt(kUseMultipleOverlays,
                                                kMaxOverlaysParam, 8);
}

bool ShouldVideoDetectorIgnoreNonVideoFrames() {
  return base::FeatureList::IsEnabled(kVideoDetectorIgnoreNonVideos);
}

bool ShouldOverrideThrottledFrameRateParams() {
  return base::FeatureList::IsEnabled(kOverrideThrottledFrameRateParams);
}

bool ShouldOnBeginFrameThrottleVideo() {
  return base::FeatureList::IsEnabled(features::kOnBeginFrameThrottleVideo);
}

bool ShouldRendererAllocateImages() {
  return base::FeatureList::IsEnabled(kRendererAllocatesImages);
}

bool IsOnBeginFrameAcksEnabled() {
  return base::FeatureList::IsEnabled(features::kOnBeginFrameAcks);
}

bool ShouldDrawImmediatelyWhenInteractive() {
  return base::FeatureList::IsEnabled(
      features::kDrawImmediatelyWhenInteractive);
}

}  // namespace features
