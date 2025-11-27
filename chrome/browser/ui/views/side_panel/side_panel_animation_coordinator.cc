// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_animation_coordinator.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_animation_ids.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/tween.h"

namespace {

// Returns true if the AnimationCoordinator is in an open state.
bool IsAnimatingOpen(SidePanelAnimationCoordinator ::AnimationType type) {
  return type != SidePanelAnimationCoordinator::AnimationType::kClose;
}

}  // namespace

using AnimationSpecification =
    SidePanelAnimationCoordinator::AnimationSpecification;
using AnimationSequence = SidePanelAnimationCoordinator::AnimationSequence;

AnimationSpecification::AnimationSpecification(
    gfx::Tween::Type tween_type,
    std::vector<AnimationSequence> sequences)
    : tween_type(tween_type), sequences(sequences) {}
AnimationSpecification::AnimationSpecification(const AnimationSpecification&) =
    default;
AnimationSpecification::~AnimationSpecification() = default;

base::TimeDelta AnimationSpecification::GetAnimationDuration() const {
  base::TimeDelta duration;
  for (const AnimationSequence& sequence : sequences) {
    duration = std::max(duration, sequence.start + sequence.duration);
  }

  return duration;
}

const AnimationSequence& AnimationSpecification::GetSequenceForAnimationId(
    const SidePanelAnimationId& animation_id) const {
  for (const AnimationSequence& sequence : sequences) {
    if (sequence.animation_id == animation_id) {
      return sequence;
    }
  }

  NOTREACHED() << "Sequence not found";
}

bool AnimationSpecification::HasAnimationId(
    const SidePanelAnimationId& animation_id) const {
  for (const AnimationSequence& sequence : sequences) {
    if (sequence.animation_id == animation_id) {
      return true;
    }
  }

  return false;
}

bool AnimationSpecification::IsSequenceRunning(
    const SidePanelAnimationId& animation_id,
    base::TimeDelta elapsed_time) const {
  const AnimationSequence& sequence = GetSequenceForAnimationId(animation_id);
  return sequence.start <= elapsed_time &&
         sequence.start + sequence.duration >= elapsed_time;
}

SidePanelAnimationCoordinator::SidePanelAnimationCoordinator(
    SidePanel* side_panel)
    : views::AnimationDelegateViews(side_panel) {
  animation_.SetTweenType(gfx::Tween::Type::LINEAR);

  const bool is_content_height_panel =
      side_panel->type() == SidePanelEntry::PanelType::kContent;

  AnimationSpecification open_animation_specifications = AnimationSpecification(
      /*tween_type=*/is_content_height_panel
          ? gfx::Tween::Type::EASE_IN_OUT_EMPHASIZED
          : gfx::Tween::Type::ACCEL_45_DECEL_88,
      /*sequences=*/{{.animation_id = kSidePanelBoundsAnimation,
                      .start = base::Milliseconds(0),
                      .duration = base::Milliseconds(
                          is_content_height_panel ? 450 : 350)}});
  if (!is_content_height_panel) {
    open_animation_specifications.sequences.push_back(
        {.animation_id = kShadowOverlayOpacityAnimation,
         .start = base::Milliseconds(150),
         .duration = base::Milliseconds(100)});
  }

  AnimationSpecification open_with_content_transition_animation_specifications =
      AnimationSpecification(
          /*tween_type=*/gfx::Tween::Type::ACCEL_45_DECEL_88,
          /*sequences=*/{
              {.animation_id = kSidePanelBoundsAnimation,
               .start = base::Milliseconds(0),
               .duration = base::Milliseconds(350)},
              {.animation_id = kSidePanelContentTopBoundAnimation,
               .start = base::Milliseconds(100),
               .duration = base::Milliseconds(200)},
              {.animation_id = kSidePanelContentBottomBoundAnimation,
               .start = base::Milliseconds(0),
               .duration = base::Milliseconds(350)},
              {.animation_id = kSidePanelContentLeftBoundAnimation,
               .start = base::Milliseconds(0),
               .duration = base::Milliseconds(350)},
              {.animation_id = kSidePanelContentWidthBoundAnimation,
               .start = base::Milliseconds(0),
               .duration = base::Milliseconds(200)},
              {.animation_id = kSidePanelContentOpacityAnimation,
               .start = base::Milliseconds(150),
               .duration = base::Milliseconds(200)},
              {.animation_id = kSidePanelContentCornerRadiusAnimation,
               .start = base::Milliseconds(0),
               .duration = base::Milliseconds(350)}});
  if (!is_content_height_panel) {
    open_with_content_transition_animation_specifications.sequences.push_back(
        {.animation_id = kShadowOverlayOpacityAnimation,
         .start = base::Milliseconds(150),
         .duration = base::Milliseconds(100)});
  }

  AnimationSpecification close_animation_specifications =
      AnimationSpecification(
          /*tween_type=*/is_content_height_panel
              ? gfx::Tween::Type::EASE_IN_OUT_EMPHASIZED
              : gfx::Tween::Type::ACCEL_45_DECEL_88,
          /*sequences=*/{{.animation_id = kSidePanelBoundsAnimation,
                          .start = base::Milliseconds(0),
                          .duration = base::Milliseconds(
                              is_content_height_panel ? 450 : 350)}});
  if (!is_content_height_panel) {
    close_animation_specifications.sequences.push_back(
        {.animation_id = kShadowOverlayOpacityAnimation,
         .start = base::Milliseconds(0),
         .duration = base::Milliseconds(100)});
  }

  animation_spec_map_ = {
      {AnimationType::kOpen, open_animation_specifications},
      {AnimationType::kOpenWithContentTransition,
       open_with_content_transition_animation_specifications},
      {AnimationType::kClose, close_animation_specifications}};

  animation_type_to_observer_map_[AnimationType::kOpen] = {};
  animation_type_to_observer_map_[AnimationType::kClose] = {};

  Reset(AnimationType::kClose);
}

SidePanelAnimationCoordinator::~SidePanelAnimationCoordinator() = default;

void SidePanelAnimationCoordinator::Start(AnimationType type) {
  if (type == animation_type_ && !animation_.is_animating()) {
    return;
  }

  notified_ended_animations_.clear();
  animation_type_ = type;
  animation_.SetSlideDuration(GetAnimationDuration(type));

  NotifyAnimationTypeStartedObservers();

  if (IsAnimatingOpen(type)) {
    animation_.Show();
  } else {
    animation_.Hide();
  }
}

void SidePanelAnimationCoordinator::Reset(AnimationType type) {
  notified_ended_animations_.clear();
  animation_type_ = type;

  if (IsAnimatingOpen(type)) {
    animation_.Reset(1.0f);
  } else {
    animation_.Reset(0.0f);
  }
}

void SidePanelAnimationCoordinator::AddObserver(
    const SidePanelAnimationId& animation_id,
    AnimationIdObserver* observer) {
  animation_id_to_observer_map_[animation_id].emplace(observer);
}

void SidePanelAnimationCoordinator::RemoveObserver(
    const SidePanelAnimationId& animation_id,
    AnimationIdObserver* observer) {
  CHECK(animation_id_to_observer_map_.contains(animation_id))
      << "Observer was not added for: " << animation_id.GetName();
  animation_id_to_observer_map_[animation_id].erase(observer);
}

void SidePanelAnimationCoordinator::AddObserver(
    AnimationType type,
    AnimationTypeObserver* observer) {
  animation_type_to_observer_map_[type].emplace(observer);
}

void SidePanelAnimationCoordinator::RemoveObserver(
    AnimationType type,
    AnimationTypeObserver* observer) {
  CHECK(animation_type_to_observer_map_.at(type).contains(observer))
      << "Observer was not added for type";
  animation_type_to_observer_map_[type].erase(observer);
}

double SidePanelAnimationCoordinator::GetAnimationValueFor(
    const SidePanelAnimationId& animation_id) {
  if (!animation_.is_animating()) {
    // If the overall animation is not running, return the final value directly.
    return IsAnimatingOpen(animation_type_) ? 1.0 : 0.0;
  }

  const std::optional<AnimationSpecification> specification =
      GetAnimationSpecificationForAnimationId(animation_id);

  if (!specification) {
    return 0.0f;
  }

  const AnimationSequence& sequence =
      specification->GetSequenceForAnimationId(animation_id);

  base::TimeDelta start_time = sequence.start;
  base::TimeDelta duration = sequence.duration;
  base::TimeDelta end_time = start_time + duration;

  const base::TimeDelta elapsed_time = GetElapsedAnimationTime();

  double progress = 0.0f;
  if (elapsed_time <= start_time) {
    progress = 0.0f;
  } else if (elapsed_time >= end_time) {
    progress = 1.0f;
  } else {
    progress = (elapsed_time - start_time) / duration;
  }

  progress = AdjustProgressForAnimationType(progress);
  return gfx::Tween::CalculateValue(specification->tween_type, progress);
}

bool SidePanelAnimationCoordinator::IsClosing() {
  return animation_.IsClosing();
}

void SidePanelAnimationCoordinator::AnimationProgressed(
    const gfx::Animation* animation) {
  for (auto& [animation_id, observers] : animation_id_to_observer_map_) {
    if (!IsAnimationSequenceRunning(animation_id)) {
      NotifyOnSequenceEndedObservers(animation_id, observers);
      continue;
    }

    double animation_value = GetAnimationValueFor(animation_id);
    for (AnimationIdObserver* observer : observers) {
      observer->OnAnimationSequenceProgressed(animation_id, animation_value);
    }
  }
}

void SidePanelAnimationCoordinator::AnimationEnded(
    const gfx::Animation* animation) {
  // Ensure all animation observers are notified if not already.
  for (auto& [animation_id, observers] : animation_id_to_observer_map_) {
    NotifyOnSequenceEndedObservers(animation_id, observers);
  }

  NotifyAnimationTypeEndedObservers();
}

void SidePanelAnimationCoordinator::AnimationCanceled(
    const gfx::Animation* animation) {
  AnimationEnded(animation);
}

double SidePanelAnimationCoordinator::AdjustProgressForAnimationType(
    double progress) const {
  // The underlying `gfx::SlideAnimation` always provides a progress value
  // from 0.0 to 1.0. For an 'open' animation, this is what we want (0% open
  // to 100% open). For a 'close' animation, we want the opposite: to animate
  // from a fully open state (1.0) to a closed state (0.0). This function
  // inverts the progress for closing animations to achieve this.
  return IsAnimatingOpen(animation_type_) ? progress : 1 - progress;
}

base::TimeDelta SidePanelAnimationCoordinator::GetElapsedAnimationTime() const {
  double progress =
      AdjustProgressForAnimationType(animation_.GetCurrentValue());
  return animation_.GetSlideDuration() * progress;
}

base::TimeDelta SidePanelAnimationCoordinator::GetAnimationDuration(
    AnimationType type) {
  base::TimeDelta duration =
      animation_spec_map_.at(type).GetAnimationDuration();
  return duration;
}

bool SidePanelAnimationCoordinator::IsAnimationSequenceRunning(
    const SidePanelAnimationId& animation_id) {
  const std::optional<AnimationSpecification> specification =
      GetAnimationSpecificationForAnimationId(animation_id);
  if (!specification) {
    return false;
  }

  return specification->IsSequenceRunning(animation_id,
                                          GetElapsedAnimationTime());
}

bool SidePanelAnimationCoordinator::IsAnimationSequenceFinished(
    const SidePanelAnimationId& animation_id) {
  const std::optional<AnimationSpecification> specification =
      GetAnimationSpecificationForAnimationId(animation_id);
  if (!specification) {
    return false;
  }

  const AnimationSequence& sequence =
      specification->GetSequenceForAnimationId(animation_id);
  return GetElapsedAnimationTime() >= sequence.start + sequence.duration;
}

const std::optional<AnimationSpecification>
SidePanelAnimationCoordinator::GetAnimationSpecificationForAnimationId(
    const SidePanelAnimationId& animation_id) {
  const AnimationSpecification& animation_specification =
      animation_spec_map_.at(animation_type_);

  if (!animation_specification.HasAnimationId(animation_id)) {
    return std::nullopt;
  }

  return animation_specification;
}

const std::set<raw_ptr<SidePanelAnimationCoordinator::AnimationTypeObserver>>&
SidePanelAnimationCoordinator::GetAnimationTypeObservers() {
  CHECK(animation_type_to_observer_map_.contains(animation_type_))
      << "The animation type must be prepopulated in the constructor";
  return animation_type_to_observer_map_.at(animation_type_);
}

void SidePanelAnimationCoordinator::NotifyOnSequenceEndedObservers(
    const SidePanelAnimationId& animation_id,
    const std::set<raw_ptr<AnimationIdObserver>> observers) {
  if (!IsAnimationSequenceFinished(animation_id) ||
      notified_ended_animations_.contains(animation_id)) {
    return;
  }

  notified_ended_animations_.insert(animation_id);
  for (AnimationIdObserver* observer : observers) {
    observer->OnAnimationSequenceEnded(animation_id);
  }
}

void SidePanelAnimationCoordinator::NotifyAnimationTypeStartedObservers() {
  for (AnimationTypeObserver* observer : GetAnimationTypeObservers()) {
    observer->OnAnimationTypeStarted(animation_type_);
  }
}

void SidePanelAnimationCoordinator::NotifyAnimationTypeEndedObservers() {
  for (AnimationTypeObserver* observer : GetAnimationTypeObservers()) {
    observer->OnAnimationTypeEnded(animation_type_);
  }
}
