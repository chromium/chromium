// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/features.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_split.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "components/viz/common/switches.h"
#include "components/viz/common/viz_utils.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_switches.h"
#include "media/media_buildflags.h"
#include "ui/gl/gl_switches.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif  // BUILDFLAG(IS_WIN)

namespace features {

#if BUILDFLAG(IS_ANDROID)
// If this flag is enabled, only the composited progress bar will be visible,
// and load progress updates will be animated instead of directly snapping to
// the new position. The animation is done in the same manner as BCIV, where
// OffsetTags and OffstTagValues will enable viz to move the progress bar.
BASE_FEATURE(kAndroidAnimatedProgressBarInViz,
             base::FEATURE_DISABLED_BY_DEFAULT);

// During a scroll, enable viz to move browser controls according to the
// offsets provided by the embedded renderer, circumventing browser main
// involvement. For now, this applies only to top controls.
BASE_FEATURE(kAndroidBrowserControlsInViz, base::FEATURE_ENABLED_BY_DEFAULT);

// If this flag is enabled, AndroidBrowserControlsInViz and
// BottomControlsRefactor with the "Dispatch yOffset" variation must also be
// enabled.
BASE_FEATURE(kAndroidBcivBottomControls, base::FEATURE_ENABLED_BY_DEFAULT);

// If this flag is enabled, a DumpWithoutCrashing() is captured when a bad
// state is detected when moving the composited UI. For example, this could
// mean scrolling without a resource, or OffsetTagValues trying to position
// the UI outside of their valid constraints.
BASE_FEATURE(kAndroidDumpForBadCompositedUiState,
             base::FEATURE_DISABLED_BY_DEFAULT);

#endif  // BUILDFLAG(IS_ANDROID)

// When there is a screenshot request against a surface, issue the copy request
// into a shared image.
BASE_FEATURE(kBackForwardTransitionsSameDocSharedImage,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBackdropFilterMirrorEdgeMode, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUseDrmBlackFullscreenOptimization,
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kUseFrameIntervalDeciderAdaptiveFrameRate,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kTemporalSkipOverlaysWithRootCopyOutputRequests,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUseMultipleOverlays,
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);
const char kMaxOverlaysParam[] = "max_overlays";

BASE_FEATURE(kDelegatedCompositing, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAvoidDuplicateDelayBeginFrame, base::FEATURE_DISABLED_BY_DEFAULT);

const char kDrawQuadSplit[] = "num_of_splits";

// If enabled, overrides the maximum number (exclusive) of quads one draw quad
// can be split into during occlusion culling.
BASE_FEATURE(kDrawQuadSplitLimit, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableRenderPassDrawQuadCullingOptimization,
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr base::FeatureParam<DelegatedCompositingMode>::Option
    kDelegatedCompositingModeOption[] = {
        {DelegatedCompositingMode::kFull, "full"},
#if BUILDFLAG(IS_WIN)
        {DelegatedCompositingMode::kLimitToUi, "limit_to_ui"},
#endif
};
const base::FeatureParam<DelegatedCompositingMode>
    kDelegatedCompositingModeParam = {
        &kDelegatedCompositing,
        "mode",
#if BUILDFLAG(IS_WIN)
        // TODO(crbug.com/324460866): Windows does not fully support full
        // delegated compositing.
        DelegatedCompositingMode::kLimitToUi,
#else
        DelegatedCompositingMode::kFull,
#endif
        &kDelegatedCompositingModeOption,
};

#if BUILDFLAG(IS_WIN)
// If enabled, the overlay processor will force the use of dcomp surfaces as the
// render pass backing while delegated ink is being employed. This will avoid
// the need for finding what surface to synchronize ink updates with by making
// all surfaces synchronize with dcomp commit
BASE_FEATURE(kDCompSurfacesForDelegatedInk, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, Chromium will utilize DXGI SwapChains and DComp visuals as the
// software output device rather than GDI bit block transfer to the redirection
// bitmap. Additionally, the redirection bitmap will be removed and replaced
// with the native acrylic background effect on Win11. Since the browser window
// appears before the GPU process is able to draw content into it, the acrylic
// effect gives the user feedback that a window is present and content is
// coming. Without the acrylic effect a transparent window will appear with a 1
// pixel border that eats all mouse clicks; not a good user experience. Further,
// the acylic effect will appear in uncovered regions of the window when the
// user resizes the window.
BASE_FEATURE(kRemoveRedirectionBitmap, base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Submit CompositorFrame from SynchronousLayerTreeFrameSink directly to viz in
// WebView.
BASE_FEATURE(kVizFrameSubmissionForWebView, base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_FUCHSIA)
// Enables SkiaOutputDeviceBufferQueue instead of Vulkan swapchain on Fuchsia.
BASE_FEATURE(kUseSkiaOutputDeviceBufferQueue, base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Whether we should log extra debug information to webrtc native log.
BASE_FEATURE(kWebRtcLogCapturePipeline, base::FEATURE_DISABLED_BY_DEFAULT);

// Used to debug Android WebView Vulkan composite. Composite to an intermediate
// buffer and draw the intermediate buffer to the secondary command buffer.
BASE_FEATURE(kWebViewVulkanIntermediateBuffer,
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// Hardcoded as disabled for WebView to have a different default for
// UseSurfaceLayerForVideo from chrome.
BASE_FEATURE(kUseSurfaceLayerForVideoDefault, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWebViewNewInvalidateHeuristic, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled and the device's SOC manufacturer satisifes the allowlist and
// blocklist rules, WebView reports the set of threads involved in frame
// production to HWUI, and they're included in the HWUI ADPF session.
// If disabled, WebView never uses ADPF.
// The allowlist takes precedence - i.e. if the allowlist is non-empty, the
// soc must be in the allowlist for WebView to use ADPF, and the blocklist is
// ignored. If there's no allowlist, the soc must be absent from the blocklist.
BASE_FEATURE(kWebViewEnableADPF, base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kWebViewADPFSocManufacturerAllowlist{
    &kWebViewEnableADPF, "webview_soc_manufacturer_allowlist", "Google"};

const base::FeatureParam<std::string> kWebViewADPFSocManufacturerBlocklist{
    &kWebViewEnableADPF, "webview_soc_manufacturer_blocklist", ""};

// If enabled, Renderer Main is included in the set of threads reported to the
// HWUI. This feature works only when WebViewEnableADPF is enabled, otherwise
// this is a no-op.
BASE_FEATURE(kWebViewEnableADPFRendererMain, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the GPU Main thread is included in the set of threads reported
// to the HWUI. This feature works only when WebViewEnableADPF is enabled,
// otherwise this is a no-op.
BASE_FEATURE(kWebViewEnableADPFGpuMain, base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_APPLE)
// Increase the max CALayer number allowed for CoreAnimation.
// * If this feature is disabled, then the default limit is 128 quads,
//   unless there are 5 or more video elements present, in which case
//   the limit is 300.
// * If this feature is enabled, then these limits are 512, and can be
// overridden by the "default" and "many-videos"
//   feature parameters.
BASE_FEATURE(kCALayerNewLimit, base::FEATURE_DISABLED_BY_DEFAULT);
// Set FeatureParam default to -1. CALayerOverlayProcessor choose the default in
// ca_layer_overlay.cc When it's < 0.
const base::FeatureParam<int> kCALayerNewLimitDefault{&kCALayerNewLimit,
                                                      "default", -1};
const base::FeatureParam<int> kCALayerNewLimitManyVideos{&kCALayerNewLimit,
                                                         "many-videos", -1};
#endif

#if BUILDFLAG(IS_MAC)
// Whether the presentation should be delayed until the next DisplayLink
// callback. Currently only for frames that handle interaction.
BASE_FEATURE(kVSyncAlignedPresent, base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Sends a CopyOutputRequest completion Ack early for view transitions so it can
// proceed with navigation. ViewTransition Animate still waits though for
// CopyOutputRequests to be actually fulfilled.
BASE_FEATURE(kAckCopyOutputRequestEarlyForViewTransition,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAllowUndamagedNonrootRenderPassToSkip,
#if BUILDFLAG(IS_MAC)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Allow SurfaceAggregator to merge render passes when they contain quads that
// require overlay (e.g. protected video). See usage in |EmitSurfaceContent|.
BASE_FEATURE(kAllowForceMergeRenderPassWithRequireOverlayQuads,
             base::FEATURE_ENABLED_BY_DEFAULT);

// if enabled, Any CompositorFrameSink of type video that defines a preferred
// framerate that is below the display framerate will throttle OnBeginFrame
// callbacks to match the preferred framerate.
BASE_FEATURE(kOnBeginFrameThrottleVideo,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// If enabled, Chrome uses ADPF(Android Dynamic Performance Framework) if the
// device's SOC manufacturer satisifes the allowlist and blocklist rules.
// If disabled, Chrome never uses ADPF.
// The allowlist takes precedence - i.e. if the allowlist is non-empty, the
// soc must be in the allowlist for Chrome to use ADPF, and the blocklist is
// ignored. If there's no allowlist, the soc must be absent from the blocklist.
BASE_FEATURE(kAdpf, base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kADPFSocManufacturerAllowlist{
    &kAdpf, "soc_manufacturer_allowlist", "Google"};

const base::FeatureParam<std::string> kADPFSocManufacturerBlocklist{
    &kAdpf, "soc_manufacturer_blocklist", ""};

// Used to enable the HintSession::Mode::BOOST mode. BOOST mode try to force
// the ADPF(Android Dynamic Performance Framework) to give Chrome more CPU
// resources during a scroll.
BASE_FEATURE(kEnableADPFScrollBoost, base::FEATURE_DISABLED_BY_DEFAULT);

// Specifies how long after the boost mode is set, it will expire.
const base::FeatureParam<base::TimeDelta> kADPFBoostTimeout{
    &kEnableADPFScrollBoost, "adpf_boost_mode_timeout",
    base::Milliseconds(200)};

// If enabled, Chrome's ADPF(Android Dynamic Performance Framework) hint
// session includes Renderer threads only if:
// - The Renderer is handling an interacton
// - The Renderer is the main frame's Renderer, and there no Renderers handling
//   an interaction.
BASE_FEATURE(kEnableInteractiveOnlyADPFRenderer,
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, Chrome puts Renderer Main threads into a separate
// ADPF(Android Dynamic Performance Framework) hint session, and does not
// report any timing hints from this session.
BASE_FEATURE(kEnableADPFSeparateRendererMainSession,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, Chrome uses SetThreads instead of recreating an
// ADPF(Android Dynamic Performance Framework) hint session when the set of
// threads in the session changes.
BASE_FEATURE(kEnableADPFSetThreads, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, Chrome uses notifyWorkloadIncrease ADPF(Android Dynamic
// Performance Framework) method before CrRendererMain starts running a heavy
// workload during page load.
// Supported only on Android >= 16.
BASE_FEATURE(kEnableADPFWorkloadIncreaseOnPageLoad,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, Chrome uses notifyWorkloadReset method on viz wakeup instead of
// sending a timing report with a fake actual duration > target duration.
// Supported only on Android >= 16.
BASE_FEATURE(kEnableADPFWorkloadReset, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, we immediately send acks to clients when a viz surface
// activates. This effectively removes back-pressure. This can result in wasted
// work and contention, but should regularize the timing of client rendering.
BASE_FEATURE(kAckOnSurfaceActivationWhenInteractive,
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<int>
    kNumCooldownFramesForAckOnSurfaceActivationDuringInteraction{
        &kAckOnSurfaceActivationWhenInteractive, "frames", 3};

// When enabled, SDR maximum luminance nits of then current display will be used
// as the HDR metadata NDWL nits.
BASE_FEATURE(kUseDisplaySDRMaxLuminanceNits, base::FEATURE_ENABLED_BY_DEFAULT);

// On mac, when the RenderWidgetHostViewMac is hidden, also hide the
// DelegatedFrameHost. Among other things, it unlocks the compositor frames,
// which can saves hundreds of MiB of memory with bfcache entries.
BASE_FEATURE(kHideDelegatedFrameHostMac, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, ClientResourceProvider will attempt to unlock and delete
// TransferableResources that have been returned as a part of eviction.
//
// Enabled by default 03/2014, kept to run a holdback experiment.
BASE_FEATURE(kEvictionUnlocksResources, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, FrameRateDecider will toggle to half framerate if there's only
// one video on screen whose framerate is lower than the display vsync and in
// perfect cadence.
BASE_FEATURE(kSingleVideoFrameRateThrottling,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Remove gpu process reference if gpu context is loss, and gpu channel cannot
// be established due to said gpu process exiting.
BASE_FEATURE(kShutdownForFailedChannelCreation,
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, info for quads from the last render pass will be reported as
// UMAs.
BASE_FEATURE(kShouldLogFrameQuadInfo, base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, ClientResourceProvider will allow for the batching of
// callbacks. So that the client can perform a series of individual releases,
// but have ClientResourceProvider coordinate the callbacks. This allows all of
// the Main-thread callbacks to be batched into a single jump to that thread.
//
// When disabled each callback will perform its own separate post task.
BASE_FEATURE(kBatchResourceRelease, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, BeginFrameSource will not send a `BeginFrameArgs::MISSED` in
// response to `AddObserver`. As these consistently miss deadlines, and increase
// latency and jank. Instead the client will receive the next BeginFrame.
BASE_FEATURE(kNoLateBeginFrames, base::FEATURE_DISABLED_BY_DEFAULT);

// Stops BeginFrame issue to use |last_vsync_interval_| instead of the current
// set of BeginFrameArgs.
// TODO(b/333940735): Should be removed if the issue isn't fixed.
BASE_FEATURE(kLastVSyncArgsKillswitch, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables IPCs to directly target Viz's compositor thread for non-root
// CompositorFrameSink messages without hopping through the IO thread first.
BASE_FEATURE(kVizDirectCompositorThreadIpcNonRoot,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables IPCs to directly target Viz's compositor thread for FrameSinkManager
// messages and, in turn, all interfaces associated with it e.g. root compositor
// frame sink, display private - skipping the IO thread hop.
BASE_FEATURE(kVizDirectCompositorThreadIpcFrameSinkManager,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Null Hypothesis test for viz. This will be used in an meta experiment to
// judge finch variation.
BASE_FEATURE(kVizNullHypothesis, base::FEATURE_DISABLED_BY_DEFAULT);

// Treat frame rates of 72hz as if they were 90Hz for buffer sizing purposes.
BASE_FEATURE(kUse90HzSwapChainCountFor72fps, base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_CHROMEOS)
// Allows the display to seamlessly adjust the refresh rate in order to match
// content preferences. ChromeOS only.
BASE_FEATURE(kCrosContentAdjustedRefreshRate,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

BASE_FEATURE(kNoCompositorFrameAcks, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int> kNumberPendingFramesUntilThrottle{
    &kNoCompositorFrameAcks, "pending_frames", 1};
BASE_FEATURE(kDisplaySchedulerAsClient, base::FEATURE_DISABLED_BY_DEFAULT);

int DrawQuadSplitLimit() {
  constexpr int kDefaultDrawQuadSplitLimit = 5;
  constexpr int kMinDrawQuadSplitLimit = 1;
  constexpr int kMaxDrawQuadSplitLimit = 15;

  const int split_limit = base::GetFieldTrialParamByFeatureAsInt(
      kDrawQuadSplitLimit, kDrawQuadSplit, kDefaultDrawQuadSplitLimit);
  return std::clamp(split_limit, kMinDrawQuadSplitLimit,
                    kMaxDrawQuadSplitLimit);
}

bool IsRenderPassDrawQuadCullingOptimizationEnabled() {
  static bool is_enabled = base::FeatureList::IsEnabled(
      kEnableRenderPassDrawQuadCullingOptimization);
  return is_enabled;
}

bool IsBackForwardTransitionsSameDocSharedImageEnabled() {
  return base::FeatureList::IsEnabled(
      kBackForwardTransitionsSameDocSharedImage);
}

bool IsDelegatedCompositingEnabled() {
  return base::FeatureList::IsEnabled(kDelegatedCompositing);
}

bool IsVizDirectCompositorThreadIpcNonRootEnabled() {
  return base::FeatureList::IsEnabled(kVizDirectCompositorThreadIpcNonRoot);
}

bool IsVizDirectCompositorThreadIpcFrameSinkManagerEnabled() {
  return base::FeatureList::IsEnabled(
      kVizDirectCompositorThreadIpcFrameSinkManager);
}

bool IsUsingVizFrameSubmissionForWebView() {
  return base::FeatureList::IsEnabled(kVizFrameSubmissionForWebView);
}

bool ShouldWebRtcLogCapturePipeline() {
  return base::FeatureList::IsEnabled(kWebRtcLogCapturePipeline);
}

#if BUILDFLAG(IS_ANDROID)
bool UseWebViewNewInvalidateHeuristic() {
  // For Android TVs we bundle this with WebViewSurfaceControlForTV.
  if (base::android::device_info::is_tv()) {
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

bool ShouldAckOnSurfaceActivationWhenInteractive() {
  return base::FeatureList::IsEnabled(
      features::kAckOnSurfaceActivationWhenInteractive);
}

bool Use90HzSwapChainCountFor72fps() {
  return base::FeatureList::IsEnabled(kUse90HzSwapChainCountFor72fps);
}

std::optional<uint64_t>
NumCooldownFramesForAckOnSurfaceActivationDuringInteraction() {
  if (!ShouldAckOnSurfaceActivationWhenInteractive()) {
    return std::nullopt;
  }
  CHECK_GE(kNumCooldownFramesForAckOnSurfaceActivationDuringInteraction.Get(),
           0)
      << "The number of cooldown frames must be non-negative";
  return static_cast<uint64_t>(
      kNumCooldownFramesForAckOnSurfaceActivationDuringInteraction.Get());
}

bool ShouldLogFrameQuadInfo() {
  return base::FeatureList::IsEnabled(features::kShouldLogFrameQuadInfo);
}

#if BUILDFLAG(IS_MAC)
bool IsVSyncAlignedPresentEnabled() {
  return base::FeatureList::IsEnabled(features::kVSyncAlignedPresent);
}
#endif

#if BUILDFLAG(IS_CHROMEOS)
bool IsCrosContentAdjustedRefreshRateEnabled() {
  return base::FeatureList::IsEnabled(kCrosContentAdjustedRefreshRate);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
bool ShouldRemoveRedirectionBitmap() {
  // Limit to Win11 because there are a high number of D3D9 users on Win10;
  // which requires the redirection bitmap. 22H2 is specified because it is the
  // lowest version supporting DWM system backdrop.
  if (base::win::GetVersion() < base::win::Version::WIN11_22H2) {
    return false;
  }

  const auto* command_line = base::CommandLine::ForCurrentProcess();

  // If direct composition is disabled say for testing, we will use an ANGLE
  // EGLSurface which uses a BitBlt swap chain that needs a redirection surface.
  if (command_line->HasSwitch(switches::kDisableDirectComposition)) {
    return false;
  }

  // When using swiftshader for testing, we will also use an ANGLE EGLSurface.
  if (command_line->HasSwitch(switches::kOverrideUseSoftwareGLForTests)) {
    return false;
  }

  // Some users set ANGLE backend to D3D9 or OpenGL via chrome://flags and in
  // that case too we would also use an ANGLE EGLSurface.
  const std::string angle_backend =
      command_line->GetSwitchValueASCII(switches::kUseANGLE);
  if (angle_backend == gl::kANGLEImplementationD3D9Name ||
      angle_backend == gl::kANGLEImplementationOpenGLName) {
    return false;
  }

  return base::FeatureList::IsEnabled(kRemoveRedirectionBitmap);
}
#endif

#if BUILDFLAG(IS_ANDROID)
bool IsAndroidAnimatedProgressBarInVizEnabled() {
  return base::FeatureList::IsEnabled(
      features::kAndroidAnimatedProgressBarInViz);
}

bool IsBcivBottomControlsEnabled() {
  return base::FeatureList::IsEnabled(features::kAndroidBcivBottomControls);
}

bool IsBrowserControlsInVizEnabled() {
  return base::FeatureList::IsEnabled(features::kAndroidBrowserControlsInViz);
}

bool ShouldUseAdpfForSoc(std::string_view soc_allowlist,
                         std::string_view soc_blocklist,
                         std::string_view soc) {
  std::vector<std::string_view> allowlist = base::SplitStringPiece(
      soc_allowlist, "|", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::string blocklist_param = features::kADPFSocManufacturerBlocklist.Get();
  std::vector<std::string_view> blocklist = base::SplitStringPiece(
      soc_blocklist, "|", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  // If there's no allowlist, soc must be absent from the blocklist.
  if (allowlist.empty()) {
    return !base::Contains(blocklist, soc);
  }
  // If there's an allowlist, soc must be in the allowlist.
  // Blocklist is ignored in this case.
  return base::Contains(allowlist, soc);
}
#endif  // BUILDFLAG(IS_ANDROID)

bool ShouldAckCOREarlyForViewTransition() {
  return base::FeatureList::IsEnabled(
      features::kAckCopyOutputRequestEarlyForViewTransition);
}

}  // namespace features
