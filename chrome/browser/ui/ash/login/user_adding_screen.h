// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LOGIN_USER_ADDING_SCREEN_H_
#define CHROME_BROWSER_UI_ASH_LOGIN_USER_ADDING_SCREEN_H_

namespace ash {

// An interface that defines screen for adding users into multi-profile session.
// Current implementation is a singleton.
// TODO(dzhioev): get rid of singleton.
class UserAddingScreen {
 public:
  struct Observer {
    virtual void OnBeforeUserAddingScreenStarted() {}
    virtual void OnUserAddingFinished() {}
    virtual ~Observer() {}
  };

  UserAddingScreen(const UserAddingScreen&) = delete;
  UserAddingScreen& operator=(const UserAddingScreen&) = delete;

  static UserAddingScreen* Get();

  virtual void Start() = 0;
  virtual void Cancel() = 0;
  virtual bool IsRunning() = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

 protected:
  UserAddingScreen();
  virtual ~UserAddingScreen();
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_LOGIN_USER_ADDING_SCREEN_H_
