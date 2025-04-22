// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_CONTROLLER_H_

#include <memory>

#include "base/callback_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/performance_controls/tab_resource_usage_collector.h"
#include "chrome/browser/ui/views/tabs/tab_slot_controller.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/events/event.h"
#include "ui/views/animation/bubble_slide_animator.h"
#include "ui/views/animation/widget_fade_animator.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace gfx {
class ImageSkia;
}

class TabHoverCardBubbleView;
class TabHoverCardThumbnailObserver;
class Tab;
class TabStrip;

// Controls how hover cards are shown and hidden for tabs.
class TabHoverCardController : public views::ViewObserver,
                               public TabResourceUsageCollector::Observer {
 public:
  explicit TabHoverCardController(TabStrip* tab_strip);
  ~TabHoverCardController() override;

  bool IsHoverCardVisible() const;
  bool IsHoverCardShowingForTab(Tab* tab) const;
  void UpdateHoverCard(Tab* tab,
                       TabSlotController::HoverCardUpdateType update_type);
  void PreventImmediateReshow();

  TabHoverCardBubbleView* hover_card_for_testing() { return hover_card_.get(); }

  size_t hover_cards_seen_count_for_testing() const {
    return hover_cards_seen_count_;
  }

  static void set_disable_animations_for_testing(
      bool disable_animations_for_testing) {
    disable_animations_for_testing_ = disable_animations_for_testing;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(TabHoverCardControllerTest, ShowWrongTabDoesntCrash);
  FRIEND_TEST_ALL_PREFIXES(TabHoverCardControllerTest,
                           SetPreviewWithNoHoverCardDoesntCrash);
  FRIEND_TEST_ALL_PREFIXES(TabHoverCardControllerTest, ShowPreviewsForTab);
  FRIEND_TEST_ALL_PREFIXES(TabHoverCardControllerTest, DisablePreviewsForTab);
  FRIEND_TEST_ALL_PREFIXES(TabHoverCardFadeFooterInteractiveUiTest,
                           HoverCardFooterMemoryUsagePrefEnabled);
  FRIEND_TEST_ALL_PREFIXES(TabHoverCardFadeFooterInteractiveUiTest,
                           HoverCardFooterMemoryUsagePrefDisabled);
  FRIEND_TEST_ALL_PREFIXES(TabHoverCardControllerTest,
                           HidePreviewsForDiscardedTab);
  FRIEND_TEST_ALL_PREFIXES(TabHoverCardControllerTest,
                           DisableMemoryUsageForTab);
  FRIEND_TEST_ALL_PREFIXES(TabHoverCardControllerTest,
                           ShowPreviewsForDiscardedTabWithThumbnail);
  FRIEND_TEST_ALL_PREFIXES(TabHoverCardControllerTest,
                           DontCaptureUnderCriticalMemoryPressure);
  FRIEND_TEST_ALL_PREFIXES(TabHoverCardPreviewsEnabledPrefTest, DefaultState);
  class EventSniffer;

  enum ThumbnailWaitState {
    kNotWaiting,
    kWaitingWithPlaceholder,
    kWaitingWithoutPlaceholder
  };

  // Returns whether the hover card preview images feature is enabled.
  static bool AreHoverCardImagesEnabled();

  // Returns whether hover card animations should be shown on the current
  // device.
  static bool UseAnimations();

  // views::ViewObserver:
  void OnViewIsDeleting(views::View* observed_view) override;
  void OnViewVisibilityChanged(views::View* observed_view,
                               views::View* starting_view) override;

  // TabResourceUsageCollector::Observer:
  void OnTabResourceMetricsRefreshed() override;

  bool ArePreviewsEnabled() const;

  void CreateHoverCard(Tab* tab);
  void UpdateCardContent(Tab* tab);
  void MaybeStartThumbnailObservation(Tab* tab, bool is_initial_show);
  void StartThumbnailObservation(Tab* tab);

  void UpdateOrShowCard(Tab* tab,
                        TabSlotController::HoverCardUpdateType update_type);
  void ShowHoverCard(bool is_initial, const Tab* intended_tab);
  void HideHoverCard();

  bool ShouldShowImmediately(const Tab* tab) const;

  const views::View* GetTargetAnchorView() const;

  // Determines if `target_tab_` is still valid. Call this when entering
  // TabHoverCardController from an asynchronous callback.
  bool TargetTabIsValid() const;

  // Helper for recording when a card becomes fully visible to the user.
  void OnCardFullyVisible();

  // Helper for resetting the cards seen count for testing.
  void ResetCardsSeenCount();

  // Animator events:
  void OnFadeAnimationEnded(views::WidgetFadeAnimator* animator,
                            views::WidgetFadeAnimator::FadeType fade_type);
  void OnSlideAnimationProgressed(views::BubbleSlideAnimator* animator,
                                  double value);
  void OnSlideAnimationComplete(views::BubbleSlideAnimator* animator);

  void OnPreviewImageAvailable(TabHoverCardThumbnailObserver* observer,
                               gfx::ImageSkia thumbnail_image);

  void OnHovercardImagesEnabledChanged();
  void OnHovercardMemoryUsageEnabledChanged();

  bool waiting_for_preview() const {
    return thumbnail_wait_state_ != ThumbnailWaitState::kNotWaiting;
  }

  // Timestamp of the last time the hover card is hidden by the mouse leaving
  // the tab strip. This is used for reshowing the hover card without delay if
  // the mouse reenters within a given amount of time.
  base::TimeTicks last_mouse_exit_timestamp_;

  raw_ptr<Tab> target_tab_ = nullptr;
  const raw_ptr<TabStrip> tab_strip_;
  raw_ptr<TabHoverCardBubbleView> hover_card_ = nullptr;
  base::ScopedObservation<views::View, views::ViewObserver>
      hover_card_observation_{this};
  base::ScopedObservation<views::View, views::ViewObserver>
      target_tab_observation_{this};
  std::unique_ptr<EventSniffer> event_sniffer_;

  // These are used to track when a hover card is shown on a new tab for
  // testing purposes. Counts cards seen from the time the first card is shown
  // to a tab is selected, or the hover card is shown from scratch again.
  raw_ptr<const void> hover_card_last_seen_on_tab_ = nullptr;
  size_t hover_cards_seen_count_ = 0;

  // Fade animations interfere with browser tests so we disable them in tests.
  static bool disable_animations_for_testing_;
  std::unique_ptr<views::WidgetFadeAnimator> fade_animator_;

  // Used to animate the tab hover card's movement between tabs.
  std::unique_ptr<views::BubbleSlideAnimator> slide_animator_;

  std::unique_ptr<TabHoverCardThumbnailObserver> thumbnail_observer_;
  base::CallbackListSubscription thumbnail_subscription_;
  ThumbnailWaitState thumbnail_wait_state_ = ThumbnailWaitState::kNotWaiting;

  base::CallbackListSubscription fade_complete_subscription_;
  base::CallbackListSubscription slide_progressed_subscription_;
  base::CallbackListSubscription slide_complete_subscription_;

  // Ensure that an instance of the TabResourceUsageCollector exists so
  // resources are up to date when we eventually show the hover card.
  raw_ptr<TabResourceUsageCollector> tab_resource_usage_collector_;

  // Tracks changes to the hover card preferences
  PrefChangeRegistrar pref_change_registrar_;
  bool hover_card_image_previews_enabled_ = false;
  bool hover_card_memory_usage_enabled_ = false;

  // Ensure that this timer is destroyed before anything else is cleaned up.
  base::OneShotTimer delayed_show_timer_;
  base::WeakPtrFactory<TabHoverCardController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_CONTROLLER_H_
