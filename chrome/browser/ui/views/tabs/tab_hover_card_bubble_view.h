// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_BUBBLE_VIEW_H_

#include <memory>

#include "base/callback_list.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "ui/views/animation/widget_fade_animator.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/metadata/metadata_header_macros.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/metrics_util.h"
#include "base/optional.h"
#endif

namespace gfx {
class ImageSkia;
}

namespace views {
class BubbleSlideAnimator;
class ImageView;
class Label;
class Widget;
}  // namespace views

class Tab;

// Dialog that displays an informational hover card containing page information.
class TabHoverCardBubbleView : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(TabHoverCardBubbleView);
  explicit TabHoverCardBubbleView(Tab* tab);
  TabHoverCardBubbleView(const TabHoverCardBubbleView&) = delete;
  TabHoverCardBubbleView& operator=(const TabHoverCardBubbleView&) = delete;
  ~TabHoverCardBubbleView() override;

  // Updates card content and anchoring and shows the tab hover card.
  void UpdateAndShow(Tab* tab);

  bool GetWidgetVisible() const;

  void FadeOutToHide();

  bool GetFadingOut() const;

  // Returns the target tab (if any).
  views::View* GetDesiredAnchorView();

  // Record a histogram metric of tab hover cards seen prior to a tab being
  // selected by mouse press.
  void RecordHoverCardsSeenRatioMetric();

  void reset_hover_cards_seen_count() { hover_cards_seen_count_ = 0; }

  // BubbleDialogDelegateView:
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;
  ax::mojom::Role GetAccessibleWindowRole() override;
  void Layout() override;

  void set_last_mouse_exit_timestamp(
      base::TimeTicks last_mouse_exit_timestamp) {
    last_mouse_exit_timestamp_ = last_mouse_exit_timestamp;
  }

 private:
  friend class TabHoverCardBubbleViewBrowserTest;
  friend class TabHoverCardBubbleViewInteractiveUiTest;
  class FadeLabel;
  class ThumbnailObserver;

  // Get delay in milliseconds based on tab width.
  base::TimeDelta GetDelay(int tab_width) const;

  void FadeInToShow();

  // Updates and formats title, alert state, domain, and preview image.
  void UpdateCardContent(const Tab* tab);

  // Update the text fade to the given percent, which should be between 0 and 1.
  void UpdateTextFade(double percent);

  void OnThumbnailImageAvailable(gfx::ImageSkia thumbnail_image);
  void ClearPreviewImage();

  // views::BubbleDialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;
  void OnThemeChanged() override;

  void RecordTimeSinceLastSeenMetric(base::TimeDelta elapsed_time);

  void OnSlideAnimationProgressed(views::BubbleSlideAnimator* animator,
                                  double value);
  void OnSlideAnimationComplete(views::BubbleSlideAnimator* animator);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void OnFadeAnimationComplete(views::WidgetFadeAnimator* animator,
                               views::WidgetFadeAnimator::FadeType fade_type);
#endif

  // Fade animations interfere with browser tests so we disable them in tests.
  static bool disable_animations_for_testing_;
  std::unique_ptr<views::WidgetFadeAnimator> fade_animator_;
  std::unique_ptr<views::BubbleSlideAnimator> slide_animator_;
  std::unique_ptr<ThumbnailObserver> thumbnail_observation_;

  // Timestamp of the last time a hover card was visible, recorded before it is
  // hidden. This is used for metrics.
  base::TimeTicks last_visible_timestamp_;

  // Timestamp of the last time the hover card is hidden by the mouse leaving
  // the tab strip. This is used for reshowing the hover card without delay if
  // the mouse reenters within a given amount of time.
  base::TimeTicks last_mouse_exit_timestamp_;

  views::Label* title_label_ = nullptr;
  FadeLabel* title_fade_label_ = nullptr;
  base::Optional<TabAlertState> alert_state_;
  views::Label* domain_label_ = nullptr;
  FadeLabel* domain_fade_label_ = nullptr;
  views::ImageView* preview_image_ = nullptr;

  // Counter used to keep track of the number of tab hover cards seen before a
  // tab is selected by mouse press.
  size_t hover_cards_seen_count_ = 0;
  bool waiting_for_decompress_ = false;

  const bool using_rounded_corners_;

  base::OneShotTimer delayed_show_timer_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::Optional<ui::ThroughputTracker> throughput_tracker_;
  base::CallbackListSubscription fade_complete_subscription_;
#endif
  base::CallbackListSubscription slide_progressed_subscription_;
  base::CallbackListSubscription slide_complete_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_BUBBLE_VIEW_H_
