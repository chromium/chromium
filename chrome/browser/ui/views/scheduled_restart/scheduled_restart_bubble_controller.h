// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SCHEDULED_RESTART_SCHEDULED_RESTART_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_SCHEDULED_RESTART_SCHEDULED_RESTART_BUBBLE_CONTROLLER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class BrowserProcess;
class BrowserWindowInterface;

namespace content {
class WebContents;
}

namespace scheduled_restart {

class ScheduledRestartManager;

// Controller managing the Scheduled Restart reminder bubble presentation on
// active New Tab Page (NTP) creations.
class ScheduledRestartBubbleController : public views::WidgetObserver {
 public:
  DECLARE_USER_DATA(ScheduledRestartBubbleController);

  // Returns the ScheduledRestartBubbleController instance attached to
  // `browser_process`, or nullptr if unavailable or disabled.
  static ScheduledRestartBubbleController* From(
      BrowserProcess* browser_process);

  // Evaluates whether the given `web_contents` represents a fresh, foreground
  // New Tab Page creation eligible for a scheduled restart reminder nudge,
  // and displays the Views bubble if eligible.
  static void MaybeShowNTPNudge(content::WebContents* web_contents);

  ScheduledRestartBubbleController();
  ScheduledRestartBubbleController(const ScheduledRestartBubbleController&) =
      delete;
  ScheduledRestartBubbleController& operator=(
      const ScheduledRestartBubbleController&) = delete;
  ~ScheduledRestartBubbleController() override;

  // Evaluates tab context and scheduled restart policy, and displays the bubble
  // if eligible.
  void MaybeShowNudgeForWebContents(content::WebContents* web_contents);

  // Returns true if a scheduled restart reminder bubble is currently showing.
  bool is_bubble_showing() const {
    return bubble_widget_observation_.IsObserving();
  }

  void set_scheduled_restart_manager_for_testing(
      ScheduledRestartManager* manager) {
    scheduled_restart_manager_for_testing_ = manager;
  }

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

 protected:
  virtual views::Widget* ShowBubble(BrowserWindowInterface* browser);

 private:
  ScheduledRestartManager* GetScheduledRestartManager() const;

  raw_ptr<ScheduledRestartManager> scheduled_restart_manager_for_testing_ =
      nullptr;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      bubble_widget_observation_{this};
  std::optional<ui::ScopedUnownedUserData<ScheduledRestartBubbleController>>
      scoped_unowned_user_data_;
};

}  // namespace scheduled_restart

#endif  // CHROME_BROWSER_UI_VIEWS_SCHEDULED_RESTART_SCHEDULED_RESTART_BUBBLE_CONTROLLER_H_
