// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ANIMATION_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ANIMATION_COORDINATOR_H_

#include <vector>

#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/side_panel/side_panel_animation_ids.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/animation/animation_delegate_views.h"

class SidePanel;

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
// 1. Get the SidePanelAnimationCoordinator from the SidePanel.
// 2. Implement `SidePanelAnimationCoordinator::Observer` for each view that
//    needs to be animated.
// 3. For each observer, define an `AnimationSpecification` struct specifying
//    the timing and tweening for the open and close animations.
// 4. Register each animation and its observer with the coordinator via
//    `AddObserver()`.
// 5. The SidePanel will automatically call `Start()` or `Reset()` on the
//    coordinator when its visibility changes.
// 6. The observer's `OnAnimationSequenceProgressed()` will be called as the
//    animation runs. Inside the callback, the observer can get its interpolated
//    value (from 0.0 to 1.0) by calling `GetAnimationValueFor()` and use it to
//    update the view's properties (e.g., position, opacity, etc.).
class SidePanelAnimationCoordinator : views::AnimationDelegateViews {
 public:
  using SidePanelAnimationId = ui::ElementIdentifier;
  enum class AnimationType { kOpen, kClose };

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

  class Observer : public base::CheckedObserver {
   public:
    // Called when the animation coordinator is progressed.
    virtual void OnAnimationSequenceProgressed(
        const SidePanelAnimationId& animation_id,
        double animation_value) {}

    // Called when the animation coordinator is ended.
    virtual void OnAnimationSequenceEnded(
        const SidePanelAnimationId& animation_id) {}
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

  void AddObserver(const SidePanelAnimationId& animation_id,
                   Observer* observer);
  void RemoveObserver(const SidePanelAnimationId& animation_id,
                      Observer* observer);
  double GetAnimationValueFor(const SidePanelAnimationId& animation_id);

  // Returns true if the AnimationCoordinator is in the process of a close
  // animation.
  bool IsClosing();

 private:
  // views::AnimationDelegateViews
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  double AdjustProgressForAnimationType(double progress) const;
  base::TimeDelta GetElapsedAnimationTime() const;
  base::TimeDelta GetAnimationDuration(AnimationType type);

  // Returns true if the animation is running based on `current_progress_ms_`
  // and the `animate_state_`.
  bool IsAnimationSequenceRunning(const SidePanelAnimationId& animation_id);

  // Returns true if the animation is finished based on `current_progress_ms_`
  // and the `animate_state_`.
  bool IsAnimationSequenceFinished(const SidePanelAnimationId& animation_id);

  const AnimationSpecification& GetAnimationSpecificationForAnimationId(
      const SidePanelAnimationId& animation_id);

  // When true we are showing the animation. When false we are hiding the
  // animation.
  AnimationType animation_type_ = AnimationType::kClose;

  // Maps the AnimationType to the list of all of the AnimationSpecifications
  // that should run for that type.
  std::map<AnimationType, std::vector<AnimationSpecification>>
      animation_spec_map_;

  // Maps the property id to the list of its observers.
  std::map<SidePanelAnimationId, std::vector<raw_ptr<Observer>>>
      animation_id_to_observer_map_;

  // Linear animation used to coordinate all of the other animations
  // added to this class. This serves as the source of truth timeline
  // for the class.
  gfx::SlideAnimation animation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ANIMATION_COORDINATOR_H_
