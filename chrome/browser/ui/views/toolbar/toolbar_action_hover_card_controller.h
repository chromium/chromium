// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTION_HOVER_CARD_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTION_HOVER_CARD_CONTROLLER_H_

#include <memory>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/toolbar/toolbar_action_hover_card_types.h"
#include "ui/views/animation/bubble_slide_animator.h"
#include "ui/views/animation/widget_fade_animator.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

class ToolbarActionHoverCardBubbleView;
class ExtensionsToolbarContainer;
class ToolbarActionView;

// Controls how hover cards are shown and hidden for toolbar actions.
class ToolbarActionHoverCardController : public views::ViewObserver {
 public:
  explicit ToolbarActionHoverCardController(
      ExtensionsToolbarContainer* extensions_container);
  ~ToolbarActionHoverCardController() override;

  // Returns whether hover card animations should be shown on the current
  // device.
  static bool UseAnimations();

  bool IsHoverCardVisible() const;
  bool IsHoverCardShowingForAction(ToolbarActionView* action_view) const;
  void UpdateHoverCard(ToolbarActionView* action_view,
                       ToolbarActionHoverCardUpdateType update_type);

 private:
  friend class ToolbarActionHoverCardBubbleViewUITest;

  class EventSniffer;

  void UpdateOrShowHoverCard(ToolbarActionView* action_view,
                             ToolbarActionHoverCardUpdateType update_type);
  void UpdateHoverCardContent(ToolbarActionView* action_view);

  void CreateHoverCard(ToolbarActionView* action_view);
  void ShowHoverCard(bool is_initial, const ToolbarActionView* action_view);
  void HideHoverCard();

  bool ShouldShowImmediately(const ToolbarActionView* action_view) const;

  const views::View* GetTargetAnchorView() const;

  // Determines if `target_action_view` is still valid. Call this when entering
  // ToolbarActionHoverCardController from an asynchronous callback.
  bool TargetActionViewIsValid() const;

  // Animator events:
  void OnFadeAnimationEnded(views::WidgetFadeAnimator* animator,
                            views::WidgetFadeAnimator::FadeType fade_type);
  void OnSlideAnimationProgressed(views::BubbleSlideAnimator* animator,
                                  double value);
  void OnSlideAnimationComplete(views::BubbleSlideAnimator* animator);

  // views::ViewObserver:
  void OnViewIsDeleting(views::View* observed_view) override;
  void OnViewVisibilityChanged(views::View* observed_view,
                               views::View* starting_view) override;

  // Timestamp of the last time the hover card is hidden by the mouse leaving
  // the tab strip. This is used for reshowing the hover card without delay if
  // the mouse reenters within a given amount of time.
  base::TimeTicks last_mouse_exit_timestamp_;

  raw_ptr<ToolbarActionView> target_action_view_ = nullptr;
  const raw_ptr<ExtensionsToolbarContainer> extensions_container_;
  raw_ptr<ToolbarActionHoverCardBubbleView> hover_card_ = nullptr;

  base::ScopedObservation<views::View, views::ViewObserver>
      hover_card_observation_{this};
  base::ScopedObservation<views::View, views::ViewObserver>
      target_action_view_observation_{this};
  std::unique_ptr<EventSniffer> event_sniffer_;

  // Used to animate the tab hover card's opacity when visible or not.
  std::unique_ptr<views::WidgetFadeAnimator> fade_animator_;
  // Fade animations interfere with browser tests so we disable them in tests.
  static bool disable_animations_for_testing_;

  // Used to animate the tab hover card's movement between tabs.
  std::unique_ptr<views::BubbleSlideAnimator> slide_animator_;

  base::CallbackListSubscription fade_complete_subscription_;
  base::CallbackListSubscription slide_progressed_subscription_;
  base::CallbackListSubscription slide_complete_subscription_;

  // Ensure that this timer is destroyed before anything else is cleaned up.
  base::OneShotTimer delayed_show_timer_;
  base::WeakPtrFactory<ToolbarActionHoverCardController> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTION_HOVER_CARD_CONTROLLER_H_
