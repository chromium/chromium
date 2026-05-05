// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANIMATION_BROWSER_ANIMATION_PROVIDER_INTERNAL_H_
#define CHROME_BROWSER_UI_ANIMATION_BROWSER_ANIMATION_PROVIDER_INTERNAL_H_

#include <map>
#include <ostream>
#include <string>
#include <variant>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "chrome/browser/ui/animation/browser_animation_types.h"
#include "ui/gfx/animation/tween.h"

namespace internal {

struct DefaultBrowserAnimationValue {};
using BrowserAnimationValue =
    std::variant<DefaultBrowserAnimationValue, double>;

extern bool operator==(const BrowserAnimationValue& lhs,
                       const BrowserAnimationValue& rhs);
extern bool operator!=(const BrowserAnimationValue& lhs,
                       const BrowserAnimationValue& rhs);
extern std::ostream& operator<<(std::ostream& os,
                                const BrowserAnimationValue& value);

// Represents an absolute time or percentage.
class BrowserAnimationTime {
 public:
  BrowserAnimationTime();
  explicit BrowserAnimationTime(double percent);
  explicit BrowserAnimationTime(base::TimeDelta absolute_time);

  std::optional<double> percent() const;
  std::optional<base::TimeDelta> absolute_time() const;
  bool is_zero() const;
  bool is_one() const;

  std::partial_ordering operator<=>(
      const internal::BrowserAnimationTime& other) const;

  std::partial_ordering operator<=>(double other_pct) const {
    return *this <=> BrowserAnimationTime(other_pct);
  }

  std::partial_ordering operator<=>(base::TimeDelta other_time) const {
    return *this <=> BrowserAnimationTime(other_time);
  }

  bool operator==(const internal::BrowserAnimationTime& other) const {
    return (*this <=> other) == std::partial_ordering::equivalent;
  }

  bool operator==(double other) const {
    return (*this <=> other) == std::partial_ordering::equivalent;
  }

  bool operator==(base::TimeDelta other) const {
    return (*this <=> other) == std::partial_ordering::equivalent;
  }

 private:
  std::variant<double, base::TimeDelta> value_;
};

// Represents one segment of an animation of one UI element.
struct BrowserAnimationKeyframe {
  enum class ValueType {
    kConcrete,
    kDefault,
    kMaxOfDefaultAnd,
    kMinOfDefaultAnd
  };

  struct Value {
    // NOLINTNEXTLINE
    Value(double value_, ValueType type_ = ValueType::kConcrete)
        : value(value_), type(type_) {}
    explicit Value(const BrowserAnimationValue& other);

    double value;
    ValueType type;

    friend bool operator==(const Value&, const Value&) = default;
  };

  // When the keyframe happens.
  BrowserAnimationTime time;

  // The value of the animation (typically between 0 and 1) at the keyframe.
  Value value;

  // What tween should be used between the previous and current keyframe?
  // When `time` is a percentage, this is applied on top of the motion's global
  // tween.
  gfx::Tween::Type tween_type = gfx::Tween::LINEAR;
};

// Describes how a sequence's value transitions from one motion or state to the
// next motion. This is used to connect motions smoothly, even if one motion
// interrupts another.
enum BrowserAnimationTransition {
  // Scale the current motion to start at the prior value.
  kStartAtOldValue,
  // Play the current motion but do not modify the value until it crosses the
  // prior value.
  kCapAtOldValue,
  // Play the current motion from beginning to end, discarding the prior value.
  kIgnoreOldValue,
};

// Represents information about a sequence in a particular group.
struct BrowserAnimationSequenceParams {
  // Whether the value persists between animations. This is used to execute
  // animation transitions.
  bool persist_between_animations = false;

  // Whether a motion which does not specify this sequence at all causes it
  // to animate back to `default_value`.
  bool auto_return_to_default = false;

  // Sets a default value that can be referenced by the `DefaultValue` value
  // and that auto return will target.
  std::optional<double> default_value;
};

using BrowserAnimationSequenceParamsLookup =
    base::flat_map<BrowserAnimationSequence, BrowserAnimationSequenceParams>;

extern std::ostream& operator<<(std::ostream& os,
                                const BrowserAnimationSequenceParams& params);
extern std::ostream& operator<<(
    std::ostream& os,
    const BrowserAnimationSequenceParamsLookup& lookup);

// Holds all segments of an animation motion for one particular UI element.
struct BrowserAnimationSequenceSpecification {
  BrowserAnimationSequenceSpecification();
  BrowserAnimationSequenceSpecification(
      const BrowserAnimationSequenceSpecification&);
  BrowserAnimationSequenceSpecification& operator=(
      const BrowserAnimationSequenceSpecification&);
  BrowserAnimationSequenceSpecification(
      BrowserAnimationSequenceSpecification&&) noexcept;
  BrowserAnimationSequenceSpecification& operator=(
      BrowserAnimationSequenceSpecification&&) noexcept;
  ~BrowserAnimationSequenceSpecification();

  BrowserAnimationTransition transition =
      BrowserAnimationTransition::kStartAtOldValue;
  std::vector<BrowserAnimationKeyframe> keyframes;

  // Determines if this is a "return to value" sequence.
  bool is_return_to() const {
    return keyframes.size() == 1U && keyframes.begin()->time.is_one();
  }

  const BrowserAnimationKeyframe::Value& start() const {
    return keyframes.front().value;
  }
  const BrowserAnimationKeyframe::Value& end() const {
    return keyframes.back().value;
  }
};

// Holds all animations for UI elements for one particular animation motion.
struct BrowserAnimationMotionSpecification {
  BrowserAnimationMotionSpecification();
  BrowserAnimationMotionSpecification(
      const BrowserAnimationMotionSpecification&);
  BrowserAnimationMotionSpecification& operator=(
      const BrowserAnimationMotionSpecification&);
  BrowserAnimationMotionSpecification(
      BrowserAnimationMotionSpecification&&) noexcept;
  BrowserAnimationMotionSpecification& operator=(
      BrowserAnimationMotionSpecification&&) noexcept;
  ~BrowserAnimationMotionSpecification();

  std::optional<base::TimeDelta> duration;
  gfx::Tween::Type global_tween = gfx::Tween::LINEAR;

  // Gets the length of the motion based on either `duration` or all keyframes
  // of all elements.
  base::TimeDelta GetDuration() const;

  // Contains all of the sequences associated with a particular motion.
  std::map<BrowserAnimationSequence, BrowserAnimationSequenceSpecification>
      sequences;
};

}  // namespace internal

#endif  // CHROME_BROWSER_UI_ANIMATION_BROWSER_ANIMATION_PROVIDER_INTERNAL_H_
