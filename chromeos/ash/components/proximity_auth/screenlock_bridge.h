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

// TODO(tbarzic): Rename ScreenlockBridge to SignInScreenBridge, as this is not
// used solely for the lock screen anymore.
// TODO(jhawkins): Rationalize this class now that it is CrOS only and most of
// its functionality is not useful.
class ScreenlockBridge {
 public:
  // User pod icons supported by lock screen / signin screen UI.
  enum UserPodCustomIcon {
    USER_POD_CUSTOM_ICON_NONE,
    USER_POD_CUSTOM_ICON_LOCKED,
    USER_POD_CUSTOM_ICON_LOCKED_TO_BE_ACTIVATED,
    // TODO(isherman): The "locked with proximity hint" icon is currently the
    // same as the "locked" icon. It's treated as a separate case to allow an
    // easy asset swap without changing the code, in case we decide to use a
    // different icon for this case. If we definitely decide against that, then
    // this enum entry should be removed.
    USER_POD_CUSTOM_ICON_LOCKED_WITH_PROXIMITY_HINT,
    USER_POD_CUSTOM_ICON_UNLOCKED,
    USER_POD_CUSTOM_ICON_SPINNER
  };

  // Class containing parameters describing the custom icon that should be
  // shown on a user's screen lock pod next to the input field.
  class UserPodCustomIconInfo {
   public:
    UserPodCustomIconInfo();
    UserPodCustomIconInfo(const UserPodCustomIconInfo&) = delete;
    UserPodCustomIconInfo& operator=(const UserPodCustomIconInfo&) = delete;
    ~UserPodCustomIconInfo();

    // Converts parameters to a dictionary values that can be sent to the
    // screenlock web UI.
    base::Value::Dict ToDictForTesting() const;

    // Sets the icon that should be shown in the UI.
    void SetIcon(UserPodCustomIcon icon);

    // Sets the icon tooltip. If |autoshow| is set the tooltip is automatically
    // shown with the icon.
    void SetTooltip(const std::u16string& tooltip, bool autoshow);

    // Sets the accessibility label of the icon. If this attribute is not
    // provided, then the tooltip will be used.
    void SetAriaLabel(const std::u16string& aria_label);

    std::string GetIDString() const;

    UserPodCustomIcon icon() const { return icon_; }

    const std::u16string tooltip() const { return tooltip_; }

    bool autoshow_tooltip() const { return autoshow_tooltip_; }

    const std::u16string aria_label() const { return aria_label_; }

   private:
    UserPodCustomIcon icon_;

    std::u16string tooltip_;
    bool autoshow_tooltip_ = false;

    std::u16string aria_label_;
  };

  class LockHandler {
   public:
    enum ScreenType { SIGNIN_SCREEN = 0, LOCK_SCREEN = 1, OTHER_SCREEN = 2 };

    // Displays |message| in a banner on the lock screen.
    virtual void ShowBannerMessage(const std::u16string& message,
                                   bool is_warning) = 0;

    // Shows a custom icon in the user pod on the lock screen.
    virtual void ShowUserPodCustomIcon(
        const AccountId& account_id,
        const UserPodCustomIconInfo& icon_info) = 0;

    // Hides the custom icon in user pod for a user.
    virtual void HideUserPodCustomIcon(const AccountId& account_id) = 0;

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
    virtual void OnScreenDidLock(LockHandler::ScreenType screen_type) = 0;

    // Invoked after the screen lock is dismissed.
    virtual void OnScreenDidUnlock(LockHandler::ScreenType screen_type) = 0;

    // Invoked when the user focused on the lock screen changes.
    virtual void OnFocusedUserChanged(const AccountId& account_id) = 0;

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

  raw_ptr<LockHandler, ExperimentalAsh> lock_handler_ = nullptr;  // Not owned

  // The last focused user's id.
  AccountId focused_account_id_;
  base::ObserverList<Observer, true>::Unchecked observers_;
};

}  // namespace proximity_auth

#endif  // CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_SCREENLOCK_BRIDGE_H_
