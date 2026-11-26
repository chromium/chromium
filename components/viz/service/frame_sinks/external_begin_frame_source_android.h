// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_EXTERNAL_BEGIN_FRAME_SOURCE_ANDROID_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_EXTERNAL_BEGIN_FRAME_SOURCE_ANDROID_H_

#include <jni.h>

#include <memory>
#include <optional>

#include "base/android/jni_weak_ref.h"
#include "base/time/time.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

// An implementation of ExternalBeginFrameSource which is driven by VSync
// signals coming from org.chromium.ui.VSyncMonitor.
class VIZ_SERVICE_EXPORT ExternalBeginFrameSourceAndroid
    : public ExternalBeginFrameSource,
      public ExternalBeginFrameSourceClient {
 public:
  // Outcome of `AChoreographerImpl::CalculateDeadlineDerivedInterval()`,
  // together with the reason why a deadline-derived VSync interval was not
  // calculated, was left unsnapped, or was snapped to a display-supported or
  // OS-provided VSync interval.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(DeadlineDerivedIntervalResult)
  enum class DeadlineDerivedIntervalResult {
    // -------------------------------------------------------------------------
    // Result: `std::nullopt` (no deadline-derived VSync interval calculated).
    // -------------------------------------------------------------------------

    // Reason: Android SDK version is older than Android 16 (Baklava, API 36),
    // where frame timeline deltas may not reflect the actual VSync interval
    // (crbug.com/529839989).
    kNotCalculatedUnsupportedAndroidVersion = 0,

    // Reason: The OS doesn't support `AChoreographer_postVsyncCallback` (i.e.
    // `FrameCallback64` was used and `possible_deadlines` is empty).
    kNotCalculatedTimelinesNotSupported = 1,

    // Reason: The OS provided only one frame timeline to `VsyncCallback`.
    kNotCalculatedOnlyOneTimeline = 2,

    // Reason: `features::kCalculateDeadlineDerivedInterval` was disabled.
    kNotCalculatedFeatureDisabled = 3,

    // Reason: The difference between the first two frame timelines'
    // presentation deltas (`present_deltas_difference`) was less than 1 ms,
    // implying a refresh rate above 1 kHz.
    //
    // Note: `present_deltas_difference_us` IS still populated in the trace
    // event for debugging.
    kNotCalculatedIntervalTooShort = 4,

    // -------------------------------------------------------------------------
    // Result: Unsnapped `present_deltas_difference`.
    // -------------------------------------------------------------------------

    // Reason: `features::kCalculateDeadlineDerivedIntervalSnapToleranceParam`
    // is non-positive (snapping disabled).
    //
    // Note: `closest_supported_interval_us` is NOT populated in the trace event
    // because Chrome skips searching display-supported intervals and comparing
    // against the OS-provided interval.
    kCalculatedUnsnappedZeroSnapTolerance = 5,

    // Reason: `features::kCalculateDeadlineDerivedIntervalSnapToleranceParam`
    // is positive, Chrome has no display-supported VSync intervals (from
    // `SetSupportedRefreshRates()` / `Display.getSupportedModes()`), and
    // `features::kCalculateDeadlineDerivedIntervalSnapToOsProvidedParam` is
    // false.
    kCalculatedUnsnappedNoSnapCandidates = 6,

    // Reason: All enabled snap candidates (display-supported VSync intervals
    // and, if `kCalculateDeadlineDerivedIntervalSnapToOsProvidedParam` is true,
    // the OS-provided VSync interval) were outside
    // `features::kCalculateDeadlineDerivedIntervalSnapToleranceParam` (> 0) of
    // `present_deltas_difference`.
    kCalculatedUnsnappedOutsideSnapTolerance = 7,

    // -------------------------------------------------------------------------
    // Result: Snapped VSync interval (display-supported or OS-provided).
    // -------------------------------------------------------------------------

    // Reason: The closest display-supported VSync interval was within
    // `features::kCalculateDeadlineDerivedIntervalSnapToleranceParam`, while
    // the OS-provided VSync interval was either outside
    // `features::kCalculateDeadlineDerivedIntervalSnapToleranceParam` or not
    // considered at all
    // (`features::kCalculateDeadlineDerivedIntervalSnapToOsProvidedParam` was
    // false).
    kCalculatedSnappedToSupportedOsProvidedDisabledOrOutsideSnapTolerance = 8,

    // Reason: `kCalculateDeadlineDerivedIntervalSnapToOsProvidedParam` is true,
    // Chrome had no display-supported VSync intervals, and the OS-provided
    // VSync interval was within
    // `features::kCalculateDeadlineDerivedIntervalSnapToleranceParam`.
    kCalculatedSnappedToOsProvidedNoSupportedIntervals = 9,

    // Reason: `kCalculateDeadlineDerivedIntervalSnapToOsProvidedParam` is true,
    // the closest display-supported VSync interval was outside
    // `features::kCalculateDeadlineDerivedIntervalSnapToleranceParam`, and the
    // OS-provided VSync interval was within
    // `features::kCalculateDeadlineDerivedIntervalSnapToleranceParam`.
    kCalculatedSnappedToOsProvidedSupportedOutsideSnapTolerance = 10,

    // Reason: `kCalculateDeadlineDerivedIntervalSnapToOsProvidedParam` is true,
    // both the closest display-supported VSync interval and the OS-provided
    // VSync interval were within
    // `features::kCalculateDeadlineDerivedIntervalSnapToleranceParam`, and both
    // intervals were equal.
    kCalculatedSnappedToEqualSupportedAndOsProvided = 11,

    // Reason: `kCalculateDeadlineDerivedIntervalSnapToOsProvidedParam` is true,
    // both the closest display-supported VSync interval and the OS-provided
    // VSync interval were within
    // `features::kCalculateDeadlineDerivedIntervalSnapToleranceParam`, and the
    // display-supported interval was strictly closer to
    // `present_deltas_difference`.
    kCalculatedSnappedToSupportedCloserThanOsProvided = 12,

    // Reason: `kCalculateDeadlineDerivedIntervalSnapToOsProvidedParam` is true,
    // both the closest display-supported VSync interval and the OS-provided
    // VSync interval were within
    // `features::kCalculateDeadlineDerivedIntervalSnapToleranceParam`, and the
    // OS-provided interval was closer to (or equidistant from)
    // `present_deltas_difference`.
    kCalculatedSnappedToOsProvidedCloserOrTiedWithSupported = 13,

    kMaxValue = kCalculatedSnappedToOsProvidedCloserOrTiedWithSupported,
  };
  // LINT.ThenChange(
  //   //tools/metrics/histograms/metadata/android/enums.xml:DeadlineDerivedIntervalResult,
  //   //base/tracing/protos/chrome_track_event.proto:DeadlineDerivedIntervalResult
  // )

  ExternalBeginFrameSourceAndroid(uint32_t restart_id,
                                  float refresh_rate,
                                  bool requires_align_with_java);

  ExternalBeginFrameSourceAndroid(const ExternalBeginFrameSourceAndroid&) =
      delete;
  ExternalBeginFrameSourceAndroid& operator=(
      const ExternalBeginFrameSourceAndroid&) = delete;

  ~ExternalBeginFrameSourceAndroid() override;

  void OnVSync(JNIEnv* env, int64_t time_micros, int64_t period_micros);
  void UpdateRefreshRate(float refresh_rate) override;
  void SetSupportedRefreshRates(
      const base::flat_map<base::TimeDelta, float>& supported_rates) override;

 private:
  class AChoreographerImpl;

  // ExternalBeginFrameSourceClient implementation.
  void OnNeedsBeginFrames(bool needs_begin_frames) override;

  void SetEnabled(bool enabled);
  void OnVSyncImpl(int64_t time_nanos,
                   base::TimeDelta vsync_period,
                   PossibleDeadlines possible_deadlines,
                   std::optional<base::TimeDelta> deadline_derived_interval);

  std::unique_ptr<AChoreographerImpl> achoreographer_;
  base::android::ScopedJavaGlobalRef<jobject> j_object_;
  BeginFrameArgsGenerator begin_frame_args_generator_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_EXTERNAL_BEGIN_FRAME_SOURCE_ANDROID_H_
