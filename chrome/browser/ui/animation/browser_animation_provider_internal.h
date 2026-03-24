// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANIMATION_BROWSER_ANIMATION_PROVIDER_INTERNAL_H_
#define CHROME_BROWSER_UI_ANIMATION_BROWSER_ANIMATION_PROVIDER_INTERNAL_H_

#include <map>
#include <variant>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/ui/animation/browser_animation_types.h"
#include "ui/gfx/animation/tween.h"

namespace internal {

// Represents an absolute time or percentage.
class BrowserAnimationTime {
 public:
  BrowserAnimationTime();
  explicit BrowserAnimationTime(double percent);
  explicit BrowserAnimationTime(base::TimeDelta absolute_time);

  std::optional<double> percent() const;
  std::optional<base::TimeDelta> absolute_time() const;
  bool is_zero() const;

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
  // When the keyframe happens.
  BrowserAnimationTime time;

  // The value of the animation (typically between 0 and 1) at the keyframe.
  double value = 1.0;

  // What tween should be used between the previous and current keyframe?
  // When `time` is a percentage, this is applied on top of the motion's global
  // tween.
  gfx::Tween::Type tween_type = gfx::Tween::LINEAR;
};

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

  std::vector<BrowserAnimationKeyframe> keyframes;
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
