// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/animation/browser_animation_provider.h"

#include <compare>
#include <optional>
#include <sstream>
#include <string>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/containers/map_util.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/ui/animation/browser_animation_provider_internal.h"
#include "chrome/browser/ui/animation/browser_animation_types.h"

BrowserAnimationProvider::HistogramPrefix::HistogramPrefix() = default;
BrowserAnimationProvider::HistogramPrefix::HistogramPrefix(
    HistogramPrefix&&) noexcept = default;
BrowserAnimationProvider::HistogramPrefix&
BrowserAnimationProvider::HistogramPrefix::operator=(
    HistogramPrefix&&) noexcept = default;
BrowserAnimationProvider::HistogramPrefix::~HistogramPrefix() = default;

std::string BrowserAnimationProvider::HistogramPrefix::GetFullPrefix() const {
  CHECK(group_name);
  CHECK(motion_name);
  if (group_name->empty()) {
    return *motion_name;
  }
  if (motion_name->empty()) {
    return *group_name;
  }
  return base::StringPrintf("%s.%s", group_name->c_str(), motion_name->c_str());
}

std::string BrowserAnimationProvider::HistogramPrefix::ToString() const {
  std::ostringstream oss;
  oss << "HistogramPrefix{ group_name = " << group_name.value_or("[none]")
      << " motion_name = " << motion_name.value_or("[none]") << " }";
  return oss.str();
}

std::optional<BrowserAnimationProvider::MotionSpecification>
BrowserAnimationProvider::GetMotionSpecification(
    BrowserAnimationGroup group,
    BrowserAnimationMotion motion) const {
  auto result = GetMotionSpecificationImpl(group, motion);
  if (result) {
    // Find all auto-return sequences.
    base::flat_set<BrowserAnimationSequence> auto_return;
    for (const auto& [sequence, params] : GetAllSequenceParams(group)) {
      if (params.auto_return_to_default) {
        auto_return.insert(sequence);
      }
    }
    // Eliminate sequences for which an explicit sequence exists.
    for (const auto& [sequence, info] : result->sequences) {
      auto_return.erase(sequence);
    }
    // Add a simple "return to" sequence for each auto-return which does not
    // have an explicit sequence.
    for (auto sequence : auto_return) {
      internal::BrowserAnimationSequenceSpecification spec;
      AddKeyframe(spec, Keyframe(AtPercent(1.0), Value(DefaultValue())));
      result->sequences.emplace(sequence, spec);
    }
  }
  return result;
}

std::optional<BrowserAnimationProvider::SequenceParams>
BrowserAnimationProvider::GetSequenceParams(
    BrowserAnimationGroup group,
    BrowserAnimationSequence sequence) const {
  const auto all_params = GetAllSequenceParams(group);
  const auto* const params = base::FindOrNull(all_params, sequence);
  return params ? std::make_optional(*params) : std::nullopt;
}

BrowserAnimationProvider::SequenceParamsLookup
BrowserAnimationProvider::GetAllSequenceParams(
    BrowserAnimationGroup group) const {
  return {};
}

BrowserAnimationProvider::HistogramPrefix
BrowserAnimationProvider::GetHistogramPrefix(
    BrowserAnimationGroup group,
    BrowserAnimationMotion motion) const {
  return {};
}

// static
void BrowserAnimationProvider::AddSequence(
    internal::BrowserAnimationMotionSpecification& motion_spec,
    SequenceInfo element) {
  if (motion_spec.duration.has_value()) {
    CHECK(!element.second.keyframes.back().time.absolute_time() ||
          *element.second.keyframes.back().time.absolute_time() <=
              motion_spec.duration.value());
  } else {
    CHECK(element.second.keyframes.back().time.absolute_time());
  }
  const auto result =
      motion_spec.sequences.emplace(element.first, std::move(element.second));
  CHECK(result.second) << "Added duplicate element: " << element.first;
}

// static
void BrowserAnimationProvider::AddKeyframe(
    internal::BrowserAnimationSequenceSpecification& sequence_spec,
    Keyframe keyframe) {
  internal::BrowserAnimationKeyframe to_add{.time = keyframe.frame_time,
                                            .value = keyframe.value,
                                            .tween_type = keyframe.tween};
  if (!sequence_spec.keyframes.empty()) {
    CHECK(sequence_spec.keyframes.back().time <= to_add.time);
  }
  sequence_spec.keyframes.emplace_back(std::move(to_add));
}

// static
void BrowserAnimationProvider::AddSegment(
    internal::BrowserAnimationSequenceSpecification& sequence_spec,
    Segment segment) {
  CHECK(segment.start >= sequence_spec.keyframes.back().time);

  // If the start is after the previous value, then another keyframe is
  // required with the same value.
  if (segment.start > sequence_spec.keyframes.back().time) {
    sequence_spec.keyframes.push_back(
        {.time = segment.start, .value = sequence_spec.keyframes.back().value});
  }

  sequence_spec.keyframes.push_back({.time = segment.end,
                                     .value = segment.animate_to,
                                     .tween_type = segment.tween});
}

CachingBrowserAnimationProvider::CachingBrowserAnimationProvider() = default;
CachingBrowserAnimationProvider::~CachingBrowserAnimationProvider() = default;

std::optional<BrowserAnimationProvider::SequenceParams>
CachingBrowserAnimationProvider::GetSequenceParams(
    BrowserAnimationGroup group,
    BrowserAnimationSequence sequence) const {
  if (auto* const result = base::FindOrNull(sequence_params_, group)) {
    if (auto* const params = base::FindOrNull(*result, sequence)) {
      return *params;
    }
  }
  return std::nullopt;
}

BrowserAnimationProvider::SequenceParamsLookup
CachingBrowserAnimationProvider::GetAllSequenceParams(
    BrowserAnimationGroup group) const {
  if (auto* const result = base::FindOrNull(sequence_params_, group)) {
    return *result;
  }
  return {};
}

BrowserAnimationProvider::HistogramPrefix
CachingBrowserAnimationProvider::GetHistogramPrefix(
    BrowserAnimationGroup group,
    BrowserAnimationMotion motion) const {
  HistogramPrefix prefix;
  if (const auto* const group_prefix =
          base::FindOrNull(group_histogram_names_, group)) {
    prefix.group_name = *group_prefix;
  }
  if (const auto* const motion_infix =
          base::FindOrNull(motion_histogram_names_, motion)) {
    prefix.motion_name = *motion_infix;
  }
  return prefix;
}

void CachingBrowserAnimationProvider::SetHistogramName(
    BrowserAnimationGroup group,
    std::string_view group_prefix) {
  CHECK(group);
  CHECK(!group_prefix.starts_with('.'));
  CHECK(!group_prefix.ends_with('.'));
  group_histogram_names_[group] = group_prefix;
}

void CachingBrowserAnimationProvider::SetHistogramName(
    BrowserAnimationMotion motion,
    std::string_view motion_infix) {
  CHECK(motion);
  CHECK(!motion_infix.starts_with('.'));
  CHECK(!motion_infix.ends_with('.'));
  motion_histogram_names_[motion] = motion_infix;
}

void CachingBrowserAnimationProvider::UpdateSequenceParams(
    BrowserAnimationGroup group,
    BrowserAnimationSequence sequence,
    std::optional<SequenceParams> params) {
  auto* const group_info = base::FindOrNull(sequence_params_, group);
  CHECK(group_info) << "No sequence params found for " << group;
  if (!params) {
    group_info->erase(sequence);
  } else {
    (*group_info)[sequence] = *params;
  }
}

void CachingBrowserAnimationProvider::UpdateDefaultValue(
    BrowserAnimationGroup group,
    BrowserAnimationSequence sequence,
    double new_default) {
  auto* const group_info = base::FindOrNull(sequence_params_, group);
  CHECK(group_info) << "No sequence params found for " << group;
  auto* const params = base::FindOrNull(*group_info, sequence);
  CHECK(params) << "No params found for " << sequence << " in " << group;
  params->default_value = new_default;
}

std::optional<internal::BrowserAnimationMotionSpecification>
CachingBrowserAnimationProvider::GetMotionSpecificationImpl(
    BrowserAnimationGroup group,
    BrowserAnimationMotion motion) const {
  if (cached_infos_.empty()) {
    cached_infos_ = GenerateAnimations();
    CHECK(!cached_infos_.empty());
  }
  if (const auto* const group_info = base::FindOrNull(cached_infos_, group)) {
    if (const auto* const motion_info = base::FindOrNull(*group_info, motion)) {
      return *motion_info;
    }
  }
  return std::nullopt;
}

// static
void CachingBrowserAnimationProvider::AddMotion(MotionLookup& motions,
                                                MotionInfo motion) {
  const auto result = motions.emplace(motion.first, std::move(motion.second));
  CHECK(result.second) << "Added duplicate motion: " << motion.first;
}
