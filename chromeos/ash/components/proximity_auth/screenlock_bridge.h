// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_SCREENLOCK_BRIDGE_H_
#define CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_SCREENLOCK_BRIDGE_H_

#include <memory>
#include <string>

#include "base/lazy_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "chromeos/ash/components/proximity_auth/public/mojom/auth_type.mojom.h"
#include "components/account_id/account_id.h"

namespace ash {
enum class SmartLockState;
}  // namespace ash

namespace proximity_auth {

// TODO(jhawkins): Rationalize this class now that it is CrOS only and most of
// its functionality is not useful.
class ScreenlockBridge {
 public:
  class LockHandler {
   public:
    enum ScreenType { LOCK_SCREEN = 0, OTHER_SCREEN = 1 };

    // Displays |message| in a banner on the lock screen.
    virtual void ShowBannerMessage(const std::u16string& message,
                                   bool is_warning) = 0;

    // Update the status of Smart Lock for |account_id|.
    virtual void SetSmartLockState(const AccountId& account_id,
                                   ash::SmartLockState state) = 0;

    // Called after a Smart Lock authentication attempt has been made. If
    // |successful| is true, then the Smart Lock authentication attempt was
    // successful and the device should be unlocked. If false, an error message
    // should be shown to the user.
    virtual void NotifySmartLockAuthResult(const AccountId& account_id,
                                           bool successful) = 0;

    // (Re)enable lock screen UI.
    virtual void EnableInput() = 0;

    // Set the authentication type to be used on the lock screen.
    virtual void SetAuthType(const AccountId& account_id,
                             proximity_auth::mojom::AuthType auth_type,
                             const std::u16string& auth_value) = 0;

    // Returns the authentication type used for a user.
    virtual proximity_auth::mojom::AuthType GetAuthType(
        const AccountId& account_id) const = 0;

    // Returns the type of the screen -- a signin or a lock screen.
    virtual ScreenType GetScreenType() const = 0;

    // Unlocks from easy unlock app for a user.
    virtual void Unlock(const AccountId& account_id) = 0;

   protected:
    virtual ~LockHandler() {}
  };

  class Observer {
   public:
    // Invoked after the screen is locked.
    virtual void OnScreenDidLock() = 0;

    // Invoked after the screen lock is dismissed.
    virtual void OnScreenDidUnlock() = 0;

    // Invoked when the user focused on the lock screen changes.
    virtual void OnFocusedUserChanged(const AccountId& account_id) {}

   protected:
    virtual ~Observer() {}
  };

  static ScreenlockBridge* Get();

  ScreenlockBridge(const ScreenlockBridge&) = delete;
  ScreenlockBridge& operator=(const ScreenlockBridge&) = delete;

  void SetLockHandler(LockHandler* lock_handler);
  void SetFocusedUser(const AccountId& account_id);

  bool IsLocked() const;
  void Lock();

  // Unlocks the screen for the authenticated user with the given |user_id|.
  void Unlock(const AccountId& account_id);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  LockHandler* lock_handler() { return lock_handler_; }

  const AccountId& focused_account_id() const { return focused_account_id_; }

 private:
  friend struct base::LazyInstanceTraitsBase<ScreenlockBridge>;
  friend std::default_delete<ScreenlockBridge>;

  ScreenlockBridge();
  ~ScreenlockBridge();

  raw_ptr<LockHandler> lock_handler_ = nullptr;  // Not owned

  // The last focused user's id.
  AccountId focused_account_id_;
  base::ObserverList<Observer, true>::Unchecked observers_;
};

}  // namespace proximity_auth

#endif  // CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_SCREENLOCK_BRIDGE_H_
