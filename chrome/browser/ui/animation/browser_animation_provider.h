// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANIMATION_BROWSER_ANIMATION_PROVIDER_H_
#define CHROME_BROWSER_UI_ANIMATION_BROWSER_ANIMATION_PROVIDER_H_

#include <map>
#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/time/time.h"
#include "chrome/browser/ui/animation/browser_animation_provider_internal.h"
#include "chrome/browser/ui/animation/browser_animation_types.h"
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
class BrowserAnimationProvider {
 public:
  BrowserAnimationProvider() = default;
  BrowserAnimationProvider(const BrowserAnimationProvider&) = delete;
  void operator=(const BrowserAnimationProvider&) = delete;
  virtual ~BrowserAnimationProvider() = default;

  using MotionSpecification = internal::BrowserAnimationMotionSpecification;

  // Creates and returns the animations for `motion` in `group`, or null if
  // this provider does not provide that motion or group.
  virtual std::optional<MotionSpecification> GetMotionSpecification(
      BrowserAnimationGroup group,
      BrowserAnimationMotion motion) const = 0;

 protected:
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
    explicit StartingValue(double starting_value_)
        : starting_value(starting_value_) {}
    double starting_value;
  };

  struct TotalDurationMs {
    explicit TotalDurationMs(int ms) : length(base::Milliseconds(ms)) {
      CHECK_GE(ms, 0);
    }
    base::TimeDelta length;
  };

  struct FromValue {
    explicit FromValue(double value_) : value(value_) {}
    double value;
  };

  struct Value {
    explicit Value(double value_) : value(value_) {}
    double value;
  };

  struct ToValue {
    explicit ToValue(double value_) : value(value_) {}
    double value;
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
    double animate_to;
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
    double value;
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
  std::optional<MotionSpecification> GetMotionSpecification(
      BrowserAnimationGroup group,
      BrowserAnimationMotion motion) const override;

 protected:
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
  // Syntax is `Motion(id, Element(...), Element(...), ...)`.
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
  //         Element(...), Element(...), ...)`.
  template <typename... Args>
    requires(std::same_as<Args, SequenceInfo> && ...)
  static MotionInfo Motion(BrowserAnimationMotion motion,
                           TotalDurationMs total_duration,
                           gfx::Tween::Type global_tween,
                           SequenceInfo first,
                           Args... rest) {
    CHECK(motion);
    CHECK_GE(total_duration.length, base::Milliseconds(0));
    internal::BrowserAnimationMotionSpecification spec;
    spec.duration = total_duration.length;
    spec.global_tween = global_tween;
    AddSequence(spec, std::move(first));
    (AddSequence(spec, std::move(rest)), ...);
    return std::make_pair(motion, std::move(spec));
  }

  // Helper method that adds motions to a group.
  static void AddMotion(MotionLookup& motions, MotionInfo motion);

 private:
  using BrowserAnimationProvider::Motion;

  // Mutable because lazily-generated.
  mutable GroupInfos cached_infos_;
};

#endif  // CHROME_BROWSER_UI_ANIMATION_BROWSER_ANIMATION_PROVIDER_H_
