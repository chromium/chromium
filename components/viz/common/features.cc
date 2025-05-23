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
#include "base/android/build_info.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif  // BUILDFLAG(IS_WIN)

namespace features {

#if BUILDFLAG(IS_ANDROID)
// During a scroll, enable viz to move browser controls according to the
// offsets provided by the embedded renderer, circumventing browser main
// involvement. For now, this applies only to top controls.
BASE_FEATURE(kAndroidBrowserControlsInViz,
             "AndroidBrowserControlsInViz",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If this flag is enabled, AndroidBrowserControlsInViz and
// BottomControlsRefactor with the "Dispatch yOffset" variation must also be
// enabled.
BASE_FEATURE(kAndroidBcivBottomControls,
             "AndroidBcivBottomControls",
             base::FEATURE_ENABLED_BY_DEFAULT);

#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kBackdropFilterMirrorEdgeMode,
             "BackdropFilterMirrorEdgeMode",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUseDrmBlackFullscreenOptimization,
             "UseDrmBlackFullscreenOptimization",
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kUseFrameIntervalDecider,
             "UseFrameIntervalDecider",
             base::FEATURE_ENABLED_BY_DEFAULT
);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kUseFrameIntervalDeciderAdaptiveFrameRate,
             "UseFrameIntervalDeciderAdaptiveFrameRate",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kTemporalSkipOverlaysWithRootCopyOutputRequests,
             "TemporalSkipOverlaysWithRootCopyOutputRequests",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUseMultipleOverlays,
             "UseMultipleOverlays",
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);
const char kMaxOverlaysParam[] = "max_overlays";

BASE_FEATURE(kDelegatedCompositing,
             "DelegatedCompositing",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAvoidDuplicateDelayBeginFrame,
             "AvoidDuplicateDelayBeginFrame",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kDrawQuadSplit[] = "num_of_splits";

// If enabled, overrides the maximum number (exclusive) of quads one draw quad
// can be split into during occlusion culling.
BASE_FEATURE(kDrawQuadSplitLimit,
             "DrawQuadSplitLimit",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
BASE_FEATURE(kDCompSurfacesForDelegatedInk,
             "DCompSurfacesForDelegatedInk",
             base::FEATURE_ENABLED_BY_DEFAULT);

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
BASE_FEATURE(kRemoveRedirectionBitmap,
             "RemoveRedirectionBitmap",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

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

// If enabled and the device's SOC manufacturer satisifes the allowlist and
// blocklist rules, WebView reports the set of threads involved in frame
// production to HWUI, and they're included in the HWUI ADPF session.
// If disabled, WebView never uses ADPF.
// The allowlist takes precedence - i.e. if the allowlist is non-empty, the
// soc must be in the allowlist for WebView to use ADPF, and the blocklist is
// ignored. If there's no allowlist, the soc must be absent from the blocklist.
BASE_FEATURE(kWebViewEnableADPF,
             "WebViewEnableADPF",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kWebViewADPFSocManufacturerAllowlist{
    &kWebViewEnableADPF, "webview_soc_manufacturer_allowlist", "Google"};

const base::FeatureParam<std::string> kWebViewADPFSocManufacturerBlocklist{
    &kWebViewEnableADPF, "webview_soc_manufacturer_blocklist", ""};

// If enabled, Renderer Main is included in the set of threads reported to the
// HWUI. This feature works only when WebViewEnableADPF is enabled, otherwise
// this is a no-op.
BASE_FEATURE(kWebViewEnableADPFRendererMain,
             "WebViewEnableADPFRendererMain",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the GPU Main thread is included in the set of threads reported
// to the HWUI. This feature works only when WebViewEnableADPF is enabled,
// otherwise this is a no-op.
BASE_FEATURE(kWebViewEnableADPFGpuMain,
             "WebViewEnableADPFGpuMain",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

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

#if BUILDFLAG(IS_MAC)
// Whether the presentation should be delayed until the next CVDisplayLink
// callback.
BASE_FEATURE(kVSyncAlignedPresent,
             "VSyncAlignedPresent",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Present the frame at next VSync only if this frame handles interaction or
// animation as described in kTargetForVSync. Three finch experiment groups for
// kVSyncAlignedPresent.
constexpr const char kTargetForVSyncAllFrames[] = "AllFrames";
constexpr const char kTargetForVSyncAnimation[] = "Animation";
constexpr const char kTargetForVSyncInteraction[] = "Interaction";
const base::FeatureParam<std::string> kTargetForVSync{
    &kVSyncAlignedPresent, "Target", kTargetForVSyncAllFrames};
#endif

BASE_FEATURE(kAllowUndamagedNonrootRenderPassToSkip,
             "AllowUndamagedNonrootRenderPassToSkip",
#if BUILDFLAG(IS_MAC)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// If enabled, complex occluders are generated for quads with rounded corners,
BASE_FEATURE(kComplexOccluderForQuadsWithRoundedCorners,
             "ComplexOccluderForQuadsWithRoundedCorners",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Allow SurfaceAggregator to merge render passes when they contain quads that
// require overlay (e.g. protected video). See usage in |EmitSurfaceContent|.
BASE_FEATURE(kAllowForceMergeRenderPassWithRequireOverlayQuads,
             "AllowForceMergeRenderPassWithRequireOverlayQuads",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// If enabled, Chrome uses ADPF(Android Dynamic Performance Framework) if the
// device's SOC manufacturer satisifes the allowlist and blocklist rules.
// If disabled, Chrome never uses ADPF.
// The allowlist takes precedence - i.e. if the allowlist is non-empty, the
// soc must be in the allowlist for Chrome to use ADPF, and the blocklist is
// ignored. If there's no allowlist, the soc must be absent from the blocklist.
BASE_FEATURE(kAdpf, "Adpf", base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kADPFSocManufacturerAllowlist{
    &kAdpf, "soc_manufacturer_allowlist", "Google"};

const base::FeatureParam<std::string> kADPFSocManufacturerBlocklist{
    &kAdpf, "soc_manufacturer_blocklist", ""};

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

// If enabled, Chrome's ADPF(Android Dynamic Performance Framework) hint
// session includes Renderer threads only if:
// - The Renderer is handling an interacton
// - The Renderer is the main frame's Renderer, and there no Renderers handling
//   an interaction.
BASE_FEATURE(kEnableInteractiveOnlyADPFRenderer,
             "EnableInteractiveOnlyADPFRenderer",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, Chrome includes the Compositor GPU Thread into the
// ADPF(Android Dynamic Performance Framework) hint session, instead
// of the GPU Main Thread.
BASE_FEATURE(kEnableADPFGpuCompositorThread,
             "EnableADPFGpuCompositorThread",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, Chrome puts Renderer Main threads into a separate
// ADPF(Android Dynamic Performance Framework) hint session, and does not
// report any timing hints from this session.
BASE_FEATURE(kEnableADPFSeparateRendererMainSession,
             "EnableADPFSeparateRendererMainSession",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, Chrome uses SetThreads instead of recreating an
// ADPF(Android Dynamic Performance Framework) hint session when the set of
// threads in the session changes.
BASE_FEATURE(kEnableADPFSetThreads,
             "EnableADPFSetThreads",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, surface activation and draw do not block on dependencies.
BASE_FEATURE(kDrawImmediatelyWhenInteractive,
             "DrawImmediatelyWhenInteractive",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, we immediately send acks to clients when a viz surface
// activates. This effectively removes back-pressure. This can result in wasted
// work and contention, but should regularize the timing of client rendering.
BASE_FEATURE(kAckOnSurfaceActivationWhenInteractive,
             "AckOnSurfaceActivationWhenInteractive",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<int>
    kNumCooldownFramesForAckOnSurfaceActivationDuringInteraction{
        &kAckOnSurfaceActivationWhenInteractive, "frames", 3};

// When enabled, SDR maximum luminance nits of then current display will be used
// as the HDR metadata NDWL nits.
BASE_FEATURE(kUseDisplaySDRMaxLuminanceNits,
             "UseDisplaySDRMaxLuminanceNits",
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

// Remove gpu process reference if gpu context is loss, and gpu channel cannot
// be established due to said gpu process exiting.
BASE_FEATURE(kShutdownForFailedChannelCreation,
             "ShutdownForFailedChannelCreation",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// Stops BeginFrame issue to use |last_vsync_interval_| instead of the current
// set of BeginFrameArgs.
// TODO(b/333940735): Should be removed if the issue isn't fixed.
BASE_FEATURE(kLastVSyncArgsKillswitch,
             "LastVSyncArgsKillswitch",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Null Hypothesis test for viz. This will be used in an meta experiment to
// judge finch variation.
BASE_FEATURE(kVizNullHypothesis,
             "VizNullHypothesis",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Treat frame rates of 72hz as if they were 90Hz for buffer sizing purposes.
BASE_FEATURE(kUse90HzSwapChainCountFor72fps,
             "Use90HzSwapChainCountFor72fps",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_CHROMEOS)
// Allows the display to seamlessly adjust the refresh rate in order to match
// content preferences. ChromeOS only.
BASE_FEATURE(kCrosContentAdjustedRefreshRate,
             "CrosContentAdjustedRefreshRate",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

int DrawQuadSplitLimit() {
  constexpr int kDefaultDrawQuadSplitLimit = 5;
  constexpr int kMinDrawQuadSplitLimit = 1;
  constexpr int kMaxDrawQuadSplitLimit = 15;

  const int split_limit = base::GetFieldTrialParamByFeatureAsInt(
      kDrawQuadSplitLimit, kDrawQuadSplit, kDefaultDrawQuadSplitLimit);
  return std::clamp(split_limit, kMinDrawQuadSplitLimit,
                    kMaxDrawQuadSplitLimit);
}

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

bool IsComplexOccluderForQuadsWithRoundedCornersEnabled() {
  static bool enabled = base::FeatureList::IsEnabled(
      features::kComplexOccluderForQuadsWithRoundedCorners);
  return enabled;
}

bool ShouldDrawImmediatelyWhenInteractive() {
  return base::FeatureList::IsEnabled(
      features::kDrawImmediatelyWhenInteractive);
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

bool IsUsingFrameIntervalDecider() {
  return base::FeatureList::IsEnabled(kUseFrameIntervalDecider);
}

#if BUILDFLAG(IS_MAC)
bool IsVSyncAlignedPresentEnabled() {
  return base::FeatureList::IsEnabled(features::kVSyncAlignedPresent);
}
#endif

#if BUILDFLAG(IS_CHROMEOS)
bool IsCrosContentAdjustedRefreshRateEnabled() {
  if (base::FeatureList::IsEnabled(kCrosContentAdjustedRefreshRate)) {
    if (base::FeatureList::IsEnabled(kUseFrameIntervalDecider)) {
      return true;
    }

    LOG(WARNING) << "Feature ContentAdjustedRefreshRate is ignored. It cannot "
                    "be used without also setting UseFrameIntervalDecider.";
  }

  return false;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
bool ShouldRemoveRedirectionBitmap() {
  // Limit to Win11 because there are a high number of D3D9 users on Win10;
  // which requires the Redirection Bitmap. Additionally, software GL in tests
  // can take the Swiftshader rendering path, which also needs the Redirection
  // Bitmap. On devices with DComp disabled, ANGLE draws to the redirection
  // bitmap via a blit swap chain, so check for the command line switch as well.
  return base::win::GetVersion() >= base::win::Version::WIN11 &&
         base::FeatureList::IsEnabled(kRemoveRedirectionBitmap) &&
         !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kOverrideUseSoftwareGLForTests) &&
         !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kDisableDirectComposition);
}
#endif

#if BUILDFLAG(IS_ANDROID)
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

}  // namespace features
