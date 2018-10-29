// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_IN_PRODUCT_HELP_REOPEN_TAB_IN_PRODUCT_HELP_TRIGGER_H_
#define CHROME_BROWSER_UI_IN_PRODUCT_HELP_REOPEN_TAB_IN_PRODUCT_HELP_TRIGGER_H_

#include "base/callback.h"
#include "base/time/tick_clock.h"

namespace feature_engagement {
class Tracker;
}  // namespace feature_engagement

namespace in_product_help {

// Contains the triggering logic for the reopen closed tab IPH. Determines when
// a user might have accidentally closed a tab based on user interactions
// reported to it. When this happens, this class asks the feature engagement
// backend whether to display IPH. If IPH should be displayed, this class
// notifies its client.
//
// Clients should listen for the relevant user events and pass them to this
// class. Additionally, clients must display IPH when told by this class.
class ReopenTabInProductHelpTrigger {
 public:
  ReopenTabInProductHelpTrigger(feature_engagement::Tracker* tracker,
                                const base::TickClock* clock);
  ~ReopenTabInProductHelpTrigger();

  using ShowHelpCallback = base::RepeatingCallback<void()>;

  // Sets callback to be called when IPH should be displayed. IPH must be
  // displayed when the callback is called, and |HelpDismissed()| must be called
  // when finished. The owner must ensure a valid callback is set before any
  // other methods are called.
  void SetShowHelpCallback(ShowHelpCallback callback);

  // Should be called when an active tab is closed.
  void ActiveTabClosed(base::TimeDelta active_duration);

  // Should be called when a blank new tab is opened by user action.
  void NewTabOpened();

  // Should be called when the user focuses on the omnibox. Possibly triggers
  // IPH.
  void OmniboxFocused();

  // Must be called once after IPH finishes. Must only be called after the
  // callback is called.
  void HelpDismissed();

  // Timeout constants. Exposed for unit testing.
  static const base::TimeDelta kTabMinimumActiveDuration;
  static const base::TimeDelta kNewTabOpenedTimeout;
  static const base::TimeDelta kOmniboxFocusedTimeout;

 private:
  // Sets state as if user has not performed any actions.
  void ResetTriggerState();

  feature_engagement::Tracker* const tracker_;
  const base::TickClock* const clock_;

  ShowHelpCallback cb_;

  enum TriggerState {
    NO_ACTIONS_SEEN,
    ACTIVE_TAB_CLOSED,
    NEW_TAB_OPENED,
  };

  TriggerState trigger_state_;

  base::TimeTicks time_of_last_step_;

  DISALLOW_COPY_AND_ASSIGN(ReopenTabInProductHelpTrigger);
};

}  // namespace in_product_help

#endif  // CHROME_BROWSER_UI_IN_PRODUCT_HELP_REOPEN_TAB_IN_PRODUCT_HELP_TRIGGER_H_
