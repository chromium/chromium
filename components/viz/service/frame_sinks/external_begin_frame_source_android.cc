// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/external_begin_frame_source_android.h"

#include <sys/types.h>

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include "base/android/android_info.h"
#include "base/android/jni_android.h"
#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "components/viz/common/features.h"
#include "components/viz/service/display/frame_deadline_decider.h"
#include "third_party/perfetto/include/perfetto/tracing/track_event_args.h"
#include "ui/gfx/android/achoreographer_compat.h"
#include "ui/gl/gl_features.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/viz/service/service_jni_headers/ExternalBeginFrameSourceAndroid_jni.h"

namespace {

constexpr base::TimeDelta kMinDerivedVsyncInterval = base::Milliseconds(1);

base::TimeTicks ToTimeTicks(int64_t time_nanos) {
  // Warning: It is generally unsafe to manufacture TimeTicks values. The
  // following assumption is being made, AND COULD EASILY BREAK AT ANY TIME:
  // Upstream, Java code is providing "System.nanos() / 1000," and this is the
  // same timestamp that would be provided by the CLOCK_MONOTONIC POSIX clock.
  DCHECK_EQ(base::TimeTicks::GetClock(),
            base::TimeTicks::Clock::LINUX_CLOCK_MONOTONIC);
  return base::TimeTicks() + base::Nanoseconds(time_nanos);
}

constexpr perfetto::protos::pbzero::AndroidDeadlineDerivedIntervalCalculation::
    Result
    ToProtoResult(
        viz::ExternalBeginFrameSourceAndroid::DeadlineDerivedIntervalResult
            result) {
#define CASE(result, proto_result)                                           \
  case viz::ExternalBeginFrameSourceAndroid::DeadlineDerivedIntervalResult:: \
      result:                                                                \
    return perfetto::protos::pbzero::                                        \
        AndroidDeadlineDerivedIntervalCalculation::Result::proto_result
  switch (result) {
    CASE(kNotCalculatedUnsupportedAndroidVersion,
         NOT_CALCULATED_UNSUPPORTED_ANDROID_VERSION);
    CASE(kNotCalculatedTimelinesNotSupported,
         NOT_CALCULATED_TIMELINES_NOT_SUPPORTED);
    CASE(kNotCalculatedOnlyOneTimeline, NOT_CALCULATED_ONLY_ONE_TIMELINE);
    CASE(kNotCalculatedFeatureDisabled, NOT_CALCULATED_FEATURE_DISABLED);
    CASE(kNotCalculatedIntervalTooShort, NOT_CALCULATED_INTERVAL_TOO_SHORT);
    CASE(kCalculatedUnsnappedZeroSnapTolerance,
         CALCULATED_UNSNAPPED_ZERO_SNAP_TOLERANCE);
    CASE(kCalculatedUnsnappedNoSnapCandidates,
         CALCULATED_UNSNAPPED_NO_SNAP_CANDIDATES);
    CASE(kCalculatedUnsnappedOutsideSnapTolerance,
         CALCULATED_UNSNAPPED_OUTSIDE_SNAP_TOLERANCE);
    CASE(
        kCalculatedSnappedToSupportedOsProvidedDisabledOrOutsideSnapTolerance,
        CALCULATED_SNAPPED_TO_SUPPORTED_OS_PROVIDED_DISABLED_OR_OUTSIDE_SNAP_TOLERANCE);
    CASE(kCalculatedSnappedToOsProvidedNoSupportedIntervals,
         CALCULATED_SNAPPED_TO_OS_PROVIDED_NO_SUPPORTED_INTERVALS);
    CASE(kCalculatedSnappedToOsProvidedSupportedOutsideSnapTolerance,
         CALCULATED_SNAPPED_TO_OS_PROVIDED_SUPPORTED_OUTSIDE_SNAP_TOLERANCE);
    CASE(kCalculatedSnappedToEqualSupportedAndOsProvided,
         CALCULATED_SNAPPED_TO_EQUAL_SUPPORTED_AND_OS_PROVIDED);
    CASE(kCalculatedSnappedToSupportedCloserThanOsProvided,
         CALCULATED_SNAPPED_TO_SUPPORTED_CLOSER_THAN_OS_PROVIDED);
    CASE(kCalculatedSnappedToOsProvidedCloserOrTiedWithSupported,
         CALCULATED_SNAPPED_TO_OS_PROVIDED_CLOSER_OR_TIED_WITH_SUPPORTED);
  }
#undef CASE
}

}  // namespace

namespace viz {

class ExternalBeginFrameSourceAndroid::AChoreographerImpl {
 public:
  static std::unique_ptr<AChoreographerImpl> Create(
      ExternalBeginFrameSourceAndroid* client);

  AChoreographerImpl(ExternalBeginFrameSourceAndroid* client,
                     AChoreographer* choreographer);
  ~AChoreographerImpl();

  void SetEnabled(bool enabled);
  void SetSupportedRefreshRates(
      const base::flat_map<base::TimeDelta, float>& supported_rates);

 private:
  static void FrameCallback64(int64_t frame_time_nanos, void* data);
  static void RefreshRateCallback(int64_t vsync_period_nanos, void* data);
  static void VsyncCallback(
      const AChoreographerFrameCallbackData* callback_data,
      void* data);

  void OnVSync(int64_t frame_time_nanos,
               PossibleDeadlines possible_deadlines,
               base::WeakPtr<AChoreographerImpl>* self);
  void SetVsyncPeriod(int64_t vsync_period_nanos);
  void RequestVsyncIfNeeded();

  // Output of `CalculateDeadlineDerivedIntervalImpl()`, bundling the actual
  // VSync interval to store in `BeginFrameArgs::deadline_derived_interval`
  // together with intermediate metadata used only for debugging/analysis (UMA
  // and trace events).
  struct DeadlineDerivedIntervalCalculation {
    // The actual VSync interval to store in
    // `BeginFrameArgs::deadline_derived_interval` (empty if none was calculated
    // for reasons stored in `result`).
    std::optional<base::TimeDelta> deadline_derived_interval;

    // The remaining fields are for debugging/analysis purposes only (UMA and
    // tracing) and do not affect frame scheduling or scroll jank metrics.

    // Outcome of the calculation and reason why `deadline_derived_interval` was
    // not calculated, was left unsnapped, or was snapped.
    DeadlineDerivedIntervalResult result;

    // Difference between the expected presentation deltas of the first two
    // frame timelines (`present_delta[1] - present_delta[0]`). Populated
    // whenever at least two timelines are available and the feature is enabled
    // (even if the difference is less than `kMinDerivedVsyncInterval`).
    std::optional<base::TimeDelta> present_deltas_difference;

    // Closest display-supported VSync interval in `supported_intervals_` to
    // `present_deltas_difference`. Populated whenever `supported_intervals_` is
    // non-empty, `present_deltas_difference >= kMinDerivedVsyncInterval`, and
    // `snap_tolerance > 0` (even when unsnapped or snapped to `vsync_period_`).
    std::optional<base::TimeDelta> closest_supported_interval;
  };

  struct SnapDeadlineDerivedIntervalResult {
    // The final VSync interval to store in
    // `BeginFrameArgs::deadline_derived_interval`.
    base::TimeDelta deadline_derived_interval;

    // Reason why `deadline_derived_interval` was left unsnapped or snapped (for
    // debugging/analysis).
    DeadlineDerivedIntervalResult result;
  };

  std::optional<base::TimeDelta> CalculateDeadlineDerivedInterval(
      const PossibleDeadlines& possible_deadlines) const;
  DeadlineDerivedIntervalCalculation CalculateDeadlineDerivedIntervalImpl(
      const PossibleDeadlines& possible_deadlines) const;
  std::optional<base::TimeDelta> FindClosestSupportedVsyncInterval(
      base::TimeDelta interval) const;
  SnapDeadlineDerivedIntervalResult SnapDeadlineDerivedInterval(
      base::TimeDelta present_deltas_difference,
      double snap_tolerance,
      std::optional<base::TimeDelta> closest_supported_interval) const;
  void RecordDeadlineDerivedIntervalCalculation(
      const DeadlineDerivedIntervalCalculation& calculation) const;

  const raw_ptr<ExternalBeginFrameSourceAndroid> client_;
  const raw_ptr<AChoreographer> achoreographer_;

  base::TimeDelta vsync_period_;
  std::vector<base::TimeDelta> supported_intervals_;
  bool vsync_notification_enabled_ = false;

  // This is a heap-allocated WeakPtr to this object. The WeakPtr is either
  // * passed to `postFrameCallback` if there is one (and exactly one) callback
  //   pending. This is in case this is deleted before a pending callback
  //   fires, in which case the callback is responsible for deleting the
  //   WeakPtr.
  // * or owned by this member variable when there is no pending callback.
  // Thus whether this is nullptr also indicates whether there is a pending
  // frame callback.
  std::unique_ptr<base::WeakPtr<AChoreographerImpl>> self_for_frame_callback_;
  base::WeakPtrFactory<AChoreographerImpl> weak_ptr_factory_{this};
};

// static
std::unique_ptr<ExternalBeginFrameSourceAndroid::AChoreographerImpl>
ExternalBeginFrameSourceAndroid::AChoreographerImpl::Create(
    ExternalBeginFrameSourceAndroid* client) {
  if (base::android::android_info::sdk_int() <
      base::android::android_info::SDK_VERSION_R) {
    return nullptr;
  }
  if (!gfx::AChoreographerCompat::Get().supported)
    return nullptr;

  AChoreographer* choreographer =
      gfx::AChoreographerCompat::Get().AChoreographer_getInstanceFn();
  if (!choreographer)
    return nullptr;

  return std::make_unique<AChoreographerImpl>(client, choreographer);
}

ExternalBeginFrameSourceAndroid::AChoreographerImpl::AChoreographerImpl(
    ExternalBeginFrameSourceAndroid* client,
    AChoreographer* choreographer)
    : client_(client),
      achoreographer_(choreographer),
      vsync_period_(base::Microseconds(16666)) {
  gfx::AChoreographerCompat::Get().AChoreographer_registerRefreshRateCallbackFn(
      achoreographer_, &RefreshRateCallback, this);
  self_for_frame_callback_ =
      std::make_unique<base::WeakPtr<AChoreographerImpl>>(
          weak_ptr_factory_.GetWeakPtr());
}

ExternalBeginFrameSourceAndroid::AChoreographerImpl::~AChoreographerImpl() {
  gfx::AChoreographerCompat::Get()
      .AChoreographer_unregisterRefreshRateCallbackFn(
          achoreographer_, &RefreshRateCallback, this);
}

void ExternalBeginFrameSourceAndroid::AChoreographerImpl::SetEnabled(
    bool enabled) {
  if (vsync_notification_enabled_ == enabled)
    return;
  vsync_notification_enabled_ = enabled;
  RequestVsyncIfNeeded();
}

// static
void ExternalBeginFrameSourceAndroid::AChoreographerImpl::FrameCallback64(
    int64_t frame_time_nanos,
    void* data) {
  TRACE_EVENT0("toplevel,viz", "VSync");
  auto* self = static_cast<base::WeakPtr<AChoreographerImpl>*>(data);
  if (!(*self)) {
    delete self;
    return;
  }
  (*self)->OnVSync(frame_time_nanos, PossibleDeadlines(), self);
}

// static
void ExternalBeginFrameSourceAndroid::AChoreographerImpl::VsyncCallback(
    const AChoreographerFrameCallbackData* callback_data,
    void* data) {
  auto* self = static_cast<base::WeakPtr<AChoreographerImpl>*>(data);
  if (!(*self)) {
    delete self;
    return;
  }

  TRACE_EVENT_BEGIN("toplevel,graphics.pipeline,viz", "Extend_VSync");

  DCHECK(gfx::AChoreographerCompat33::Get().supported);
  int64_t frame_time_nanos =
      gfx::AChoreographerCompat33::Get()
          .AChoreographerFrameCallbackData_getFrameTimeNanosFn(callback_data);
  size_t os_preferred_index =
      gfx::AChoreographerCompat33::Get()
          .AChoreographerFrameCallbackData_getPreferredFrameTimelineIndexFn(
              callback_data);
  size_t size = gfx::AChoreographerCompat33::Get()
                    .AChoreographerFrameCallbackData_getFrameTimelinesLengthFn(
                        callback_data);
  CHECK_LT(os_preferred_index, size);

  std::vector<PossibleDeadline> deadlines;
  deadlines.reserve(size);
  for (size_t i = 0; i < size; ++i) {
    int64_t vsync_id =
        gfx::AChoreographerCompat33::Get()
            .AChoreographerFrameCallbackData_getFrameTimelineVsyncIdFn(
                callback_data, i);
    int64_t deadline =
        gfx::AChoreographerCompat33::Get()
            .AChoreographerFrameCallbackData_getFrameTimelineDeadlineNanosFn(
                callback_data, i);
    int64_t present_time =
        gfx::AChoreographerCompat33::Get()
            .AChoreographerFrameCallbackData_getFrameTimelineExpectedPresentationTimeNanosFn(
                callback_data, i);
    deadlines.emplace_back(vsync_id,
                           base::Nanoseconds(deadline - frame_time_nanos),
                           base::Nanoseconds(present_time - frame_time_nanos));
  }
  PossibleDeadlines possible_deadlines(os_preferred_index,
                                       std::move(deadlines));

  (*self)->OnVSync(frame_time_nanos, possible_deadlines, self);

  // When `viz` is enabled all the possible deadlines are emitted as trace event
  // arguments. In case `viz` is not enabled, we do not emit them to save some
  // trace buffer space.
  TRACE_EVENT_END(
      "toplevel,graphics.pipeline,viz",
      perfetto::Flow::ProcessScoped(FrameDeadlineDecider::GetTraceFlowId(
          base::TimeTicks::FromJavaNanoTime(frame_time_nanos).since_origin())),
      [&](perfetto::EventContext ctx) {
        auto* data = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                         ->set_android_choreographer_frame_callback_data();

        bool viz_enabled = false;
        TRACE_EVENT_CATEGORY_GROUP_ENABLED("viz", &viz_enabled);

        auto frame_time_us = base::TimeTicks::FromJavaNanoTime(frame_time_nanos)
                                 .since_origin()
                                 .InMicroseconds();
        data->set_frame_time_us(frame_time_us);

        if (!viz_enabled) {
          return;
        }

        for (const auto& deadline : possible_deadlines.deadlines()) {
          auto* timeline = data->add_frame_timeline();
          deadline.SetTraceTimelineData(*timeline);
        }
        data->set_preferred_frame_timeline_index(os_preferred_index);
      });
}

// static
void ExternalBeginFrameSourceAndroid::AChoreographerImpl::RefreshRateCallback(
    int64_t vsync_period_nanos,
    void* data) {
  static_cast<AChoreographerImpl*>(data)->SetVsyncPeriod(vsync_period_nanos);
}

void ExternalBeginFrameSourceAndroid::AChoreographerImpl::
    SetSupportedRefreshRates(
        const base::flat_map<base::TimeDelta, float>& supported_rates) {
  supported_intervals_.clear();
  supported_intervals_.reserve(supported_rates.size());
  for (const auto& [interval, rate] : supported_rates) {
    supported_intervals_.push_back(interval);
  }
}

std::optional<base::TimeDelta> ExternalBeginFrameSourceAndroid::
    AChoreographerImpl::CalculateDeadlineDerivedInterval(
        const PossibleDeadlines& possible_deadlines) const {
  const DeadlineDerivedIntervalCalculation calculation =
      CalculateDeadlineDerivedIntervalImpl(possible_deadlines);
  RecordDeadlineDerivedIntervalCalculation(calculation);
  return calculation.deadline_derived_interval;
}

ExternalBeginFrameSourceAndroid::AChoreographerImpl::
    DeadlineDerivedIntervalCalculation
    ExternalBeginFrameSourceAndroid::AChoreographerImpl::
        CalculateDeadlineDerivedIntervalImpl(
            const PossibleDeadlines& possible_deadlines) const {
  using enum DeadlineDerivedIntervalResult;

  // Order matters: check Android version and deadline count BEFORE querying
  // `kCalculateDeadlineDerivedInterval` so ineligible devices/frames neither
  // activate the field trial nor dilute UMA buckets.
  if (base::android::android_info::sdk_int() <
      base::android::android_info::SDK_VERSION_BAKLAVA) {
    return {.result = kNotCalculatedUnsupportedAndroidVersion};
  }
  if (possible_deadlines.empty()) {
    return {.result = kNotCalculatedTimelinesNotSupported};
  }
  if (possible_deadlines.size() < 2u) {
    return {.result = kNotCalculatedOnlyOneTimeline};
  }
  if (!base::FeatureList::IsEnabled(
          features::kCalculateDeadlineDerivedInterval)) {
    return {.result = kNotCalculatedFeatureDisabled};
  }

  const base::TimeDelta present_deltas_difference =
      possible_deadlines.GetDeadlineByIndex(1).present_delta -
      possible_deadlines.GetDeadlineByIndex(0).present_delta;
  if (present_deltas_difference < kMinDerivedVsyncInterval) {
    return {
        .result = kNotCalculatedIntervalTooShort,
        .present_deltas_difference = present_deltas_difference,
    };
  }

  const double snap_tolerance =
      features::kCalculateDeadlineDerivedIntervalSnapToleranceParam.Get();
  if (snap_tolerance <= 0.0) {
    return {
        .deadline_derived_interval = present_deltas_difference,
        .result = kCalculatedUnsnappedZeroSnapTolerance,
        .present_deltas_difference = present_deltas_difference,
    };
  }

  const std::optional<base::TimeDelta> closest_supported_interval =
      FindClosestSupportedVsyncInterval(present_deltas_difference);
  const SnapDeadlineDerivedIntervalResult snap_result =
      SnapDeadlineDerivedInterval(present_deltas_difference, snap_tolerance,
                                  closest_supported_interval);
  return {
      .deadline_derived_interval = snap_result.deadline_derived_interval,
      .result = snap_result.result,
      .present_deltas_difference = present_deltas_difference,
      .closest_supported_interval = closest_supported_interval,
  };
}

std::optional<base::TimeDelta> ExternalBeginFrameSourceAndroid::
    AChoreographerImpl::FindClosestSupportedVsyncInterval(
        base::TimeDelta interval) const {
  if (supported_intervals_.empty()) {
    return std::nullopt;
  }
  const auto it = std::lower_bound(supported_intervals_.begin(),
                                   supported_intervals_.end(), interval);
  if (it == supported_intervals_.begin()) {
    return *it;
  }
  if (it == supported_intervals_.end()) {
    return *(it - 1);
  }
  const base::TimeDelta next_interval = *it;
  const base::TimeDelta prev_interval = *(it - 1);
  return (next_interval - interval < interval - prev_interval) ? next_interval
                                                               : prev_interval;
}

ExternalBeginFrameSourceAndroid::AChoreographerImpl::
    SnapDeadlineDerivedIntervalResult
    ExternalBeginFrameSourceAndroid::AChoreographerImpl::
        SnapDeadlineDerivedInterval(
            base::TimeDelta present_deltas_difference,
            double snap_tolerance,
            std::optional<base::TimeDelta> closest_supported_interval) const {
  DCHECK_GT(snap_tolerance, 0.0);
  using enum DeadlineDerivedIntervalResult;

  const bool snap_to_os_provided =
      features::kCalculateDeadlineDerivedIntervalSnapToOsProvidedParam.Get();
  if (!closest_supported_interval.has_value() && !snap_to_os_provided) {
    return {
        .deadline_derived_interval = present_deltas_difference,
        .result = kCalculatedUnsnappedNoSnapCandidates,
    };
  }

  const base::TimeDelta max_snap_distance =
      snap_tolerance * present_deltas_difference;
  const std::optional<base::TimeDelta> closest_supported_interval_distance =
      closest_supported_interval.transform(
          [&](const base::TimeDelta& interval) {
            return (present_deltas_difference - interval).magnitude();
          });
  const bool closest_supported_interval_within_tolerance =
      closest_supported_interval_distance.has_value() &&
      *closest_supported_interval_distance <= max_snap_distance;

  const std::optional<base::TimeDelta> vsync_period_distance =
      snap_to_os_provided
          ? std::make_optional(
                (present_deltas_difference - vsync_period_).magnitude())
          : std::nullopt;
  const bool vsync_period_within_tolerance =
      vsync_period_distance.has_value() &&
      *vsync_period_distance <= max_snap_distance;

  // Neither candidate is within `snap_tolerance`.
  if (!closest_supported_interval_within_tolerance &&
      !vsync_period_within_tolerance) {
    return {
        .deadline_derived_interval = present_deltas_difference,
        .result = kCalculatedUnsnappedOutsideSnapTolerance,
    };
  }

  // Only `closest_supported_interval` is within `snap_tolerance`.
  if (closest_supported_interval_within_tolerance &&
      !vsync_period_within_tolerance) {
    return {
        .deadline_derived_interval = *closest_supported_interval,
        .result =
            kCalculatedSnappedToSupportedOsProvidedDisabledOrOutsideSnapTolerance,
    };
  }

  // Only `vsync_period_` is within `snap_tolerance`.
  if (!closest_supported_interval_within_tolerance &&
      vsync_period_within_tolerance) {
    return {
        .deadline_derived_interval = vsync_period_,
        .result =
            closest_supported_interval.has_value()
                ? kCalculatedSnappedToOsProvidedSupportedOutsideSnapTolerance
                : kCalculatedSnappedToOsProvidedNoSupportedIntervals,
    };
  }

  // Both `closest_supported_interval` and `vsync_period_` are within
  // `snap_tolerance`.

  if (*closest_supported_interval == vsync_period_) {
    return {
        .deadline_derived_interval = vsync_period_,
        .result = kCalculatedSnappedToEqualSupportedAndOsProvided,
    };
  }

  if (*closest_supported_interval_distance < *vsync_period_distance) {
    return {
        .deadline_derived_interval = *closest_supported_interval,
        .result = kCalculatedSnappedToSupportedCloserThanOsProvided,
    };
  }

  // If `*closest_supported_interval_distance == *vsync_period_distance`, i.e.
  // there's a tie between `closest_supported_interval` and `vsync_period_` and
  // they sit on different sides of `present_deltas_difference`, prefer
  // `vsync_period_`.
  return {
      .deadline_derived_interval = vsync_period_,
      .result = kCalculatedSnappedToOsProvidedCloserOrTiedWithSupported,
  };
}

void ExternalBeginFrameSourceAndroid::AChoreographerImpl::
    RecordDeadlineDerivedIntervalCalculation(
        const DeadlineDerivedIntervalCalculation& calculation) const {
  TRACE_EVENT_INSTANT(
      "viz,input.scrolling",
      "AChoreographerImpl::CalculateDeadlineDerivedInterval",
      [&](perfetto::EventContext ctx) {
        auto* out = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                        ->set_android_deadline_derived_interval_calculation();
        out->set_result(ToProtoResult(calculation.result));
        out->set_os_provided_interval_us(vsync_period_.InMicroseconds());
        if (calculation.present_deltas_difference.has_value()) {
          out->set_present_deltas_difference_us(
              calculation.present_deltas_difference->InMicroseconds());
        }
        if (calculation.closest_supported_interval.has_value()) {
          out->set_closest_supported_interval_us(
              calculation.closest_supported_interval->InMicroseconds());
        }
        if (calculation.deadline_derived_interval.has_value()) {
          out->set_deadline_derived_interval_us(
              calculation.deadline_derived_interval->InMicroseconds());
        }
      });
  if (base::ShouldRecordSubsampledMetric(0.01)) {
    UMA_HISTOGRAM_ENUMERATION(
        "Android.AChoreographer.DeadlineDerivedIntervalResult",
        calculation.result);
  }
}

void ExternalBeginFrameSourceAndroid::AChoreographerImpl::OnVSync(
    int64_t frame_time_nanos,
    PossibleDeadlines possible_deadlines,
    base::WeakPtr<AChoreographerImpl>* self) {
  DCHECK(!self_for_frame_callback_);
  DCHECK(self);
  self_for_frame_callback_.reset(self);
  if (vsync_notification_enabled_) {
    const std::optional<base::TimeDelta> deadline_derived_interval =
        CalculateDeadlineDerivedInterval(possible_deadlines);
    client_->OnVSyncImpl(frame_time_nanos, vsync_period_,
                         std::move(possible_deadlines),
                         deadline_derived_interval);
    RequestVsyncIfNeeded();
  }
}

void ExternalBeginFrameSourceAndroid::AChoreographerImpl::SetVsyncPeriod(
    int64_t vsync_period_nanos) {
  vsync_period_ = base::Nanoseconds(vsync_period_nanos);
  TRACE_EVENT_INSTANT(
      "viz,input.scrolling",
      "ExternalBeginFrameSourceAndroid::AChoreographerImpl::SetVsyncPeriod",
      [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* out = event->set_begin_frame_args();
        out->set_interval_delta_us(vsync_period_.InMicroseconds());
      });
}

void ExternalBeginFrameSourceAndroid::AChoreographerImpl::
    RequestVsyncIfNeeded() {
  if (!vsync_notification_enabled_ || !self_for_frame_callback_)
    return;
  if (gfx::AChoreographerCompat33::Get().supported) {
    gfx::AChoreographerCompat33::Get().AChoreographer_postVsyncCallbackFn(
        achoreographer_, &VsyncCallback, self_for_frame_callback_.release());
  } else {
    gfx::AChoreographerCompat::Get().AChoreographer_postFrameCallback64Fn(
        achoreographer_, &FrameCallback64, self_for_frame_callback_.release());
  }
}

// ============================================================================

ExternalBeginFrameSourceAndroid::ExternalBeginFrameSourceAndroid(
    uint32_t restart_id,
    float refresh_rate,
    bool requires_align_with_java)
    : ExternalBeginFrameSource(this, restart_id) {
  // Android WebView requires begin frame to be inside the "animate" stage of
  // input-animate-draw stages of the java Choreographer, which requires using
  // java Choreographer.
  if (requires_align_with_java) {
    achoreographer_ = nullptr;
  } else {
    achoreographer_ = AChoreographerImpl::Create(this);
  }
  if (!achoreographer_) {
    j_object_ = Java_ExternalBeginFrameSourceAndroid_Constructor(
        base::android::AttachCurrentThread(), reinterpret_cast<int64_t>(this),
        refresh_rate);
  }
}

ExternalBeginFrameSourceAndroid::~ExternalBeginFrameSourceAndroid() {
  SetEnabled(false);
}

void ExternalBeginFrameSourceAndroid::OnVSync(JNIEnv* env,
                                              int64_t time_micros,
                                              int64_t period_micros) {
  OnVSyncImpl(time_micros * 1000, base::Microseconds(period_micros),
              PossibleDeadlines(),
              /*deadline_derived_interval=*/std::nullopt);
}

void ExternalBeginFrameSourceAndroid::OnVSyncImpl(
    int64_t time_nanos,
    base::TimeDelta vsync_period,
    PossibleDeadlines possible_deadlines,
    std::optional<base::TimeDelta> deadline_derived_interval) {
  DCHECK_EQ(base::TimeTicks::GetClock(),
            base::TimeTicks::Clock::LINUX_CLOCK_MONOTONIC);
  base::TimeTicks frame_time = ToTimeTicks(time_nanos);
  // TODO(crbug.com/40829076): If `possible_deadlines` is present, should
  // really pick a deadline from `possible_deadlines`. However some code
  // still assume the deadline is a multiple of interval from frame time.
  base::TimeTicks deadline = frame_time + vsync_period;

  auto begin_frame_args = begin_frame_args_generator_.GenerateBeginFrameArgs(
      source_id(), frame_time, deadline, vsync_period,
      GetMinimumFrameInterval());
  begin_frame_args.deadline_derived_interval = deadline_derived_interval;
  if (features::IsAndroidFrameDeadlineEnabled()) {
    begin_frame_args.possible_deadlines = std::move(possible_deadlines);
  }
  OnBeginFrame(begin_frame_args);
}

void ExternalBeginFrameSourceAndroid::UpdateRefreshRate(float refresh_rate) {
  if (j_object_) {
    Java_ExternalBeginFrameSourceAndroid_updateRefreshRate(
        base::android::AttachCurrentThread(), j_object_, refresh_rate);
  }
}

void ExternalBeginFrameSourceAndroid::SetSupportedRefreshRates(
    const base::flat_map<base::TimeDelta, float>& supported_rates) {
  if (achoreographer_) {
    achoreographer_->SetSupportedRefreshRates(supported_rates);
  }
}

void ExternalBeginFrameSourceAndroid::OnNeedsBeginFrames(
    bool needs_begin_frames) {
  SetEnabled(needs_begin_frames);
}

void ExternalBeginFrameSourceAndroid::SetEnabled(bool enabled) {
  if (achoreographer_) {
    achoreographer_->SetEnabled(enabled);
  } else {
    DCHECK(j_object_);
    Java_ExternalBeginFrameSourceAndroid_setEnabled(
        base::android::AttachCurrentThread(), j_object_, enabled);
  }
}

}  // namespace viz

DEFINE_JNI(ExternalBeginFrameSourceAndroid)
