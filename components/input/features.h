// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_FEATURES_H_
#define COMPONENTS_INPUT_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace input::features {

#if BUILDFLAG(IS_ANDROID)

// If enabled, touch input on Android is handled on Viz process. The feature is
// still in development and might not have any functional effects yet.
// Design doc for InputVizard project for moving touch input to viz on Android:
// https://docs.google.com/document/d/1mcydbkgFCO_TT9NuFE962L8PLJWT2XOfXUAPO88VuKE
COMPONENT_EXPORT(INPUT) BASE_DECLARE_FEATURE(kInputOnViz);
COMPONENT_EXPORT(INPUT)
extern const base::FeatureParam<bool> kTransferSequencesWithAbnormalDownTime;
COMPONENT_EXPORT(INPUT)
extern const base::FeatureParam<bool> kForwardEventsSeenOnBrowserToViz;

// If enabled, Chrome will receive buffered/batched input from Android.
// Specifically, Chrome will NOT call
// https://developer.android.com/reference/kotlin/android/view/View#requestunbuffereddispatch.
// If |kInputOnViz| is also enabled, Chrome will call
// https://developer.android.com/ndk/reference/group/native-activity#ainputreceiver_createbatchedinputreceiver
// instead of the default
// https://developer.android.com/ndk/reference/group/native-activity#ainputreceiver_createunbatchedinputreceiver.
COMPONENT_EXPORT(INPUT) BASE_DECLARE_FEATURE(kUseAndroidBufferedInputDispatch);

#endif

// When enabled, if a synthetic scroll prediction cannot be generated (e.g.,
// due to insufficient history), the next event from the queue is dispatched
// directly without resampling. This ensures that input is not stalled when
// prediction fails.
COMPONENT_EXPORT(INPUT)
BASE_DECLARE_FEATURE(kDispatchSingleEventIfNoPrediction);

COMPONENT_EXPORT(INPUT)
BASE_DECLARE_FEATURE(kLogBubblingTouchscreenGesturesForDebug);
COMPONENT_EXPORT(INPUT)
BASE_DECLARE_FEATURE(kIgnoreBubblingCollisionIfSourceDevicesMismatch);
COMPONENT_EXPORT(INPUT) BASE_DECLARE_FEATURE(kScrollBubblingFix);

COMPONENT_EXPORT(INPUT) BASE_DECLARE_FEATURE(kRendererHangWatcher);
COMPONENT_EXPORT(INPUT)
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kRendererHangWatcherDelay);

// Updates the scroll predictor's input mapping and behavior. When enabled:
// 1. It uses `sample_time` (VSync time - 5ms) as the boundary for a frame's
//    events and "looks ahead" at the next event to improve prediction.
// 2. It generates a synthetic scroll event if the queue is empty, keeping
//    scrolling smooth even if input events are missed.
// This feature depends on kRefactorCompositorThreadEventQueue being enabled
// to function correctly, as the refactored event queue logic is necessary
// for the new predictor input mapping and synthetic event generation.
COMPONENT_EXPORT(INPUT)
BASE_DECLARE_FEATURE(kUpdateScrollPredictorInputMapping);
COMPONENT_EXPORT(INPUT)
BASE_DECLARE_FEATURE_PARAM(bool, kGenerateSyntheticScrollPrediction);

COMPONENT_EXPORT(INPUT)
BASE_DECLARE_FEATURE(kUnresponsiveMultipleStackCollection);
COMPONENT_EXPORT(INPUT)
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                           kUnresponsiveMultipleStackCollectionDelay);
COMPONENT_EXPORT(INPUT)
BASE_DECLARE_FEATURE_PARAM(size_t, kUnresponsiveMultipleStackCollectionCount);

}  // namespace input::features

#endif  // COMPONENTS_INPUT_FEATURES_H_
