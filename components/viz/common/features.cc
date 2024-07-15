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

namespace features {

#if BUILDFLAG(IS_ANDROID)
// During a scroll, enable viz to move browser controls according to the
// offsets provided by the embedded renderer, circumventing browser main
// involvement. For now, this applies only to top controls.
BASE_FEATURE(kAndroidBrowserControlsInViz,
             "AndroidBrowserControlsInViz",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kBackdropFilterMirrorEdgeMode,
             "BackdropFilterMirrorEdgeMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUseDrmBlackFullscreenOptimization,
             "UseDrmBlackFullscreenOptimization",
#if BUILDFLAG(IS_CHROMEOS_ASH)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

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

#if BUILDFLAG(IS_WIN)
// Enable partially delegated compositing. In this mode, the web contents will
// be forced into its own render pass instead of merging into the root pass.
// This effectively makes it so only the browser UI quads get delegated
// compositing.
// TODO(crbug.com/324460866): Consider removing partially delegated compositing.
BASE_FEATURE(kDelegatedCompositingLimitToUi,
             "DelegatedCompositingLimitToUi",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// If enabled, the overlay processor will force the use of dcomp surfaces as the
// render pass backing while delegated ink is being employed. This will avoid
// the need for finding what surface to synchronize ink updates with by making
// all surfaces synchronize with dcomp commit
BASE_FEATURE(kUseDCompSurfacesForDelegatedInk,
             "UseDCompSurfacesForDelegatedInk",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kRenderPassDrawnRect,
             "RenderPassDrawnRect",
#if BUILDFLAG(IS_CHROMEOS_LACROS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

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

// If enabled, WebView reports the set of threads involved in frame production
// to HWUI, and they're included in the HWUI ADPF session.
BASE_FEATURE(kWebViewEnableADPF,
             "WebViewEnableADPF",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, Renderer Main is included in the set of threads reported to the
// HWUI. This feature works only when WebViewEnableADPF is enabled, otherwise
// this is a no-op.
BASE_FEATURE(kWebViewEnableADPFRendererMain,
             "WebViewEnableADPFRendererMain",
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
             base::FEATURE_DISABLED_BY_DEFAULT);
// Set FeatureParam default to -1. CALayerOverlayProcessor choose the default in
// ca_layer_overlay.cc When it's < 0.
const base::FeatureParam<int> kCALayerNewLimitDefault{&kCALayerNewLimit,
                                                      "default", -1};
const base::FeatureParam<int> kCALayerNewLimitManyVideos{&kCALayerNewLimit,
                                                         "many-videos", -1};
#endif

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_WIN)
BASE_FEATURE(kCanSkipRenderPassOverlay,
             "CanSkipRenderPassOverlay",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_MAC)
// Use the system CVDisplayLink callbacks for the BeginFrame source, so
// BeginFrame is aligned with HW VSync.
BASE_FEATURE(kCVDisplayLinkBeginFrameSource,
             "CVDisplayLinkBeginFrameSource",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether the presentation should be delayed until the next CVDisplayLink
// callback when kCVDisplayLinkBeginFrameSource is enabled. This flag has no
// effect if kCVDisplayLinkBeginFrameSource is disabled.
BASE_FEATURE(kVSyncAlignedPresent,
             "VSyncAlignedPresent",
             base::FEATURE_DISABLED_BY_DEFAULT);

// The paramters for the number of supported pending Frames.
// 1: Support one pending frame. This is the old default.
// 2: Support two pending frames. New. This is the number of max pending
//    swap in the scheduler.
// Others: Error! It will be overwritten to 2 pending frames.
const base::FeatureParam<int> kNumPendingFrames{&kVSyncAlignedPresent,
                                                "PendingFrames", 2};
#endif

BASE_FEATURE(kAllowUndamagedNonrootRenderPassToSkip,
             "AllowUndamagedNonrootRenderPassToSkip",
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS_LACROS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Allow SurfaceAggregator to merge render passes when they contain quads that
// require overlay (e.g. protected video). See usage in |EmitSurfaceContent|.
BASE_FEATURE(kAllowForceMergeRenderPassWithRequireOverlayQuads,
             "AllowForceMergeRenderPassWithRequireOverlayQuads",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// On platforms using SkiaOutputDeviceBufferQueue and not yet universally using
// SkiaRenderer-allocated images, when this is true SkiaRenderer will allocate
// and maintain a buffer queue of images for the root render pass, instead of
// SkiaOutputDeviceBufferQueue itself.
BASE_FEATURE(kRendererAllocatesImages,
             "RendererAllocatesImages",
             base::FEATURE_ENABLED_BY_DEFAULT
);
#endif

// If enabled, CompositorFrameSinkClient::OnBeginFrame is also treated as the
// DidReceiveCompositorFrameAck. Both in providing the Ack for the previous
// frame, and in returning resources. While enabled we attempt to not send
// separate Ack and ReclaimResources signals. However if while sending an
// OnBeginFrame there is a pending Ack, then if the Ack arrives before the next
// OnBeginFrame we will send the Ack immediately, rather than batching it.
BASE_FEATURE(kOnBeginFrameAcks,
             "OnBeginFrameAcks",
#if BUILDFLAG(IS_CHROMEOS_LACROS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// if enabled, Any CompositorFrameSink of type video that defines a preferred
// framerate that is below the display framerate will throttle OnBeginFrame
// callbacks to match the preferred framerate.
BASE_FEATURE(kOnBeginFrameThrottleVideo,
             "OnBeginFrameThrottleVideo",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
             );

BASE_FEATURE(kSharedBitmapToSharedImage,
             "SharedBitmapToSharedImage",
             base::FEATURE_ENABLED_BY_DEFAULT);
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

// If enabled, Chrome includes the Renderer Main thread(s) into the
// ADPF(Android Dynamic Performance Framework) hint session.
BASE_FEATURE(kEnableADPFRendererMain,
             "EnableADPFRendererMain",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, Chrome includes the Compositor GPU Thread into the
// ADPF(Android Dynamic Performance Framework) hint session, instead
// of the GPU Main Thread.
BASE_FEATURE(kEnableADPFGpuCompositorThread,
             "EnableADPFGpuCompositorThread",
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

// When enabled, SDR maximum luminance nits of then current display will be used
// as the HDR metadata NDWL nits.
BASE_FEATURE(kUseDisplaySDRMaxLuminanceNits,
             "UseDisplaySDRMaxLuminanceNits",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Invalidate the `viz::LocalSurfaceId` on the browser side when the page is
// navigated away. This flag serves as the kill-switch for the uncaught edge
// cases in production.
BASE_FEATURE(kInvalidateLocalSurfaceIdPreCommit,
             "InvalidateLocalSurfaceIdPreCommit",
             base::FEATURE_ENABLED_BY_DEFAULT);

// On mac, when the RenderWidgetHostViewMac is hidden, also hide the
// DelegatedFrameHost. Among other things, it unlocks the compositor frames,
// which can saves hundreds of MiB of memory with bfcache entries.
BASE_FEATURE(kHideDelegatedFrameHostMac,
             "HideDelegatedFrameHostMac",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, ClientResourceProvider will attempt to unlock and delete
// TransferableResources that have been returned as a part of eviction.
//
// Enabled by default 03/2014, kept to run a holdback experiment.
BASE_FEATURE(kEvictionUnlocksResources,
             "EvictionUnlocksResources",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, FrameRateDecider will toggle to half framerate if there's only
// one video on screen whose framerate is lower than the display vsync and in
// perfect cadence.
BASE_FEATURE(kSingleVideoFrameRateThrottling,
             "SingleVideoFrameRateThrottling",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, ClientResourceProvider will take callbacks intended to be ran
// on the Main-thread, and will batch them into a single jump to that thread.
// Rather than each performing its own separate post task.
//
// Enabled 03/2024, kept to run a holdback experiment.
BASE_FEATURE(kBatchMainThreadReleaseCallbacks,
             "BatchMainThreadReleaseCallbacks",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, snapshot the root surface when it is evicted.
BASE_FEATURE(kSnapshotEvictedRootSurface,
             "SnapshotEvictedRootSurface",
// TODO(edcourtney): Enable for Android.
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

// If enabled, info for quads from the last render pass will be reported as
// UMAs.
BASE_FEATURE(kShouldLogFrameQuadInfo,
             "ShouldLogFrameQuadInfo",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, ClientResourceProvider will allow for the batching of
// callbacks. So that the client can perform a series of individual releases,
// but have ClientResourceProvider coordinate the callbacks. This allows all of
// the Main-thread callbacks to be batched into a single jump to that thread.
//
// When disabled each callback will perform its own separate post task.
BASE_FEATURE(kBatchResourceRelease,
             "BatchResourceRelease",
             base::FEATURE_DISABLED_BY_DEFAULT);

// The scale to use for root surface snapshots on eviction. See
// `kSnapshotEvictedRootSurface`.
const base::FeatureParam<double> kSnapshotEvictedRootSurfaceScale{
    &kSnapshotEvictedRootSurface, "scale", 0.4};

// Do HDR color conversion per render pass update rect in renderer instead of
// inserting a separate color conversion pass during surface aggregation.
BASE_FEATURE(kColorConversionInRenderer,
             "ColorConversionInRenderer",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Stops BeginFrame issue to use |last_vsync_interval_| instead of the current
// set of BeginFrameArgs.
// TODO(b/333940735): Should be removed if the issue isn't fixed.
BASE_FEATURE(kLastVSyncArgsKillswitch,
             "LastVSyncArgsKillswitch",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Use BlitRequests for copy requests made by ViewTransition.
BASE_FEATURE(kBlitRequestsForViewTransition,
             "BlitRequestsForViewTransition",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsDelegatedCompositingEnabled() {
  return base::FeatureList::IsEnabled(kDelegatedCompositing);
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

bool ShouldWebRtcLogCapturePipeline() {
  return base::FeatureList::IsEnabled(kWebRtcLogCapturePipeline);
}

#if BUILDFLAG(IS_WIN)
bool ShouldUseSetPresentDuration() {
  return base::FeatureList::IsEnabled(kUseSetPresentDuration);
}
#endif  // BUILDFLAG(IS_WIN)

std::optional<int> ShouldDrawPredictedInkPoints() {
  if (!base::FeatureList::IsEnabled(kDrawPredictedInkPoint))
    return std::nullopt;

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

  NOTREACHED_IN_MIGRATION();
  return std::nullopt;
}

std::string InkPredictor() {
  if (!base::FeatureList::IsEnabled(kDrawPredictedInkPoint))
    return "";

  return GetFieldTrialParamValueByFeature(kDrawPredictedInkPoint, "predictor");
}

#if BUILDFLAG(IS_ANDROID)
bool UseWebViewNewInvalidateHeuristic() {
  // For Android TVs we bundle this with WebViewSurfaceControlForTV.
  if (base::android::BuildInfo::GetInstance()->is_tv()) {
    return base::FeatureList::IsEnabled(kWebViewSurfaceControlForTV);
  }

  return base::FeatureList::IsEnabled(kWebViewNewInvalidateHeuristic);
}
#endif

bool UseSurfaceLayerForVideo() {
#if BUILDFLAG(IS_ANDROID)
  // SurfaceLayer video should work fine with new heuristic.
  if (UseWebViewNewInvalidateHeuristic()) {
    return true;
  }

  // Allow enabling UseSurfaceLayerForVideo if webview is using surface control.
  if (::features::IsAndroidSurfaceControlEnabled()) {
    return true;
  }
  return base::FeatureList::IsEnabled(kUseSurfaceLayerForVideoDefault);
#else
  return true;
#endif
}

int MaxOverlaysConsidered() {
  if (!base::FeatureList::IsEnabled(kUseMultipleOverlays)) {
    return 1;
  }

  return base::GetFieldTrialParamByFeatureAsInt(kUseMultipleOverlays,
                                                kMaxOverlaysParam, 8);
}

bool ShouldOnBeginFrameThrottleVideo() {
  return base::FeatureList::IsEnabled(features::kOnBeginFrameThrottleVideo);
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
bool ShouldRendererAllocateImages() {
  return base::FeatureList::IsEnabled(kRendererAllocatesImages);
}
#endif

bool IsOnBeginFrameAcksEnabled() {
  return base::FeatureList::IsEnabled(features::kOnBeginFrameAcks);
}

bool ShouldDrawImmediatelyWhenInteractive() {
  return base::FeatureList::IsEnabled(
      features::kDrawImmediatelyWhenInteractive);
}

std::optional<double> SnapshotEvictedRootSurfaceScale() {
  if (!base::FeatureList::IsEnabled(kSnapshotEvictedRootSurface)) {
    return std::nullopt;
  }
  return kSnapshotEvictedRootSurfaceScale.Get();
}

bool ShouldLogFrameQuadInfo() {
  return base::FeatureList::IsEnabled(features::kShouldLogFrameQuadInfo);
}

#if BUILDFLAG(IS_MAC)
bool IsCVDisplayLinkBeginFrameSourceEnabled() {
  return base::FeatureList::IsEnabled(features::kCVDisplayLinkBeginFrameSource);
}

bool IsVSyncAlignedPresentEnabled() {
  return base::FeatureList::IsEnabled(features::kVSyncAlignedPresent);
}

int NumPendingFrameSupported() {
  // Return the old default if this feature is not enabled.
  if (!base::FeatureList::IsEnabled(kVSyncAlignedPresent)) {
    return 1;
  }

  // Unless 1 pending frame is specified, overwrite all other params to the new
  // default, 2 pending frames.
  int num = kNumPendingFrames.Get() == 1 ? 1 : 2;
  return num;
}
#endif
}  // namespace features
