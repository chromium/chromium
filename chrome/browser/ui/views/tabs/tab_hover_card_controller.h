// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_CONTROLLER_H_

#include <memory>

#include "base/callback_list.h"
#include "base/gtest_prod_util.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/tabs/tab_controller.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_metrics.h"
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
                               public TabHoverCardMetrics::Delegate {
 public:
  explicit TabHoverCardController(TabStrip* tab_strip);
  ~TabHoverCardController() override;

  // Returns whether the hover card preview images feature is enabled.
  static bool AreHoverCardImagesEnabled();

  bool IsHoverCardVisible() const;
  bool IsHoverCardShowingForTab(Tab* tab) const;
  void UpdateHoverCard(Tab* tab,
                       TabController::HoverCardUpdateType update_type);
  void PreventImmediateReshow();
  void TabSelectedViaMouse(Tab* tab);

 private:
  friend class TabHoverCardBubbleViewBrowserTest;
  friend class TabHoverCardBubbleViewInteractiveUiTest;
  friend class TabHoverCardMetrics;
  FRIEND_TEST_ALL_PREFIXES(TabHoverCardControllerTest, ShowWrongTabDoesntCrash);
  FRIEND_TEST_ALL_PREFIXES(TabHoverCardControllerTest,
                           SetPreviewWithNoHoverCardDoesntCrash);
  class EventSniffer;

  static bool UseAnimations();

  // views::ViewObserver:
  void OnViewIsDeleting(views::View* observed_view) override;

  // TabHoverCardMetrics::Delegate:
  size_t GetTabCount() const override;
  bool ArePreviewsEnabled() const override;
  bool HasPreviewImage() const override;
  views::Widget* GetHoverCardWidget() override;

  void CreateHoverCard(Tab* tab);
  void UpdateCardContent(Tab* tab);
  void MaybeStartThumbnailObservation(Tab* tab, bool is_initial_show);
  void StartThumbnailObservation(Tab* tab);

  void UpdateOrShowCard(Tab* tab,
                        TabController::HoverCardUpdateType update_type);
  void ShowHoverCard(bool is_initial, const Tab* intended_tab);
  void HideHoverCard();

  bool ShouldShowImmediately(const Tab* tab) const;

  const views::View* GetTargetAnchorView() const;

  // Animator events:
  void OnFadeAnimationEnded(views::WidgetFadeAnimator* animator,
                            views::WidgetFadeAnimator::FadeType fade_type);
  void OnSlideAnimationProgressed(views::BubbleSlideAnimator* animator,
                                  double value);
  void OnSlideAnimationComplete(views::BubbleSlideAnimator* animator);

  void OnPreviewImageAvaialble(TabHoverCardThumbnailObserver* observer,
                               gfx::ImageSkia thumbnail_image);

  TabHoverCardMetrics* metrics_for_testing() const { return metrics_.get(); }

  // Timestamp of the last time the hover card is hidden by the mouse leaving
  // the tab strip. This is used for reshowing the hover card without delay if
  // the mouse reenters within a given amount of time.
  base::TimeTicks last_mouse_exit_timestamp_;

  base::OneShotTimer delayed_show_timer_;

  Tab* target_tab_ = nullptr;
  TabStrip* const tab_strip_;
  TabHoverCardBubbleView* hover_card_ = nullptr;
  base::ScopedObservation<views::View, views::ViewObserver>
      hover_card_observation_{this};
  std::unique_ptr<EventSniffer> event_sniffer_;

  // Handles metrics around cards being seen by the user.
  std::unique_ptr<TabHoverCardMetrics> metrics_;

  // Fade animations interfere with browser tests so we disable them in tests.
  static bool disable_animations_for_testing_;
  std::unique_ptr<views::WidgetFadeAnimator> fade_animator_;

  // Used to animate the tab hover card's movement between tabs.
  std::unique_ptr<views::BubbleSlideAnimator> slide_animator_;

  std::unique_ptr<TabHoverCardThumbnailObserver> thumbnail_observer_;
  base::CallbackListSubscription thumbnail_subscription_;
  bool waiting_for_preview_ = false;

  base::CallbackListSubscription fade_complete_subscription_;
  base::CallbackListSubscription slide_progressed_subscription_;
  base::CallbackListSubscription slide_complete_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_CONTROLLER_H_
