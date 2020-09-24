// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_FEATURES_H_
#define COMPONENTS_VIZ_COMMON_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "components/viz/common/viz_common_export.h"


namespace features {

VIZ_COMMON_EXPORT extern const base::Feature kForcePreferredIntervalForVideo;
VIZ_COMMON_EXPORT extern const base::Feature kUseSkiaRenderer;
VIZ_COMMON_EXPORT extern const base::Feature kRecordSkPicture;
VIZ_COMMON_EXPORT extern const base::Feature kDisableDeJelly;
#if defined(OS_ANDROID)
VIZ_COMMON_EXPORT extern const base::Feature kDynamicColorGamut;
#endif
VIZ_COMMON_EXPORT extern const base::Feature kVizForWebView;
VIZ_COMMON_EXPORT extern const base::Feature kVizFrameSubmissionForWebView;
VIZ_COMMON_EXPORT extern const base::Feature kUsePreferredIntervalForVideo;
VIZ_COMMON_EXPORT extern const base::Feature kUseRealBuffersForPageFlipTest;
#if defined(OS_FUCHSIA)
VIZ_COMMON_EXPORT extern const base::Feature kUseSkiaOutputDeviceBufferQueue;
#endif
VIZ_COMMON_EXPORT extern const base::Feature kWebRtcLogCapturePipeline;
#if defined(OS_WIN)
VIZ_COMMON_EXPORT extern const base::Feature kUseSetPresentDuration;
#endif  // OS_WIN

VIZ_COMMON_EXPORT bool IsForcePreferredIntervalForVideoEnabled();
VIZ_COMMON_EXPORT bool IsVizHitTestingDebugEnabled();
VIZ_COMMON_EXPORT bool IsUsingSkiaRenderer();
#if defined(OS_ANDROID)
VIZ_COMMON_EXPORT bool IsDynamicColorGamutEnabled();
#endif
VIZ_COMMON_EXPORT bool IsUsingVizForWebView();
VIZ_COMMON_EXPORT bool IsUsingVizFrameSubmissionForWebView();
VIZ_COMMON_EXPORT bool IsUsingPreferredIntervalForVideo();
VIZ_COMMON_EXPORT int NumOfFramesToToggleInterval();
VIZ_COMMON_EXPORT bool ShouldUseRealBuffersForPageFlipTest();
VIZ_COMMON_EXPORT bool ShouldWebRtcLogCapturePipeline();
#if defined(OS_WIN)
VIZ_COMMON_EXPORT bool ShouldUseSetPresentDuration();
#endif  // OS_WIN

}  // namespace features

#endif  // COMPONENTS_VIZ_COMMON_FEATURES_H_
