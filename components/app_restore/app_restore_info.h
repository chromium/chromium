// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_APP_RESTORE_APP_RESTORE_INFO_H_
#define COMPONENTS_APP_RESTORE_APP_RESTORE_INFO_H_

#include <set>

#include "base/component_export.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"

class AccountId;

namespace aura {
class Window;
}

namespace views {
class Widget;
}

namespace app_restore {

// AppRestoreInfo is responsible for providing the information for
// AppRestoreInfo::Observer, including:
// 1. Whether we should restore apps and browser windows for |account_id|.
// 2. Notifies when the restore pref is changed for |account_id|.
// 3. Notifies when |window| is ready to be restored, after we have the app
// launch information, e.g. a task id for an ARC app
class COMPONENT_EXPORT(APP_RESTORE) AppRestoreInfo {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Notifies when the restore pref is changed. If the restore pref is 'Do not
    // restore', `could_restore` is false. Otherwise, `could_restore` is true,
    // for the pref 'Always' and 'Ask every time'.
    virtual void OnRestorePrefChanged(const AccountId& account_id,
                                      bool could_restore) {}

    // Notifies when |window| is ready to save the window info.
    //
    // When |window| is created, we might not have the app launch info yet. For
    // example, if the ARC task is not created, we don't have the launch info.
    // When the task is created, OnAppLaunched is called to notify observers to
    // save the window info.
    virtual void OnAppLaunched(aura::Window* window) {}

    // If |window| is restored, notifies observers to restore |window|, when
    // |window| has been initialized.
    //
    // For ARC app windows, when |window| is initialized, the task might not be
    // created yet, so we don't have the window info, |window| might be parent
    // to a hidden container based on the property kParentToHiddenContainerKey.
    virtual void OnWindowInitialized(aura::Window* window) {}

    // Called once the widget associated with an app restored window is
    // initialized. This is called sometime after OnWindowInitialized, and the
    // ARC task also may not be created yet at this point.
    virtual void OnWidgetInitialized(views::Widget* widget) {}

    // Called when `window` is ready to be parented to a valid desk container.
    //
    // For Lacros windows, called when `window` is associated with a Lacros
    // window id.
    //
    // For ARC app windows, called once a window which was created without an
    // associated task is now associated with a ARC task.
    virtual void OnParentWindowToValidContainer(aura::Window* window) {}

   protected:
    ~Observer() override = default;
  };

  static AppRestoreInfo* GetInstance();

  AppRestoreInfo();
  AppRestoreInfo(const AppRestoreInfo&) = delete;
  AppRestoreInfo& operator=(const AppRestoreInfo&) = delete;
  ~AppRestoreInfo();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns true if the restore pref is 'Always' or 'Ask every time', as we
  // could restore apps and pages based on the user's choice from the
  // notification for `account_id`. Otherwise, returns false, when the restore
  // pref is 'Do not restore'.
  bool CanPerformRestore(const AccountId& account_id);

  // Sets whether we could restore apps and pages, based on the restore pref
  // setting for `account_id`.
  void SetRestorePref(const AccountId& account_id, bool could_restore);

  // Notifies observers to observe |window| and restore or save the window info
  // for |window|.
  void OnAppLaunched(aura::Window* window);

  // Notifies observers that |window| has been initialized.
  void OnWindowInitialized(aura::Window* window);

  // Notifies observers that |widget| has been initialized.
  void OnWidgetInitialized(views::Widget* widget);

  // Notifies observers that `window` is ready to be parented to a valid desk
  // container..
  void OnParentWindowToValidContainer(aura::Window* window);

 private:
  base::ObserverList<Observer> observers_;

  // Records the restore pref. If the account id is not added, that means the
  // restore pref is 'Do not restore' for the account id. Otherwise, the restore
  // pref is 'Always' or 'Ask every time', and we could restore for the account
  // id.
  std::set<AccountId> restore_prefs_;
};

}  // namespace app_restore

#endif  // COMPONENTS_APP_RESTORE_APP_RESTORE_INFO_H_
