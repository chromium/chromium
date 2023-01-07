// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_REOPEN_TAB_IN_PRODUCT_HELP_TRIGGER_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_REOPEN_TAB_IN_PRODUCT_HELP_TRIGGER_H_

#include <map>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"

namespace feature_engagement {
class Tracker;
}  // namespace feature_engagement

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

  ReopenTabInProductHelpTrigger(const ReopenTabInProductHelpTrigger&) = delete;
  ReopenTabInProductHelpTrigger& operator=(
      const ReopenTabInProductHelpTrigger&) = delete;

  ~ReopenTabInProductHelpTrigger();

  using ShowHelpCallback = base::RepeatingCallback<void()>;

  // Sets callback to be called when IPH should be displayed. IPH must be
  // displayed when the callback is called, and |HelpDismissed()| must be called
  // when finished. The owner must ensure a valid callback is set before any
  // other methods are called.
  void SetShowHelpCallback(ShowHelpCallback callback);

  // Should be called when an active tab is closed.
  void ActiveTabClosed(base::TimeDelta active_duration);

  // Should be called when a blank new tab is opened by user action. Possibly
  // triggers IPH.
  void NewTabOpened();

  static std::map<std::string, std::string> GetFieldTrialParamsForTest(
      int tab_minimum_active_duration_seconds,
      int new_tab_opened_timeout_seconds);

 private:
  // Sets state as if user has not performed any actions.
  void ResetTriggerState();

  const raw_ptr<feature_engagement::Tracker> tracker_;
  const raw_ptr<const base::TickClock> clock_;

  ShowHelpCallback cb_;

  const base::TimeDelta tab_minimum_active_duration_;
  const base::TimeDelta new_tab_opened_timeout_;

  enum TriggerState {
    NO_ACTIONS_SEEN,
    ACTIVE_TAB_CLOSED,
  };

  TriggerState trigger_state_;

  base::TimeTicks time_of_last_step_;
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_REOPEN_TAB_IN_PRODUCT_HELP_TRIGGER_H_
