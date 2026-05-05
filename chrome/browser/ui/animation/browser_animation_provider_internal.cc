// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/animation/browser_animation_provider_internal.h"

#include <type_traits>
#include <variant>

namespace internal {

namespace {
template <typename T, typename... Args>
T GetOr(const std::variant<Args...>& v, T default_value) {
  const T* const t = std::get_if<std::remove_cvref_t<T>>(&v);
  return t ? *t : default_value;
}

}  // namespace

bool operator==(const BrowserAnimationValue& lhs,
                const BrowserAnimationValue& rhs) {
  if (lhs.index() != rhs.index()) {
    return false;
  }
  return std::holds_alternative<DefaultBrowserAnimationValue>(lhs) ||
         std::get<double>(lhs) == std::get<double>(rhs);
}

bool operator!=(const BrowserAnimationValue& lhs,
                const BrowserAnimationValue& rhs) {
  return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os, const BrowserAnimationValue& value) {
  if (std::holds_alternative<DefaultBrowserAnimationValue>(value)) {
    os << "[default]";
  } else {
    os << std::get<double>(value);
  }
  return os;
}

BrowserAnimationKeyframe::Value::Value(const BrowserAnimationValue& other)
    : value(GetOr<double>(other, 0.0)),
      type(std::holds_alternative<DefaultBrowserAnimationValue>(other)
               ? ValueType::kDefault
               : ValueType::kConcrete) {}

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
    return *p == 0.0;
  }
  return absolute_time()->is_zero();
}

bool BrowserAnimationTime::is_one() const {
  if (const auto p = percent()) {
    return *p == 1.0;
  }
  return false;
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

std::ostream& operator<<(std::ostream& os,
                         const BrowserAnimationSequenceParams& params) {
  os << "{ persist? " << params.persist_between_animations << " auto-return? "
     << params.auto_return_to_default;
  if (params.default_value) {
    os << " default: " << *params.default_value;
  } else {
    os << " no default";
  }
  os << " }";
  return os;
}

std::ostream& operator<<(std::ostream& os,
                         const BrowserAnimationSequenceParamsLookup& lookup) {
  os << "Params lookup:";
  for (const auto& [sequence, params] : lookup) {
    os << "\n  " << sequence << ": " << params;
  }
  return os;
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
