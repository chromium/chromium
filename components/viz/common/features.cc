// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/features.h"

#include "base/command_line.h"
#include "base/system/sys_info.h"
#include "build/chromecast_buildflags.h"
#include "components/viz/common/switches.h"
#include "components/viz/common/viz_utils.h"
#include "gpu/config/gpu_finch_features.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace features {

const base::Feature kForcePreferredIntervalForVideo{
    "ForcePreferredIntervalForVideo", base::FEATURE_DISABLED_BY_DEFAULT};

// Use the SkiaRenderer.
#if defined(OS_WIN) || \
    (defined(OS_LINUX) && !(defined(OS_CHROMEOS) || BUILDFLAG(IS_CHROMECAST)))
const base::Feature kUseSkiaRenderer{"UseSkiaRenderer",
                                     base::FEATURE_ENABLED_BY_DEFAULT};
#else
const base::Feature kUseSkiaRenderer{"UseSkiaRenderer",
                                     base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Kill-switch to disable de-jelly, even if flags/properties indicate it should
// be enabled.
const base::Feature kDisableDeJelly{"DisableDeJelly",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_ANDROID)
// When wide color gamut content from the web is encountered, promote our
// display to wide color gamut if supported.
const base::Feature kDynamicColorGamut{"DynamicColorGamut",
                                       base::FEATURE_ENABLED_BY_DEFAULT};
#endif

// Viz for WebView architecture.
const base::Feature kVizForWebView{"VizForWebView",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Submit CompositorFrame from SynchronousLayerTreeFrameSink directly to viz in
// WebView.
const base::Feature kVizFrameSubmissionForWebView{
    "VizFrameSubmissionForWebView", base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_ANDROID)
const base::Feature kUsePreferredIntervalForVideo{
    "UsePreferredIntervalForVideo", base::FEATURE_DISABLED_BY_DEFAULT};
#else
const base::Feature kUsePreferredIntervalForVideo{
    "UsePreferredIntervalForVideo", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

// Whether we should use the real buffers corresponding to overlay candidates in
// order to do a pageflip test rather than allocating test buffers.
const base::Feature kUseRealBuffersForPageFlipTest{
    "UseRealBuffersForPageFlipTest", base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_FUCHSIA)
// Enables SkiaOutputDeviceBufferQueue instead of Vulkan swapchain on Fuchsia.
const base::Feature kUseSkiaOutputDeviceBufferQueue{
    "UseSkiaOutputDeviceBufferQueue", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Whether we should log extra debug information to webrtc native log.
const base::Feature kWebRtcLogCapturePipeline{
    "WebRtcLogCapturePipeline", base::FEATURE_DISABLED_BY_DEFAULT};

// The number of frames to wait before toggling to a lower frame rate.
const base::FeatureParam<int> kNumOfFramesToToggleInterval{
    &kUsePreferredIntervalForVideo, "NumOfFramesToToggleInterval", 6};

#if defined(OS_WIN)
// Enables swap chains to call SetPresentDuration to request DWM/OS to reduce
// vsync.
const base::Feature kUseSetPresentDuration{"UseSetPresentDuration",
                                           base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // OS_WIN

bool IsForcePreferredIntervalForVideoEnabled() {
  return base::FeatureList::IsEnabled(kForcePreferredIntervalForVideo);
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

  return base::FeatureList::IsEnabled(kUseSkiaRenderer) ||
         base::FeatureList::IsEnabled(kVulkan);
}

#if defined(OS_ANDROID)
bool IsDynamicColorGamutEnabled() {
  if (viz::AlwaysUseWideColorGamut())
    return false;
  auto* build_info = base::android::BuildInfo::GetInstance();
  if (!build_info->is_at_least_q())
    return false;
  return base::FeatureList::IsEnabled(kDynamicColorGamut);
}
#endif

bool IsUsingVizForWebView() {
  // Viz for WebView requires shared images to be enabled.
  if (!base::FeatureList::IsEnabled(kEnableSharedImageForWebview))
    return false;

  return base::FeatureList::IsEnabled(kVizForWebView);
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
  return IsForcePreferredIntervalForVideoEnabled() ||
         base::FeatureList::IsEnabled(kUsePreferredIntervalForVideo);
}

int NumOfFramesToToggleInterval() {
  return kNumOfFramesToToggleInterval.Get();
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
