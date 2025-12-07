// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/features.h"

#include "base/feature_list.h"
#include "components/input/input_constants.h"

namespace input::features {

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kInputOnViz, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kUseAndroidBufferedInputDispatch,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether to transfer the sequences to Viz which have down time after event
// time.
const base::FeatureParam<bool> kTransferSequencesWithAbnormalDownTime{
    &features::kInputOnViz,
    /*name=*/"transfer_sequences_with_abnormal_down_time", false};

// Whether to forward the events that were seen by Browser to Viz.
const base::FeatureParam<bool> kForwardEventsSeenOnBrowserToViz{
    &features::kInputOnViz,
    /*name=*/"forward_events_seen_on_browser_to_viz", false};
#endif

BASE_FEATURE(kDispatchSingleEventIfNoPrediction,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLogBubblingTouchscreenGesturesForDebug,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

// Flag guard for fix for crbug.com/346629231.
BASE_FEATURE(kIgnoreBubblingCollisionIfSourceDevicesMismatch,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Flag guard for fix for crbug.com/346629231.
BASE_FEATURE(kScrollBubblingFix, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUpdateScrollPredictorInputMapping,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(bool,
                   kGenerateSyntheticScrollPrediction,
                   &kUpdateScrollPredictorInputMapping,
                   "generate_synthetic_scroll",
                   true);

// Flag guard for renderer hang watcher \ hang monitor.
BASE_FEATURE(kRendererHangWatcher, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(
    base::TimeDelta,
    kRendererHangWatcherDelay,
    &kRendererHangWatcher,
    "delay",
    input::kHungRendererDelay  // Default value in input_constants.h
);

// Flag guard for unresponsive renderer multiple stack collection attempts.
BASE_FEATURE(kUnresponsiveMultipleStackCollection,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kUnresponsiveMultipleStackCollectionDelay,
                   &kUnresponsiveMultipleStackCollection,
                   "delay",
                   base::Milliseconds(100));
BASE_FEATURE_PARAM(size_t,
                   kUnresponsiveMultipleStackCollectionCount,
                   &kUnresponsiveMultipleStackCollection,
                   "count",
                   5);

}  // namespace input::features
