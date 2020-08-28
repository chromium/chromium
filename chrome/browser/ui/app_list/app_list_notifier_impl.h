// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_NOTIFIER_IMPL_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_NOTIFIER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_controller_observer.h"
#include "ash/public/cpp/app_list/app_list_notifier.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"

namespace base {
class OneShotTimer;
}

namespace ash {
class AppListController;
}

// Chrome implementation of the AppListNotifier. This is mainly responsible for
// translating notifications about launcher UI events - eg. launcher opened,
// results changed, etc. - into impression, launch, and abandon notifications
// for observers. See the header comment in app_list_notifier.h for definitions
// of these.
//
// This handles results on three UI views: the suggestion chips, app tiles,
// and results list. These are just called _views_ in this comment.
//
// TODO(crbug.com/1076270): NotifyLaunch is incorrectly passed kTile when an app
// chip is launched. This should be fixed at the NotifyLaunch call site.
//
// State machine
// =============
//
// This class implements three state machines, one for each view,that are
// mostly independent. Each state machine can be in one of three primary
// states:
//
//  - kNone: the view is not displayed.
//  - kShown: the view is displayed.
//  - kSeen: the same results on the view have been displayed for a certain
//           amount of time.
//
// There are two extra 'transitional' states:
//
//  - kLaunched: a user has launched a result in this surface.
//  - kIgnored: a user has launched a result in another visible surface.
//
// These states exist only to simplify implementation, and the state machine
// doesn't stay in them for any length of time. Immediately after a transition
// to kLaunch or kIgnore, the launcher closes and the state is set to kNone.
//
// Various user and background events cause _transitions_ between states. The
// notifier performs _actions_ on a transition based on the (from, to) pair of
// states.
//
// Each state machine is associated with an _impression timer_ that begins on a
// transition to kShown. Once the timer finishes, it causes a transition to
// kSeen.
//
// Transitions
// ===========
//
// The events that cause a transition to a state are as follows.
//
//  - kNone: closing the launcher or moving to a different view.
//  - kShown: opening the relevant view, or changing the search query if the
//            view is the app tiles or results list.
//  - kLaunched: launching a result.
//  - kSeen: the impression timer finishing for the view.
//
// Actions
// =======
//
// These actions are triggered on a state transition.
//
//  From -> To         | Actions
//  -------------------|--------------------------------------------------------
//  kNone -> kNone     | No action.
//                     |
//  kNone -> kShown    | Start impression timer, as view just displayed.
//                     |
//  kShown -> kNone    | Cancel impression timer, as view changed.
//                     |
//  kShown -> kSeen    | Notify of an impression, as impression timer finished.
//                     |
//  kShown -> kShown   | Restart impression timer. Only possible for the app
//                     | tiles or results list, when the search query is
//                     | updated. This should not be triggered unless the
//                     | displayed results change.
//                     |
//                     |
//  kSeen -> kLaunch   | Notify of a launch and immediately set state to kNone,
//                     | as user launched a result.
//                     |
//  kShown -> kLaunch  | Notify of a launch and impression then set state to
//                     | kNone and cancel impression timer, as user launched
//                     | result so must have seen the results.
//                     |
//                     |
//  kShown -> kIgnored | Notify of an ignore and impression, then set state to
//                     | kNone and cancel timer, as user launched a result in
//                     | a different view to must have seen the results.
//                     |
//  kSeen -> kIgnored  | Notify of an ignore, then set state to kNone, as user
//                     | launched a result in a different view.
//                     |
//                     |
//  kSeen -> kNone     | Notify of an abandon, as user closed launcher.
//                     |
//  kSeen -> kShown    | Notify of an abandon and restart timer, as user saw
//                     | results but changed view or updated the search query.
//
// The transitions we consider impossible are kNone -> {kSeen, kLaunch},
// kSeen -> kSeen, and {kLaunch, kIgnored } -> anything, because kLaunch and
// kIgnored are temporary states.
//
// Discussion
// ==========
//
// Warning: NotifyResultsUpdated cannot be used as a signal of user actions or
// UI state. Results can be updated at any time for any UI view, regardless
// of the state of the launcher or what the user is doing.
class AppListNotifierImpl : public ash::AppListNotifier,
                            public ash::AppListControllerObserver {
 public:
  explicit AppListNotifierImpl(ash::AppListController* app_list_controller);
  ~AppListNotifierImpl() override;

  AppListNotifierImpl(const AppListNotifierImpl&) = delete;
  AppListNotifierImpl& operator=(const AppListNotifierImpl&) = delete;

  // AppListNotifier:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void NotifyLaunched(Location location, const Result& result) override;
  void NotifyResultsUpdated(Location location,
                            const std::vector<Result>& results) override;
  void NotifySearchQueryChanged(const base::string16& query) override;
  void NotifyUIStateChanged(ash::AppListViewState view) override;

  // AppListControllerObserver:
  void OnAppListVisibilityWillChange(bool shown, int64_t display_id) override;

 private:
  // Possible states of the state machine.
  enum class State {
    kNone,
    kShown,
    kSeen,
    kLaunched,
    kIgnored,
  };

  // Performs all state transition logic.
  void DoStateTransition(Location location, State new_state);

  // (Re)starts the impression timer for |location|.
  void RestartTimer(Location location);

  // Stops the impression timer for |location|.
  void StopTimer(Location location);

  // Handles a finished impression timer for |location|.
  void OnTimerFinished(Location location);

  ash::AppListController* const app_list_controller_;

  base::ObserverList<Observer> observers_;

  // The current state of each state machine.
  base::flat_map<Location, State> states_;
  // An impression timer for each state machine.
  base::flat_map<Location, std::unique_ptr<base::OneShotTimer>> timers_;

  // Whether or not the app list is shown.
  bool shown_ = false;
  // The current UI view. Can have a non-kClosed value when the app list is not
  // |shown_| due to tablet mode.
  ash::AppListViewState view_ = ash::AppListViewState::kClosed;
  // The currently shown results for each UI view.
  base::flat_map<Location, std::vector<Result>> results_;
  // The current search query, may be empty.
  base::string16 query_;
  // The most recently launched result.
  base::Optional<Result> launched_result_;

  base::WeakPtrFactory<AppListNotifierImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_NOTIFIER_IMPL_H_
