// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/features.h"

#include "base/command_line.h"
#include "base/system/sys_info.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/common/switches.h"
#include "components/viz/common/viz_utils.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_switches.h"
#include "media/media_buildflags.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace features {

const base::Feature kEnableOverlayPrioritization {
  "EnableOverlayPrioritization",
#if BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Use the SkiaRenderer.
const base::Feature kUseSkiaRenderer {
  "UseSkiaRenderer",
#if defined(OS_WIN) || defined(OS_ANDROID) || BUILDFLAG(IS_CHROMEOS_LACROS) || \
    (defined(OS_LINUX) && !BUILDFLAG(IS_CHROMECAST))
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

// Viz for WebView architecture.
const base::Feature kVizForWebView{"VizForWebView",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// We use this feature for default value, because enabled VizForWebView forces
// skia renderer on and we want to have different feature state between webview
// and chrome. This one is set by webview, while the above can be set via finch.
const base::Feature kVizForWebViewDefault{"VizForWebViewDefault",
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
    "UseSkiaOutputDeviceBufferQueue", base::FEATURE_DISABLED_BY_DEFAULT};
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

// Used to debug Android WebView Vulkan composite. Composite to an intermediate
// buffer and draw the intermediate buffer to the secondary command buffer.
const base::Feature kWebViewVulkanIntermediateBuffer{
    "WebViewVulkanIntermediateBuffer", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsOverlayPrioritizationEnabled() {
  return base::FeatureList::IsEnabled(kEnableOverlayPrioritization);
}

bool IsVizHitTestingDebugEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableVizHitTestDebug);
}

bool IsUsingSkiaRenderer() {
#if defined(OS_ANDROID)
  // We don't support KitKat. Check for it before looking at the feature flag
  // so that KitKat doesn't show up in Control or Enabled experiment group.
  if (base::android::BuildInfo::GetInstance()->sdk_int() <=
      base::android::SDK_VERSION_KITKAT)
    return false;
#endif

  // Viz for webview requires SkiaRenderer.
  if (IsUsingVizForWebView())
    return true;

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

bool IsUsingVizForWebView() {
  // Viz for WebView requires shared images to be enabled.
  if (!base::FeatureList::IsEnabled(kEnableSharedImageForWebview))
    return false;

  // Vulkan on WebView requires viz.
  if (features::IsUsingVulkan())
    return true;

  // If the feature is overridden from command line or finch we will use this
  // value. If not we check for different state that is altered in
  // aw_main_delegate.cc.
  base::FeatureList* feature_list = base::FeatureList::GetInstance();
  if (feature_list && feature_list->IsFeatureOverridden(kVizForWebView.name))
    return base::FeatureList::IsEnabled(kVizForWebView);

  return base::FeatureList::IsEnabled(kVizForWebViewDefault);
}

bool IsUsingVizFrameSubmissionForWebView() {
  if (base::FeatureList::IsEnabled(kVizFrameSubmissionForWebView)) {
    DCHECK(IsUsingVizForWebView())
        << "kVizFrameSubmissionForWebView requires kVizForWebView";
    return true;
  }
  return false;
}

bool IsUsingPreferredIntervalForVideo() {
  return base::FeatureList::IsEnabled(kUsePreferredIntervalForVideo);
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
}  // namespace features
