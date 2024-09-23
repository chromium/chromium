// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_FRAME_INTERVAL_MATCHERS_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_FRAME_INTERVAL_MATCHERS_H_

#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "components/viz/common/constants.h"
#include "components/viz/common/quads/frame_interval_inputs.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/service/viz_service_export.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace viz {

enum class FrameIntervalMatcherType {
  kNone,
  kInputBoost,
  kOnlyVideo,
  kVideoConference,
  kOnlyAnimatingImage,
  kOnlyScrollBarFadeOut,
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
  using Result = absl::variant<FrameIntervalClass, base::TimeDelta>;
  using ResultCallback =
      base::RepeatingCallback<void(Result, FrameIntervalMatcherType)>;

  // Settings for configuration where display supports a fixed set of discrete
  // frame intervals.
  struct VIZ_SERVICE_EXPORT FixedIntervalSettings {
    FixedIntervalSettings();
    FixedIntervalSettings(const FixedIntervalSettings&);
    ~FixedIntervalSettings();

    base::TimeDelta default_interval;  // Must be in `supported_intervals`.
    base::flat_set<base::TimeDelta> supported_intervals;  // Cannot be empty.
  };

  // Settings for configuration where display supports a continuous range of
  // frame intervals.
  struct VIZ_SERVICE_EXPORT ContinuousRangeSettings {
    ContinuousRangeSettings();
    ContinuousRangeSettings(const ContinuousRangeSettings&);
    ~ContinuousRangeSettings();

    base::TimeDelta min_interval;  // Used as default value.
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
    absl::
        variant<absl::monostate, FixedIntervalSettings, ContinuousRangeSettings>
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
    explicit Inputs(const Settings& settings);
    ~Inputs();

    Inputs(const Inputs& other);
    Inputs& operator=(const Inputs& other);

    base::raw_ref<const Settings> settings;
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

#undef DECLARE_SIMPLE_FRAME_INTERVAL_MATCHER

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_FRAME_INTERVAL_MATCHERS_H_
