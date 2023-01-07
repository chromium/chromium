// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_RELAUNCH_NOTIFICATION_RELAUNCH_RECOMMENDED_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_RELAUNCH_NOTIFICATION_RELAUNCH_RECOMMENDED_BUBBLE_VIEW_H_

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "chrome/browser/ui/views/relaunch_notification/relaunch_recommended_timer.h"

class Browser;

namespace views {
class Button;
class Widget;
}  // namespace views

// A View for the relaunch recommended bubble. This is shown to users to
// encourage them to relaunch Chrome by the RelaunchNotificationController as
// dictated by policy settings and upgrade availability.
class RelaunchRecommendedBubbleView : public LocationBarBubbleDelegateView {
 public:
  // Shows the bubble in |browser| for an upgrade that was detected at
  // |detection_time|. |on_accept| is run if the user accepts the prompt to
  // restart.
  static views::Widget* ShowBubble(Browser* browser,
                                   base::Time detection_time,
                                   base::RepeatingClosure on_accept);

  RelaunchRecommendedBubbleView(const RelaunchRecommendedBubbleView&) = delete;
  RelaunchRecommendedBubbleView& operator=(
      const RelaunchRecommendedBubbleView&) = delete;

  ~RelaunchRecommendedBubbleView() override;

  // LocationBarBubbleDelegateView:
  bool Accept() override;
  std::u16string GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  ui::ImageModel GetWindowIcon() override;

 protected:
  // LocationBarBubbleDelegateView:
  void Init() override;
  void VisibilityChanged(views::View* starting_from, bool is_visible) override;

 private:
  RelaunchRecommendedBubbleView(views::Button* anchor_button,
                                base::Time detection_time,
                                base::RepeatingClosure on_accept);

  // Invoked when the timer fires to refresh the title text.
  void UpdateWindowTitle();

  // A callback run if the user accepts the prompt to relaunch the browser.
  base::RepeatingClosure on_accept_;

  // Timer that schedules title refreshes.
  RelaunchRecommendedTimer relaunch_recommended_timer_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_RELAUNCH_NOTIFICATION_RELAUNCH_RECOMMENDED_BUBBLE_VIEW_H_
