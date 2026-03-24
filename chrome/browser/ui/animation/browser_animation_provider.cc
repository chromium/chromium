// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/animation/browser_animation_provider.h"

#include <compare>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/map_util.h"
#include "base/time/time.h"
#include "chrome/browser/ui/animation/browser_animation_types.h"

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

std::optional<internal::BrowserAnimationMotionSpecification>
CachingBrowserAnimationProvider::GetMotionSpecification(
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
