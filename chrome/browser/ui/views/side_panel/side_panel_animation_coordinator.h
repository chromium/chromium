// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ANIMATION_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ANIMATION_COORDINATOR_H_

#include <set>
#include <vector>

#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/side_panel/side_panel_animation_ids.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/animation/animation_delegate_views.h"

class SidePanel;

namespace gfx {
class Animation;
}

// Coordinates multiple animations that need to be synchronized on a single
// timeline for the side panel. This class is useful for creating complex,
// multi-part animations where different elements animate in parallel or in
// sequence when the side panel opens or closes.
//
// The coordinator is driven by a single `gfx::SlideAnimation` that acts as a
// master timeline. Callers can add any number of individual animations, each
// with its own start/end times and tweening behavior, which will be driven by
// the master timeline.
//
// This animation coordinator is owned and managed by the SidePanel. To use it,
// you must get a pointer to it from the SidePanel instance.
//
// Usage:
// 1. Get the SidePanelAnimationCoordinator from the SidePanel instance.
// 2. Implement `SidePanelAnimationCoordinator::Observer` on the view that
//    needs to be animated.
// 3. In the view's initialization, register it as an observer for a specific
//    `SidePanelAnimationId` by calling `AddObserver()`.
// 4. Implement `OnAnimationSequenceProgressed()`. Inside this callback, use the
//    provided `animation_value` (from 0.0 to 1.0) to update the view's
//    properties (e.g., position, opacity).
// 5. The SidePanel automatically calls `Start()` or `Reset()` on the
//    coordinator when its visibility changes, driving the animation.
class SidePanelAnimationCoordinator : public views::AnimationDelegateViews {
 public:
  using SidePanelAnimationId = ui::ElementIdentifier;
  enum class AnimationType { kOpen, kOpenWithContentTransition, kClose };

  // Represents a single animation sequence for a specific animation id.
  struct AnimationSequence {
    SidePanelAnimationId animation_id;
    base::TimeDelta start;
    base::TimeDelta duration;
  };

  // Defines the overall specification for an animation, including its tween
  // type and a collection of individual animation sequences.
  struct AnimationSpecification {
    AnimationSpecification(gfx::Tween::Type tween_type,
                           std::vector<AnimationSequence> sequences);
    AnimationSpecification(const AnimationSpecification&);
    ~AnimationSpecification();

    base::TimeDelta GetAnimationDuration() const;
    const AnimationSequence& GetSequenceForAnimationId(
        const SidePanelAnimationId& animation_id) const;
    bool HasAnimationId(const SidePanelAnimationId& animation_id) const;
    bool IsSequenceRunning(const SidePanelAnimationId& animation_id,
                           base::TimeDelta elapsed_time) const;

    gfx::Tween::Type tween_type;
    std::vector<AnimationSequence> sequences;
  };

  // Used to observe changes to a particular animation id.
  class AnimationIdObserver : public base::CheckedObserver {
   public:
    // Called when the animation sequence for `animation_id` has progressed.
    virtual void OnAnimationSequenceProgressed(
        const SidePanelAnimationId& animation_id,
        double animation_value) {}

    // Called when the animation sequence for `animation_id` has ended.
    virtual void OnAnimationSequenceEnded(
        const SidePanelAnimationId& animation_id) {}
  };

  // Used to observe changes to a particular animation type.
  class AnimationTypeObserver : public base::CheckedObserver {
   public:
    // Called when the coordinator's animation has started for `type`.
    virtual void OnAnimationTypeStarted(AnimationType type) {}

    // Called when the animation coordinator has ended for `type`.
    virtual void OnAnimationTypeEnded(AnimationType type) {}
  };

  explicit SidePanelAnimationCoordinator(SidePanel* side_panel);
  SidePanelAnimationCoordinator(const SidePanelAnimationCoordinator&) = delete;
  SidePanelAnimationCoordinator& operator=(
      const SidePanelAnimationCoordinator&) = delete;
  ~SidePanelAnimationCoordinator() override;

  // Start the animation for a specific type.
  void Start(AnimationType type);
  // Snap to the end state for the animations associated with `type`.
  void Reset(AnimationType type);

  // Add / Remove observers for a specific animation id.
  void AddObserver(const SidePanelAnimationId& animation_id,
                   AnimationIdObserver* observer);
  void RemoveObserver(const SidePanelAnimationId& animation_id,
                      AnimationIdObserver* observer);

  // Add / Remove observers for a specific animation type.
  void AddObserver(AnimationType type, AnimationTypeObserver* observer);
  void RemoveObserver(AnimationType type, AnimationTypeObserver* observer);

  // Returns the animation value for `animation_id` based on it's
  // AnimationSpecification's tween / animation curve. Will always return 0 for
  // animation ids that don't exist for current animation type.
  double GetAnimationValueFor(const SidePanelAnimationId& animation_id);

  // Returns true if the AnimationCoordinator is in the process of a close
  // animation.
  bool IsClosing();

  std::map<AnimationType, AnimationSpecification>&
  animation_spec_map_for_testing() {
    return animation_spec_map_;
  }

  gfx::Animation* animation_for_testing() { return &animation_; }

 private:
  // views::AnimationDelegateViews
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

  double AdjustProgressForAnimationType(double progress) const;
  base::TimeDelta GetElapsedAnimationTime() const;
  base::TimeDelta GetAnimationDuration(AnimationType type);

  // Returns true if the animation is running based on `current_progress_ms_`
  // and the `animate_state_`. Can return false if `animation_id` was not found.
  bool IsAnimationSequenceRunning(const SidePanelAnimationId& animation_id);

  // Returns true if the animation is finished based on `current_progress_ms_`
  // and the `animate_state_`. Can return false if `animation_id` was not found.
  bool IsAnimationSequenceFinished(const SidePanelAnimationId& animation_id);

  // Returns the AnimationSpecification for `animation_id`. Will always return 0
  // for animation_ids that don't exist for current animation type.
  const std::optional<AnimationSpecification>
  GetAnimationSpecificationForAnimationId(
      const SidePanelAnimationId& animation_id);

  // Returns the set of observers listening to the current animation type. These
  // observers will be notified of changes to the main animation state before
  // AnimationProgressed is called.
  const std::set<raw_ptr<AnimationTypeObserver>>& GetAnimationTypeObservers();

  // Notifies observers that an animation sequence has ended for `animation_id`.
  // If the observer has already been notified, do nothing.
  void NotifyOnSequenceEndedObservers(
      const SidePanelAnimationId& animation_id,
      const std::set<raw_ptr<AnimationIdObserver>> observers);

  // Notifies observers that the animation for `animation_type_` will start.
  // This gives observers a chance to set prerequisite states before
  // AnimationProgressed is called.
  void NotifyAnimationTypeStartedObservers();

  // Notifies observers that the animation for `animation_type_` has ended. This
  // gives observers a chance to set final states after AnimationEnded /
  // AnimationCanceled is called.
  void NotifyAnimationTypeEndedObservers();

  // The current AnimationType of the coordinator.
  AnimationType animation_type_ = AnimationType::kClose;

  // Maps the AnimationType to a single AnimationSpecifications that will run
  // for that type.
  std::map<AnimationType, AnimationSpecification> animation_spec_map_;

  // Maps the animation id to the set of its observers.
  std::map<SidePanelAnimationId, std::set<raw_ptr<AnimationIdObserver>>>
      animation_id_to_observer_map_;

  // Maps the animation type to the set of observers
  std::map<AnimationType, std::set<raw_ptr<AnimationTypeObserver>>>
      animation_type_to_observer_map_;

  // Set of animation ids that have already notified their observers that they
  // have ended for the current animation cycle. AnimationIdObservers are only
  // notified if a sequence has ended once.
  std::set<SidePanelAnimationId> notified_ended_animations_;

  // Linear animation used to coordinate all of the other animations
  // added to this class. This serves as the source of truth timeline
  // for the class.
  gfx::SlideAnimation animation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ANIMATION_COORDINATOR_H_
