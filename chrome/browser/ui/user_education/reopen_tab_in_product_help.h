// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_REOPEN_TAB_IN_PRODUCT_HELP_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_REOPEN_TAB_IN_PRODUCT_HELP_H_

#include "base/memory/raw_ptr.h"
#include "base/time/tick_clock.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/user_education/active_tab_tracker.h"
#include "chrome/browser/ui/user_education/reopen_tab_in_product_help_trigger.h"
#include "components/keyed_service/core/keyed_service.h"

namespace base {
class TickClock;
}

namespace feature_engagement {
class Tracker;
}

class TabStripModel;

// Listens for the triggering conditions for the reopen tab in-product help and
// starts the IPH flow at the appropriate time. This is a |Profile|-keyed
// service since we track interactions per user profile. Hooks throughout the
// browser UI code will fetch this service and notify it of interesting user
// actions.
class ReopenTabInProductHelp : public BrowserListObserver, public KeyedService {
 public:
  ReopenTabInProductHelp(Profile* profile, const base::TickClock* clock);

  ReopenTabInProductHelp(const ReopenTabInProductHelp&) = delete;
  ReopenTabInProductHelp& operator=(const ReopenTabInProductHelp&) = delete;

  ~ReopenTabInProductHelp() override;

  // Should be called when the user opens a blank new tab. Possibly triggers
  // IPH.
  void NewTabOpened();

  // Should be called when the user reopens a previously closed tab, either
  // through CTRL+SHIFT+T or through the recent tabs menu.
  void TabReopened();

 private:
  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  feature_engagement::Tracker* GetTracker();

  // Callback passed to the |ActiveTabTracker|.
  void OnActiveTabClosed(TabStripModel* tab_strip_model,
                         base::TimeDelta active_duration);

  // Callback passed to |ReopenTabInProductHelpTrigger|.
  void OnShowHelp();

  const raw_ptr<Profile> profile_;
  const raw_ptr<const base::TickClock> clock_;

  // Tracks active tab durations and notifies us when an active tab is closed.
  ActiveTabTracker active_tab_tracker_;
  // Manages the triggering logic for this IPH. This object calls into the IPH
  // backend.
  ReopenTabInProductHelpTrigger trigger_;
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_REOPEN_TAB_IN_PRODUCT_HELP_H_
