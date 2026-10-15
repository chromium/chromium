// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SCHEDULED_RESTART_SCHEDULED_RESTART_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_SCHEDULED_RESTART_SCHEDULED_RESTART_BUBBLE_CONTROLLER_H_

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/views/widget/widget.h"

class BrowserProcess;
class BrowserWindowInterface;
class Profile;

namespace content {
class WebContents;
}

namespace scheduled_restart {

class ScheduledRestartManager;

// Controller managing the Scheduled Restart reminder bubble presentation on
// active New Tab Page (NTP) creations.
class ScheduledRestartBubbleController {
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
  virtual ~ScheduledRestartBubbleController();

  // Evaluates tab context and scheduled restart policy, and displays the bubble
  // if eligible.
  void MaybeShowNudgeForWebContents(content::WebContents* web_contents);

  // Displays the scheduled restart bubble for `browser` when explicitly
  // invoked by the user from the 3-dot App Menu ("Relaunch later..."),
  // bypassing the passive NTP nudge delay, lull window, and cooldown checks.
  void ShowBubbleFromAppMenu(BrowserWindowInterface* browser);

  // Returns true if a scheduled restart reminder bubble is currently showing.
  bool is_bubble_showing() const { return bubble_widget_ != nullptr; }

  void set_scheduled_restart_manager_for_testing(
      ScheduledRestartManager* manager) {
    scheduled_restart_manager_for_testing_ = manager;
  }

 protected:
  // Shows the bubble anchored to `browser`'s App Menu button and returns its
  // widget, or null if it could not be anchored. `show_active` is true when
  // the user opens the dialog from the App Menu, so the bubble takes focus.
  // It is false for the passive New Tab Page nudge, which is shown inactive
  // so it does not take focus away from the Omnibox.
  virtual std::unique_ptr<views::Widget> ShowBubble(
      BrowserWindowInterface* browser,
      views::Widget::ClosedCallback on_close,
      bool show_active);

 private:
  // Shows the bubble for `browser` and keeps its widget in `bubble_widget_`.
  // If a widget was created, also calls `srm->RecordNudgeShown()`, which
  // stores the current time as the last nudge time and so restarts the
  // passive nudge cooldown. When no widget could be created the user saw
  // nothing, so the cooldown is left untouched and a later New Tab Page can
  // still show the nudge.
  void ShowAndRecordBubble(BrowserWindowInterface* browser,
                           ScheduledRestartManager* srm,
                           bool show_active);

  ScheduledRestartManager* GetScheduledRestartManager() const;
  void OnBubbleClosed(views::Widget::ClosedReason reason);
  void OnSessionRestored(Profile* profile, int num_tabs_restored);

  raw_ptr<ScheduledRestartManager> scheduled_restart_manager_for_testing_ =
      nullptr;
  std::unique_ptr<views::Widget> bubble_widget_;
  std::optional<ui::ScopedUnownedUserData<ScheduledRestartBubbleController>>
      scoped_unowned_user_data_;
  base::CallbackListSubscription session_restored_subscription_;
};

}  // namespace scheduled_restart

#endif  // CHROME_BROWSER_UI_VIEWS_SCHEDULED_RESTART_SCHEDULED_RESTART_BUBBLE_CONTROLLER_H_
