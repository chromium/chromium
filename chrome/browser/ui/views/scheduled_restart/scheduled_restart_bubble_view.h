// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SCHEDULED_RESTART_SCHEDULED_RESTART_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SCHEDULED_RESTART_SCHEDULED_RESTART_BUBBLE_VIEW_H_

#include <memory>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/widget/widget.h"

class BrowserWindowInterface;

namespace scheduled_restart {

DECLARE_ELEMENT_IDENTIFIER_VALUE(kScheduledRestartDialogId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kScheduledRestartBodyId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kRestartNowButtonId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kRestartWhenIdleButtonId);

// Reminder bubble prompt shown to unmanaged users during lull hours when
// opening a fresh NTP tab to schedule an automatic restart on idle or restart
// immediately.
class ScheduledRestartBubbleView {
 public:
  // Represents the choice selected by the user in the dialog for UMA logging.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(ScheduledRestartDialogChoice)
  enum class ScheduledRestartDialogChoice {
    kDismissed = 0,
    kRestartNow = 1,
    kScheduledOnIdle = 2,
    kMaxValue = kScheduledOnIdle,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/session/enums.xml:ScheduledRestartDialogChoice)

  ScheduledRestartBubbleView() = delete;
  ScheduledRestartBubbleView(const ScheduledRestartBubbleView&) = delete;
  ScheduledRestartBubbleView& operator=(const ScheduledRestartBubbleView&) =
      delete;

  // Shows the bubble anchored to the browser's App Menu. `on_close` is invoked
  // when the bubble widget closes. When `show_active` is false (default for
  // passive New Tab Page nudges), the bubble is shown inactive so it does not
  // steal focus; when true (explicit App Menu action), it is shown active.
  static std::unique_ptr<views::Widget> ShowBubble(
      BrowserWindowInterface* browser,
      views::Widget::ClosedCallback on_close = base::NullCallback(),
      bool show_active = false);

  // Destroys `widget`, a bubble previously returned by ShowBubble(), without
  // recording Session.ScheduledRestart.DialogChoice. Used when the bubble is
  // replaced by a new one rather than dismissed by the user, so that the
  // replacement does not inflate the dismissal count.
  static void DestroyReplacedBubble(std::unique_ptr<views::Widget> widget);

  // Formats the idle duration for display, binding the numeral to the unit
  // with a non-breaking space to prevent orphaned numerals on line wrap.
  static std::u16string FormatIdleDuration(base::TimeDelta idle_threshold);

  // Sets a testing callback to be executed instead of chrome::AttemptRelaunch()
  // when the user clicks "Restart now".
  static void set_relaunch_callback_for_testing(
      base::RepeatingClosure callback);
};

}  // namespace scheduled_restart

#endif  // CHROME_BROWSER_UI_VIEWS_SCHEDULED_RESTART_SCHEDULED_RESTART_BUBBLE_VIEW_H_
