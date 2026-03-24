// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/animation/browser_animation_provider_internal.h"

namespace internal {

BrowserAnimationTime::BrowserAnimationTime() : value_(0.0) {}
BrowserAnimationTime::BrowserAnimationTime(double percent) : value_(percent) {}
BrowserAnimationTime::BrowserAnimationTime(base::TimeDelta absolute_time)
    : value_(absolute_time) {}

std::optional<double> BrowserAnimationTime::percent() const {
  if (const auto* const pct = std::get_if<double>(&value_)) {
    return *pct;
  }
  return std::nullopt;
}

std::optional<base::TimeDelta> BrowserAnimationTime::absolute_time() const {
  if (const auto* const time = std::get_if<base::TimeDelta>(&value_)) {
    return *time;
  }
  return std::nullopt;
}

bool BrowserAnimationTime::is_zero() const {
  if (const auto p = percent()) {
    return *p == 0;
  }
  return absolute_time()->is_zero();
}

std::partial_ordering BrowserAnimationTime::operator<=>(
    const internal::BrowserAnimationTime& other) const {
  if (is_zero()) {
    return other.is_zero() ? std::partial_ordering::equivalent
                           : std::partial_ordering::less;
  }
  if (other.is_zero()) {
    return std::partial_ordering::greater;
  }
  if (const auto t = absolute_time()) {
    const auto other_t = other.absolute_time();
    CHECK(other_t) << "Value is " << *t << " with other as "
                   << *other.percent();
    return t <=> other_t;
  }
  const auto p = percent();
  const auto other_p = other.percent();
  CHECK(other_p) << "Value is " << *p << " with other as "
                 << *other.absolute_time();
  return p <=> other_p;
}

BrowserAnimationSequenceSpecification::BrowserAnimationSequenceSpecification() =
    default;
BrowserAnimationSequenceSpecification::BrowserAnimationSequenceSpecification(
    const BrowserAnimationSequenceSpecification&) = default;
BrowserAnimationSequenceSpecification&
BrowserAnimationSequenceSpecification::operator=(
    const BrowserAnimationSequenceSpecification&) = default;
BrowserAnimationSequenceSpecification::BrowserAnimationSequenceSpecification(
    BrowserAnimationSequenceSpecification&&) noexcept = default;
BrowserAnimationSequenceSpecification&
BrowserAnimationSequenceSpecification::operator=(
    BrowserAnimationSequenceSpecification&&) noexcept = default;
BrowserAnimationSequenceSpecification::
    ~BrowserAnimationSequenceSpecification() = default;

BrowserAnimationMotionSpecification::BrowserAnimationMotionSpecification() =
    default;
BrowserAnimationMotionSpecification::BrowserAnimationMotionSpecification(
    const BrowserAnimationMotionSpecification&) = default;
BrowserAnimationMotionSpecification&
BrowserAnimationMotionSpecification::operator=(
    const BrowserAnimationMotionSpecification&) = default;
BrowserAnimationMotionSpecification::BrowserAnimationMotionSpecification(
    BrowserAnimationMotionSpecification&&) noexcept = default;
BrowserAnimationMotionSpecification&
BrowserAnimationMotionSpecification::operator=(
    BrowserAnimationMotionSpecification&&) noexcept = default;
BrowserAnimationMotionSpecification::~BrowserAnimationMotionSpecification() =
    default;

base::TimeDelta BrowserAnimationMotionSpecification::GetDuration() const {
  base::TimeDelta result;
  if (duration.has_value()) {
    return duration.value();
  }
  for (const auto& [seq, spec] : sequences) {
    if (!spec.keyframes.empty() && spec.keyframes.back().time.absolute_time()) {
      result = std::max(result, *spec.keyframes.back().time.absolute_time());
    }
  }
  return result;
}

}  // namespace internal
