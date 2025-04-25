// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_FRAME_INTERVAL_MATCHERS_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_FRAME_INTERVAL_MATCHERS_H_

#include <optional>
#include <string>
#include <variant>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "components/viz/common/constants.h"
#include "components/viz/common/quads/frame_interval_inputs.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/service/viz_service_export.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"

namespace viz {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class FrameIntervalMatcherType {
  kNone = 0,
  kInputBoost = 1,
  kOnlyVideo = 2,
  kVideoConference = 3,
  kOnlyAnimatingImage = 4,
  kOnlyScrollBarFadeOut = 5,
  kUserInputBoost = 6,
  kSlowScrollThrottle = 7,
  kMaxValue = kSlowScrollThrottle,
};

// Works with `FrameIntervalDecider` to compute the ideal frame interval.
// Matchers are independent and each matcher matches a specific scenario. Note
// however matcher order is important.
// Matchers should generally be stateless as they can be recreated at run time,
// and any input or state should be stored in `FrameIntervalDecider`.
// Matcher should generally not be platform-specific.
class VIZ_SERVICE_EXPORT FrameIntervalMatcher {
 public:
  // Result can either be an interval class or a specific frame interval,
  // depending on setting and the inputs.
  enum class FrameIntervalClass {
    // These are ordered from lowest frame interval to highest.
    kBoost,    // Used for latency or smoothness sensitive situation such as
               // scrolling.
    kDefault,  // Used if nothing matched.
  };
  enum class ResultIntervalType {
    kExact,
    kAtLeast,
  };
  struct ResultInterval {
    base::TimeDelta interval;
    ResultIntervalType type = ResultIntervalType::kExact;
    bool operator==(const ResultInterval& other) const;
  };
  using Result = std::variant<FrameIntervalClass, ResultInterval>;
  using ResultCallback =
      base::RepeatingCallback<void(Result, FrameIntervalMatcherType)>;

  // Settings for configuration where display supports a fixed set of discrete
  // frame intervals.
  struct VIZ_SERVICE_EXPORT FixedIntervalSettings {
    FixedIntervalSettings();
    FixedIntervalSettings(const FixedIntervalSettings&);
    ~FixedIntervalSettings();

    base::TimeDelta default_interval;  // Used for FrameIntervalClass::kDefault.
    base::flat_set<base::TimeDelta> supported_intervals;  // Cannot be empty.
  };

  // Settings for configuration where display supports a continuous range of
  // frame intervals.
  struct VIZ_SERVICE_EXPORT ContinuousRangeSettings {
    ContinuousRangeSettings();
    ContinuousRangeSettings(const ContinuousRangeSettings&);
    ~ContinuousRangeSettings();

    base::TimeDelta default_interval;  // Used for FrameIntervalClass::kDefault.
    base::TimeDelta min_interval;
    base::TimeDelta max_interval;
  };

  struct VIZ_SERVICE_EXPORT Settings {
    Settings();
    ~Settings();

    Settings(const Settings& other);
    Settings& operator=(const Settings& other);
    Settings(Settings&& other);
    Settings& operator=(Settings&& other);

    // Called with desired display frame interval Result. Must remain valid to
    // call for the lifetime of FrameIntervalDecider.
    ResultCallback result_callback;

    // Settings for what intervals are supported by the display. If this is
    // provided, then `FrameIntervalDecider` matchers should never return a
    // FrameIntervalClass result, and instead should pick one of the
    // supported intervals. If this is set to `monostate`, then
    // `FrameIntervalClass` as well as any frame interval can be returned.
    std::variant<std::monostate, FixedIntervalSettings, ContinuousRangeSettings>
        interval_settings;

    // Timeout to wait for when increasing frame interval, to avoid blip when
    // rapidly switching frame intervals..
    base::TimeDelta increase_frame_interval_timeout = base::Milliseconds(100);

    // For matchers where relevant, frame sinks that has not updated in this
    // time will be assumed to be static and ignored for computation.
    base::TimeDelta ignore_frame_sink_timeout = base::Milliseconds(250);

    // Used for time delta equality comparisons.
    base::TimeDelta epsilon = base::Milliseconds(0.5);

    // Passed into `media::VideoCadenceEstimator::HasSimpleCadence` to compute
    // ideal frame interval.
    base::TimeDelta max_time_until_next_glitch = kMaxTimeUntilNextGlitch;
  };

  struct VIZ_SERVICE_EXPORT Inputs {
    Inputs(const Settings& settings, uint64_t frame_id);
    ~Inputs();

    Inputs(const Inputs& other);
    Inputs& operator=(const Inputs& other);

    // Serializes this struct into a trace.
    void WriteIntoTrace(perfetto::TracedValue trace_context) const;

    base::raw_ref<const Settings> settings;
    // Increasing id for each viz frame.
    uint64_t frame_id;
    base::TimeTicks aggregated_frame_time;
    base::flat_map<FrameSinkId, FrameIntervalInputs> inputs_map;
  };

  static std::string ResultToString(const Result& result);
  static std::string MatcherTypeToString(FrameIntervalMatcherType type);

  static bool AreAlmostEqual(base::TimeDelta a,
                             base::TimeDelta b,
                             base::TimeDelta epsilon);

  virtual ~FrameIntervalMatcher() = default;

  FrameIntervalMatcherType type() const { return type_; }

  virtual std::optional<Result> Match(const Inputs& matcher_inputs) = 0;

 protected:
  explicit FrameIntervalMatcher(FrameIntervalMatcherType type);

 private:
  const FrameIntervalMatcherType type_;
};

#define DECLARE_SIMPLE_FRAME_INTERVAL_MATCHER(ClassName)                \
  class VIZ_SERVICE_EXPORT ClassName : public FrameIntervalMatcher {    \
   public:                                                              \
    ClassName();                                                        \
    ~ClassName() override;                                              \
    std::optional<Result> Match(const Inputs& matcher_inputs) override; \
  }

DECLARE_SIMPLE_FRAME_INTERVAL_MATCHER(InputBoostMatcher);
DECLARE_SIMPLE_FRAME_INTERVAL_MATCHER(OnlyVideoMatcher);
DECLARE_SIMPLE_FRAME_INTERVAL_MATCHER(VideoConferenceMatcher);
DECLARE_SIMPLE_FRAME_INTERVAL_MATCHER(OnlyAnimatingImageMatcher);
DECLARE_SIMPLE_FRAME_INTERVAL_MATCHER(OnlyScrollBarFadeOutAnimationMatcher);
DECLARE_SIMPLE_FRAME_INTERVAL_MATCHER(UserInputBoostMatcher);

#undef DECLARE_SIMPLE_FRAME_INTERVAL_MATCHER

class VIZ_SERVICE_EXPORT SlowScrollThrottleMatcher
    : public FrameIntervalMatcher {
 public:
  explicit SlowScrollThrottleMatcher(float device_scale_factor);
  ~SlowScrollThrottleMatcher() override;
  std::optional<Result> Match(const Inputs& matcher_inputs) override;

 private:
  const float device_scale_factor_;
  uint64_t last_frame_id_matched_without_extra_update_ = 0u;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_FRAME_INTERVAL_MATCHERS_H_
