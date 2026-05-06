// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANIMATION_BROWSER_ANIMATION_PROVIDER_H_
#define CHROME_BROWSER_UI_ANIMATION_BROWSER_ANIMATION_PROVIDER_H_

#include <concepts>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "chrome/browser/ui/animation/browser_animation_provider_internal.h"
#include "chrome/browser/ui/animation/browser_animation_types.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/gfx/animation/tween.h"

// Convenience class for creating browser animation specifications.
//
// If you wish to create specifications on-demand, extend this class and
// override `GetAnimation()`.
//
// If you would prefer to create all of the animations for one or more animation
// groups all at once, see `CachingBrowserAnimationProvider`.
//
// See README.md for full details.
class BrowserAnimationProvider : public ui::FrameworkSpecificImplementation {
 public:
  BrowserAnimationProvider() = default;
  BrowserAnimationProvider(const BrowserAnimationProvider&) = delete;
  void operator=(const BrowserAnimationProvider&) = delete;
  ~BrowserAnimationProvider() override = default;

  using MotionSpecification = internal::BrowserAnimationMotionSpecification;
  using Transition = internal::BrowserAnimationTransition;
  using SequenceParams = internal::BrowserAnimationSequenceParams;
  using SequenceParamsLookup = internal::BrowserAnimationSequenceParamsLookup;
  using KeyframeValueType = internal::BrowserAnimationKeyframe::ValueType;
  using KeyframeValue = internal::BrowserAnimationKeyframe::Value;

  static KeyframeValue DefaultValue() {
    return KeyframeValue(0.0, KeyframeValueType::kDefault);
  }
  static KeyframeValue MaxOfDefaultAnd(double value) {
    return KeyframeValue(value, KeyframeValueType::kMaxOfDefaultAnd);
  }
  static KeyframeValue MinOfDefaultAnd(double value) {
    return KeyframeValue(value, KeyframeValueType::kMinOfDefaultAnd);
  }

  // Creates and returns the animations for `motion` in `group`, or null if
  // this provider does not provide that motion or group.
  std::optional<MotionSpecification> GetMotionSpecification(
      BrowserAnimationGroup group,
      BrowserAnimationMotion motion) const;

  // Gets additional parameters for `sequence` in `group`. By default calls
  // `GetAllSequenceParams()` and looks up `sequence`.
  virtual std::optional<SequenceParams> GetSequenceParams(
      BrowserAnimationGroup group,
      BrowserAnimationSequence sequence) const;

  // Returns all parameters for all sequences in `group`. By default returns
  // nothing.
  virtual SequenceParamsLookup GetAllSequenceParams(
      BrowserAnimationGroup group) const;

  // Returns the histogram prefix to use to log performance metrics for `group`
  // and `motion`. If empty (default) no logging is performed.
  struct HistogramPrefix {
    HistogramPrefix();
    HistogramPrefix(HistogramPrefix&&) noexcept;
    HistogramPrefix& operator=(HistogramPrefix&&) noexcept;
    ~HistogramPrefix();

    std::optional<std::string> group_name;
    std::optional<std::string> motion_name;

    bool is_specified() const {
      return group_name.has_value() && motion_name.has_value();
    }

    // Gets the fully-formatted prefix. Must be specified an in a correct
    // format.
    std::string GetFullPrefix() const;

    // Returns a string representation of this object for debugging, which can
    // be in any state.
    std::string ToString() const;
  };
  virtual HistogramPrefix GetHistogramPrefix(
      BrowserAnimationGroup group,
      BrowserAnimationMotion motion) const;

 protected:
  // Implementation of `GetMotionSpecification()` that just retrieves the
  // motion, without accounting for default values, etc.
  virtual std::optional<MotionSpecification> GetMotionSpecificationImpl(
      BrowserAnimationGroup group,
      BrowserAnimationMotion motion) const = 0;

  using SequenceInfo =
      std::pair<BrowserAnimationSequence,
                internal::BrowserAnimationSequenceSpecification>;

  struct AtMs {
    explicit AtMs(int ms) : time(base::Milliseconds(ms)) {}
    base::TimeDelta time;
  };

  struct AtPercent {
    explicit AtPercent(double percent_) : percent(percent_) {
      CHECK_GE(percent, 0.0);
      CHECK_LE(percent, 1.0);
    }
    double percent;
  };

  struct StartMs {
    explicit StartMs(int ms) : start_time(base::Milliseconds(ms)) {}
    base::TimeDelta start_time;
  };

  struct StartPercent {
    explicit StartPercent(double percent_) : percent(percent_) {
      CHECK_GE(percent, 0.0);
      CHECK_LE(percent, 1.0);
    }
    double percent;
  };

  struct LengthMs {
    explicit LengthMs(int ms) : length(base::Milliseconds(ms)) {
      CHECK_GE(ms, 0);
    }
    base::TimeDelta length;
  };

  struct EndMs {
    explicit EndMs(int ms) : end_time(base::Milliseconds(ms)) {}
    base::TimeDelta end_time;
  };

  struct EndPercent {
    explicit EndPercent(double percent_) : percent(percent_) {
      CHECK_GE(percent, 0.0);
      CHECK_LE(percent, 1.0);
    }
    double percent;
  };

  struct StartingValue {
    explicit StartingValue(KeyframeValue starting_value_)
        : starting_value(starting_value_) {}
    KeyframeValue starting_value;
  };

  struct TotalDurationMs {
    explicit TotalDurationMs(int ms) : length(base::Milliseconds(ms)) {
      CHECK_GE(ms, 0);
    }
    base::TimeDelta length;
  };

  struct FromValue {
    explicit FromValue(KeyframeValue value_) : value(value_) {}
    KeyframeValue value;
  };

  struct Value {
    explicit Value(KeyframeValue value_) : value(value_) {}
    KeyframeValue value;
  };

  struct ToValue {
    explicit ToValue(KeyframeValue value_) : value(value_) {}
    KeyframeValue value;
  };

  // Represents a segment which performs a `tween` to `animate_to` from `start`
  // to `length`. Syntax is: `Segment(StartMs(start), LengthMs(length),
  // AnimateTo(value), tween)`. Unlike keyframes, each segment should define an
  // actual animation, and therefore requires a tween.
  struct Segment {
    Segment(StartMs start_,
            LengthMs length_,
            ToValue animate_to_,
            gfx::Tween::Type tween_ = gfx::Tween::LINEAR)
        : start(start_.start_time),
          end(start_.start_time + length_.length),
          animate_to(animate_to_.value),
          tween(tween_) {}
    Segment(StartMs start_,
            EndMs end_,
            ToValue animate_to_,
            gfx::Tween::Type tween_ = gfx::Tween::LINEAR)
        : start(start_.start_time),
          end(end_.end_time),
          animate_to(animate_to_.value),
          tween(tween_) {
      CHECK_LE(start_.start_time, end_.end_time);
    }
    Segment(StartPercent start_,
            EndPercent end_,
            ToValue animate_to_,
            gfx::Tween::Type tween_ = gfx::Tween::LINEAR)
        : start(start_.percent),
          end(end_.percent),
          animate_to(animate_to_.value),
          tween(tween_) {
      CHECK_LE(start_.percent, end_.percent);
    }

    internal::BrowserAnimationTime start;
    internal::BrowserAnimationTime end;
    KeyframeValue animate_to;
    gfx::Tween::Type tween;
  };

  // Represents a keyframe of an animation.
  // Syntax is `Keyframe(AtMs(ms), Value(value)[, tween])`.
  struct Keyframe {
    Keyframe(AtMs at,
             Value value_,
             gfx::Tween::Type tween_ = gfx::Tween::LINEAR)
        : frame_time(at.time), value(value_.value), tween(tween_) {}
    Keyframe(AtPercent at,
             Value value_,
             gfx::Tween::Type tween_ = gfx::Tween::LINEAR)
        : frame_time(at.percent), value(value_.value), tween(tween_) {}
    internal::BrowserAnimationTime frame_time;
    KeyframeValue value;
    gfx::Tween::Type tween;
  };

  // Creates a list of sequences.
  // Syntax is `Sequences(Sequence(...), Sequence(...), ...)`.
  template <typename... Args>
    requires(std::same_as<Args, SequenceInfo> && ...)
  static MotionSpecification Motion(SequenceInfo first, Args... rest) {
    internal::BrowserAnimationMotionSpecification spec;
    AddSequence(spec, std::move(first));
    (AddSequence(spec, std::move(rest)), ...);
    return spec;
  }

  // Creates a list of sequences with a global duration and tween.
  // Syntax is
  // `Sequences(TotalDurationMs(ms), global_tween,
  //            Sequence(...), Sequence(...), ...)`.
  template <typename... Args>
    requires(std::same_as<Args, SequenceInfo> && ...)
  static MotionSpecification Motion(TotalDurationMs total_duration,
                                    gfx::Tween::Type global_tween,
                                    SequenceInfo first,
                                    Args... rest) {
    CHECK_GE(total_duration.length, base::Milliseconds(0));
    internal::BrowserAnimationMotionSpecification spec;
    spec.duration = total_duration.length;
    spec.global_tween = global_tween;
    AddSequence(spec, std::move(first));
    (AddSequence(spec, std::move(rest)), ...);
    return spec;
  }

  // Creates a sequence from a starting value and list of segments.
  // Syntax is `Sequence(id, StartingValue(value), Segment(...), ...)`.
  template <typename... Args>
    requires(std::same_as<Args, Segment> && ...)
  static SequenceInfo Sequence(BrowserAnimationSequence sequence,
                               StartingValue starting_at,
                               Args... rest) {
    CHECK(sequence);
    internal::BrowserAnimationSequenceSpecification sequence_spec;
    sequence_spec.keyframes.push_back(internal::BrowserAnimationKeyframe{
        .time{}, .value = starting_at.starting_value});
    (AddSegment(sequence_spec, std::move(rest)), ...);
    return std::make_pair(sequence, std::move(sequence_spec));
  }

  // Creates a sequence from a starting value and list of segments, with an
  // explicit transition from the previous value. Syntax is: `Sequence(id,
  // StartingValue(value), transition, Segment(...), ...)`.
  template <typename... Args>
    requires(std::same_as<Args, Segment> && ...)
  static SequenceInfo Sequence(BrowserAnimationSequence sequence,
                               StartingValue starting_at,
                               Transition transition,
                               Args... rest) {
    CHECK(sequence);
    internal::BrowserAnimationSequenceSpecification sequence_spec;
    sequence_spec.transition = transition;
    sequence_spec.keyframes.push_back(internal::BrowserAnimationKeyframe{
        .time{}, .value = starting_at.starting_value});
    (AddSegment(sequence_spec, std::move(rest)), ...);
    return std::make_pair(sequence, std::move(sequence_spec));
  }

  // Creates a sequence that snaps from `starting_value` to `ending_value` at
  // `at_time`.
  static SequenceInfo Snap(BrowserAnimationSequence sequence,
                           FromValue starting_value,
                           ToValue ending_value,
                           AtMs at_time) {
    return at_time.time.is_zero()
               ? Sequence(sequence,
                          Keyframe(at_time, Value(ending_value.value)))
               : Sequence(sequence,
                          Keyframe(at_time, Value(starting_value.value)),
                          Keyframe(at_time, Value(ending_value.value)));
  }
  static SequenceInfo Snap(BrowserAnimationSequence sequence,
                           FromValue starting_value,
                           ToValue ending_value,
                           AtPercent at_time) {
    return at_time.percent == 0.0
               ? Sequence(sequence,
                          Keyframe(at_time, Value(ending_value.value)))
               : Sequence(sequence,
                          Keyframe(at_time, Value(starting_value.value)),
                          Keyframe(at_time, Value(ending_value.value)));
  }

  // Creates a sequence that animates from `starting_value` to `ending_value`
  // over the entire animation, optionally using `tween` (which is added on top
  // of the global tween).
  static SequenceInfo Animate(BrowserAnimationSequence sequence,
                              FromValue starting_value,
                              ToValue ending_value,
                              gfx::Tween::Type tween = gfx::Tween::LINEAR) {
    return Sequence(sequence,
                    Keyframe(AtPercent(0.0), Value(starting_value.value)),
                    Keyframe(AtPercent(1.0), Value(ending_value.value), tween));
  }

  static SequenceInfo Animate(BrowserAnimationSequence sequence,
                              Transition transition,
                              FromValue starting_value,
                              ToValue ending_value,
                              gfx::Tween::Type tween = gfx::Tween::LINEAR) {
    return Sequence(sequence, transition,
                    Keyframe(AtPercent(0.0), Value(starting_value.value)),
                    Keyframe(AtPercent(1.0), Value(ending_value.value), tween));
  }

  // Returns the value of sequence to `ending_value` optionally using `tween`
  // over the whole animation.
  static SequenceInfo Return(BrowserAnimationSequence sequence,
                             ToValue ending_value,
                             gfx::Tween::Type tween = gfx::Tween::LINEAR) {
    CHECK(sequence);
    internal::BrowserAnimationSequenceSpecification spec;
    AddKeyframe(spec,
                Keyframe(AtPercent(1.0), Value(ending_value.value), tween));
    return std::make_pair(sequence, std::move(spec));
  }

  // Creates a sequence from a list of keyframes. The first keyframe will define
  // the starting value fo the animation, and the final keyframe defines the
  // end.
  // Syntax is `Sequence(id, Keyframe(...), Keyframe(...), ...)`.
  template <typename... Args>
    requires(std::same_as<Args, Keyframe> && ...)
  static SequenceInfo Sequence(BrowserAnimationSequence sequence,
                               Keyframe first,
                               Args... rest) {
    CHECK(sequence);
    internal::BrowserAnimationSequenceSpecification spec;
    AddKeyframe(spec, std::move(first));
    (AddKeyframe(spec, std::move(rest)), ...);
    return std::make_pair(sequence, std::move(spec));
  }

  // Creates a sequence from a list of keyframes. The first keyframe will define
  // the starting value fo the animation, and the final keyframe defines the
  // end. Specifies an explicit transition from the previous value.
  // Syntax is `Sequence(id, transition, Keyframe(...), Keyframe(...), ...)`.
  template <typename... Args>
    requires(std::same_as<Args, Keyframe> && ...)
  static SequenceInfo Sequence(BrowserAnimationSequence sequence,
                               Transition transition,
                               Keyframe first,
                               Args... rest) {
    CHECK(sequence);
    internal::BrowserAnimationSequenceSpecification spec;
    spec.transition = transition;
    AddKeyframe(spec, std::move(first));
    (AddKeyframe(spec, std::move(rest)), ...);
    return std::make_pair(sequence, std::move(spec));
  }

  // Helper function used to add elements to a motion.
  static void AddSequence(internal::BrowserAnimationMotionSpecification& spec,
                          SequenceInfo element);

 private:
  static void AddKeyframe(internal::BrowserAnimationSequenceSpecification& spec,
                          Keyframe keyframe);
  static void AddSegment(internal::BrowserAnimationSequenceSpecification& spec,
                         Segment segment);
};

// Animation provider that pre-generates all of its animations at once.
// Override `GenerateAnimations()` to write the generation code.
//
// See README.md for full information.
class CachingBrowserAnimationProvider : public BrowserAnimationProvider {
 public:
  CachingBrowserAnimationProvider();
  ~CachingBrowserAnimationProvider() override;

  using MotionInfo = std::pair<BrowserAnimationMotion,
                               internal::BrowserAnimationMotionSpecification>;
  using MotionLookup = std::map<BrowserAnimationMotion,
                                internal::BrowserAnimationMotionSpecification>;
  using GroupInfo = std::pair<BrowserAnimationGroup, MotionLookup>;
  using GroupInfos = std::map<BrowserAnimationGroup, MotionLookup>;
  // Override this to do the generation; it will be lazily-computed.
  virtual GroupInfos GenerateAnimations() const = 0;

  // BrowserAnimationProvider:
  std::optional<SequenceParams> GetSequenceParams(
      BrowserAnimationGroup group,
      BrowserAnimationSequence sequence) const override;
  SequenceParamsLookup GetAllSequenceParams(
      BrowserAnimationGroup group) const override;
  HistogramPrefix GetHistogramPrefix(
      BrowserAnimationGroup group,
      BrowserAnimationMotion motion) const override;

  // Update or clear the sequence params associated with `sequence` in `group`.
  void UpdateSequenceParams(BrowserAnimationGroup group,
                            BrowserAnimationSequence sequence,
                            std::optional<SequenceParams> params);

  // Updates the default for `sequence` in `group` to `new_default`. There must
  // already be an entry for `sequence`.
  void UpdateDefaultValue(BrowserAnimationGroup group,
                          BrowserAnimationSequence sequence,
                          double new_default);

 protected:
  // BrowserAnimationProvider:
  std::optional<MotionSpecification> GetMotionSpecificationImpl(
      BrowserAnimationGroup group,
      BrowserAnimationMotion motion) const override;

  struct Persist {
    explicit Persist(BrowserAnimationSequence sequence_)
        : sequence(sequence_) {}
    BrowserAnimationSequence sequence;
  };

  struct Default {
    Default(BrowserAnimationSequence sequence_,
            double value_,
            bool auto_return_to_default_)
        : sequence(sequence_),
          value(value_),
          auto_return_to_default(auto_return_to_default_) {}
    BrowserAnimationSequence sequence;
    double value = 0.0;
    bool auto_return_to_default = false;
  };

  // Sets the parameters for any/all sequences in `group`.
  template <typename... Args>
    requires((std::same_as<std::remove_cvref_t<Args>, Persist> ||
              std::same_as<std::remove_cvref_t<Args>, Default>) &&
             ...)
  void SetSequenceParams(BrowserAnimationGroup group,
                         const Args&... sequence_params) {
    SequenceParamsLookup params;
    (AddSequenceParams(params, sequence_params), ...);
    sequence_params_[group] = std::move(params);
  }

  // Sets the histogram prefix components for a group or motion.
  //
  // When an animation is played where both the group and motion have values
  // set, histograms are recorded in the form:
  //   group_prefix.motion_infix.performance_metric
  void SetHistogramName(BrowserAnimationGroup group,
                        std::string_view group_prefix);
  void SetHistogramName(BrowserAnimationMotion motion,
                        std::string_view motion_infix);

  // Creates a list of groups.
  // Syntax is `Groups(Group(...), Group(...), ...)`
  template <typename... Args>
    requires(std::same_as<Args, GroupInfo> && ...)
  static GroupInfos Groups(GroupInfo first, Args... rest) {
    GroupInfos infos;
    infos.insert(std::move(first));
    (infos.insert(std::move(rest)), ...);
    return infos;
  }

  // Creates a group from a list of motions.
  // Syntax is `Group(id, Motion(...), Motion(...), ...)`.
  template <typename... Args>
    requires(std::same_as<Args, MotionInfo> && ...)
  static GroupInfo Group(BrowserAnimationGroup group,
                         MotionInfo first,
                         Args... rest) {
    CHECK(group);
    MotionLookup motions;
    AddMotion(motions, std::move(first));
    (AddMotion(motions, std::move(rest)), ...);
    return std::make_pair(group, std::move(motions));
  }

  // Creates a motion within the current group.
  // Syntax is `Motion(id, Sequence(...), Sequence(...), ...)`.
  template <typename... Args>
    requires(std::same_as<Args, SequenceInfo> && ...)
  static MotionInfo Motion(BrowserAnimationMotion motion,
                           SequenceInfo first,
                           Args... rest) {
    CHECK(motion);
    internal::BrowserAnimationMotionSpecification spec;
    AddSequence(spec, std::move(first));
    (AddSequence(spec, std::move(rest)), ...);
    return std::make_pair(motion, std::move(spec));
  }

  // Creates a motion within the current group with global duration and tween.
  // Syntax is
  // `Motion(id, TotalDurationMs(...), global_tween,
  //         Sequence(...), Sequence(...), ...)`.
  //
  // Note that all the sequences can be implicit (specified as default-value
  // transitions) resulting in no explicit sequences.
  template <typename... Args>
    requires(std::same_as<Args, SequenceInfo> && ...)
  static MotionInfo Motion(BrowserAnimationMotion motion,
                           TotalDurationMs total_duration,
                           gfx::Tween::Type global_tween,
                           Args... sequences) {
    CHECK(motion);
    CHECK_GE(total_duration.length, base::Milliseconds(0));
    internal::BrowserAnimationMotionSpecification spec;
    spec.duration = total_duration.length;
    spec.global_tween = global_tween;
    (AddSequence(spec, std::move(sequences)), ...);
    return std::make_pair(motion, std::move(spec));
  }

  // Helper method that adds motions to a group.
  static void AddMotion(MotionLookup& motions, MotionInfo motion);

 private:
  using BrowserAnimationProvider::Motion;

  void AddSequenceParams(SequenceParamsLookup& lookup, const Persist& persist) {
    lookup.emplace(persist.sequence,
                   SequenceParams{.persist_between_animations = true});
  }

  void AddSequenceParams(SequenceParamsLookup& lookup, const Default& def) {
    lookup.emplace(
        def.sequence,
        SequenceParams{.persist_between_animations = true,
                       .auto_return_to_default = def.auto_return_to_default,
                       .default_value = def.value});
  }

  // Mutable because lazily-generated.
  mutable GroupInfos cached_infos_;
  base::flat_map<BrowserAnimationGroup, SequenceParamsLookup> sequence_params_;
  base::flat_map<BrowserAnimationGroup, std::string> group_histogram_names_;
  base::flat_map<BrowserAnimationMotion, std::string> motion_histogram_names_;
};

#endif  // CHROME_BROWSER_UI_ANIMATION_BROWSER_ANIMATION_PROVIDER_H_
