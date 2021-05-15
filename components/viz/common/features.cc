// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/features.h"

#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/system/sys_info.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/common/delegated_ink_prediction_configuration.h"
#include "components/viz/common/switches.h"
#include "components/viz/common/viz_utils.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_switches.h"
#include "media/media_buildflags.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace features {

// Enables the use of CPU scheduling APIs on Android.
const base::Feature kAdpf{"Adpf", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableOverlayPrioritization {
  "EnableOverlayPrioritization",
#if BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

const base::Feature kSimpleFrameRateThrottling{
    "SimpleFrameRateThrottling", base::FEATURE_DISABLED_BY_DEFAULT};

// Use the SkiaRenderer.
const base::Feature kUseSkiaRenderer {
  "UseSkiaRenderer",
#if defined(OS_WIN) || defined(OS_ANDROID) || BUILDFLAG(IS_CHROMEOS_LACROS) || \
    defined(OS_LINUX)
      base::FEATURE_ENABLED_BY_DEFAULT
#elif defined(OS_MAC)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Kill-switch to disable de-jelly, even if flags/properties indicate it should
// be enabled.
const base::Feature kDisableDeJelly{"DisableDeJelly",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_ANDROID)
// When wide color gamut content from the web is encountered, promote our
// display to wide color gamut if supported.
const base::Feature kDynamicColorGamut{"DynamicColorGamut",
                                       base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Uses glClear to composite solid color quads whenever possible.
const base::Feature kFastSolidColorDraw{"FastSolidColorDraw",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Submit CompositorFrame from SynchronousLayerTreeFrameSink directly to viz in
// WebView.
const base::Feature kVizFrameSubmissionForWebView{
    "VizFrameSubmissionForWebView", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kUsePreferredIntervalForVideo{
  "UsePreferredIntervalForVideo",
#if defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// Whether we should use the real buffers corresponding to overlay candidates in
// order to do a pageflip test rather than allocating test buffers.
const base::Feature kUseRealBuffersForPageFlipTest{
  "UseRealBuffersForPageFlipTest",
#if BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

#if defined(OS_FUCHSIA)
// Enables SkiaOutputDeviceBufferQueue instead of Vulkan swapchain on Fuchsia.
const base::Feature kUseSkiaOutputDeviceBufferQueue{
    "UseSkiaOutputDeviceBufferQueue", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

// Whether we should log extra debug information to webrtc native log.
const base::Feature kWebRtcLogCapturePipeline{
    "WebRtcLogCapturePipeline", base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_WIN)
// Enables swap chains to call SetPresentDuration to request DWM/OS to reduce
// vsync.
const base::Feature kUseSetPresentDuration{"UseSetPresentDuration",
                                           base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // OS_WIN

#if defined(USE_X11)
// Uses X11 Present Extensions instead of the Vulkan swapchain for presenting.
const base::Feature kUseX11Present{"UseX11Present",
                                   base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enables platform supported delegated ink trails instead of Skia backed
// delegated ink trails.
const base::Feature kUsePlatformDelegatedInk{"UsePlatformDelegatedInk",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Used to debug Android WebView Vulkan composite. Composite to an intermediate
// buffer and draw the intermediate buffer to the secondary command buffer.
const base::Feature kWebViewVulkanIntermediateBuffer{
    "WebViewVulkanIntermediateBuffer", base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_ANDROID)
// Hardcoded as disabled for WebView to have a different default for
// UseSurfaceLayerForVideo from chrome.
const base::Feature kUseSurfaceLayerForVideoDefault{
    "UseSurfaceLayerForVideoDefault", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

bool IsAdpfEnabled() {
  // TODO(crbug.com/1157620): Limit this to correct android version.
  return base::FeatureList::IsEnabled(kAdpf);
}

bool IsOverlayPrioritizationEnabled() {
  return base::FeatureList::IsEnabled(kEnableOverlayPrioritization);
}

// If a synchronous IPC should used when destroying windows. This exists to test
// the impact of removing the sync IPC.
bool IsSyncWindowDestructionEnabled() {
  static constexpr base::Feature kSyncWindowDestruction{
      "SyncWindowDestruction", base::FEATURE_ENABLED_BY_DEFAULT};

  return base::FeatureList::IsEnabled(kSyncWindowDestruction);
}

bool IsSimpleFrameRateThrottlingEnabled() {
  return base::FeatureList::IsEnabled(kSimpleFrameRateThrottling);
}

bool IsUsingSkiaRenderer() {
#if defined(OS_ANDROID)
  // We don't support KitKat. Check for it before looking at the feature flag
  // so that KitKat doesn't show up in Control or Enabled experiment group.
  if (base::android::BuildInfo::GetInstance()->sdk_int() <=
      base::android::SDK_VERSION_KITKAT)
    return false;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(https://crbug.com/1145180): SkiaRenderer isn't supported on Chrome
  // OS boards that still use the legacy video decoder.
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(
          switches::kPlatformDisallowsChromeOSDirectVideoDecoder))
    return false;
#endif

  return base::FeatureList::IsEnabled(kUseSkiaRenderer) ||
         features::IsUsingVulkan();
}

#if defined(OS_ANDROID)
bool IsDynamicColorGamutEnabled() {
  if (viz::AlwaysUseWideColorGamut())
    return false;
  auto* build_info = base::android::BuildInfo::GetInstance();
  if (build_info->sdk_int() < base::android::SDK_VERSION_Q)
    return false;
  return base::FeatureList::IsEnabled(kDynamicColorGamut);
}
#endif

bool IsUsingFastPathForSolidColorQuad() {
  return base::FeatureList::IsEnabled(kFastSolidColorDraw);
}

bool IsUsingVizFrameSubmissionForWebView() {
  return base::FeatureList::IsEnabled(kVizFrameSubmissionForWebView);
}

bool IsUsingPreferredIntervalForVideo() {
  return base::FeatureList::IsEnabled(kUsePreferredIntervalForVideo);
}

bool IsVizHitTestingDebugEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableVizHitTestDebug);
}

bool ShouldUseRealBuffersForPageFlipTest() {
  return base::FeatureList::IsEnabled(kUseRealBuffersForPageFlipTest);
}

bool ShouldWebRtcLogCapturePipeline() {
  return base::FeatureList::IsEnabled(kWebRtcLogCapturePipeline);
}

#if defined(OS_WIN)
bool ShouldUseSetPresentDuration() {
  return base::FeatureList::IsEnabled(kUseSetPresentDuration);
}
#endif  // OS_WIN

absl::optional<int> ShouldDrawPredictedInkPoints() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kDrawPredictedInkPoint))
    return absl::nullopt;

  std::string predicted_points =
      command_line->GetSwitchValueASCII(switches::kDrawPredictedInkPoint);
  if (predicted_points == switches::kDraw1Point12Ms)
    return viz::PredictionConfig::k1Point12Ms;
  else if (predicted_points == switches::kDraw2Points6Ms)
    return viz::PredictionConfig::k2Points6Ms;
  else if (predicted_points == switches::kDraw1Point6Ms)
    return viz::PredictionConfig::k1Point6Ms;
  else if (predicted_points == switches::kDraw2Points3Ms)
    return viz::PredictionConfig::k2Points3Ms;

  NOTREACHED();
  return absl::nullopt;
}

bool ShouldUsePlatformDelegatedInk() {
  return base::FeatureList::IsEnabled(kUsePlatformDelegatedInk);
}

#if defined(OS_ANDROID)
bool UseSurfaceLayerForVideo() {
  // Allow enabling UseSurfaceLayerForVideo if webview is using surface control.
  if (::features::IsAndroidSurfaceControlEnabled()) {
    return true;
  }
  return base::FeatureList::IsEnabled(kUseSurfaceLayerForVideoDefault);
}
#endif

}  // namespace features
