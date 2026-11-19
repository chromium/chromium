// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LOGIN_USER_ADDING_SCREEN_H_
#define CHROME_BROWSER_UI_ASH_LOGIN_USER_ADDING_SCREEN_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/ash/login/user_adding_screen_input_methods_controller.h"
#include "components/account_id/account_id.h"

namespace ash {

class LoginDisplayHost;

// Screen for adding users into a multi-profile session.
class UserAddingScreen {
 public:
  struct Observer {
    virtual void OnBeforeUserAddingScreenStarted() {}
    virtual void OnUserAddingFinished() {}
    virtual ~Observer() = default;
  };

  // Only one instance may exist at a time.
  UserAddingScreen();
  UserAddingScreen(const UserAddingScreen&) = delete;
  UserAddingScreen& operator=(const UserAddingScreen&) = delete;
  ~UserAddingScreen();

  // Returns the global instance.
  static UserAddingScreen* Get();

  void Start();
  void Cancel();
  bool IsRunning() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  void OnDisplayHostCompletion();

  base::ObserverList<Observer>::UncheckedAndDanglingUntriaged observers_;
  raw_ptr<LoginDisplayHost, DanglingUntriaged> display_host_ = nullptr;

  UserAddingScreenInputMethodsController im_controller_;

  // The user whose session the screen was opened on top of, so that Cancel()
  // can put their wallpaper back and the input methods controller can tell
  // whether a different user is active once the screen is gone. Empty when
  // the screen was opened from the login screen, where no session exists.
  std::optional<AccountId> interrupted_user_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_LOGIN_USER_ADDING_SCREEN_H_
