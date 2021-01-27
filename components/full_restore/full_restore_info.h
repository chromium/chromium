// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FULL_RESTORE_FULL_RESTORE_INFO_H_
#define COMPONENTS_FULL_RESTORE_FULL_RESTORE_INFO_H_

#include <set>

#include "base/component_export.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"

class AccountId;

namespace aura {
class Window;
}

namespace full_restore {

// FullRestoreInfo is responsible for providing the information for
// FullRestoreInfo::Observer, including:
// 1. Whether we should restore apps and browser windows for |account_id|.
// 2. Notifies when |window| is ready to be restored, after we have the app
// launch information, e.g. a task id for an ARC app
//
// TODO(crbug.com/1146900): Get the app launch information, and notify
// observers.
class COMPONENT_EXPORT(FULL_RESTORE) FullRestoreInfo {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Notifies when |restore_flags_| is changed.
    virtual void OnRestoreFlagChanged(const AccountId& account_id,
                                      bool should_restore) {}

    // Notifies when |window| is ready to be restored, after we have the app
    // launch information, e.g. a task id for an ARC app.
    virtual void OnAppLaunched(aura::Window* window) {}

   protected:
    ~Observer() override = default;
  };

  static FullRestoreInfo* GetInstance();

  FullRestoreInfo();
  FullRestoreInfo(const FullRestoreInfo&) = delete;
  FullRestoreInfo& operator=(const FullRestoreInfo&) = delete;
  ~FullRestoreInfo();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns true if we should restore apps and pages based on the restore
  // setting and the user's choice from the notification for |account_id|.
  // Otherwise, returns false.
  bool ShouldRestore(const AccountId& account_id);

  // Sets whether we should restore apps and pages, based on the restore setting
  // and the user's choice from the notification for |account_id|.
  void SetRestoreFlag(const AccountId& account_id, bool should_restore);

  // Notifies observers to observe |window| and restore or save the window info
  // for |window|.
  void OnAppLaunched(aura::Window* window);

 private:
  base::ObserverList<Observer> observers_;

  // Records whether restore or not for the account id. If the account id is
  // added, that means we should restore apps and pages for the account id.
  // Otherwise, we should not restore for the account id.
  std::set<AccountId> restore_flags_;
};

}  // namespace full_restore

#endif  // COMPONENTS_FULL_RESTORE_FULL_RESTORE_INFO_H_
