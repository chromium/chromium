// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_LOGIN_LOGIN_SCREEN_SHOWN_OBSERVER_H_
#define CHROMEOS_ASH_EXPERIENCES_LOGIN_LOGIN_SCREEN_SHOWN_OBSERVER_H_

#include "base/observer_list_types.h"

// A class that observes when the login screen is shown.
class LoginScreenShownObserver : public base::CheckedObserver {
 public:
  LoginScreenShownObserver() {}
  LoginScreenShownObserver(const LoginScreenShownObserver&) = delete;
  LoginScreenShownObserver& operator=(const LoginScreenShownObserver&) = delete;

  virtual void OnLoginScreenShown() = 0;
};

#endif  // CHROMEOS_ASH_EXPERIENCES_LOGIN_LOGIN_SCREEN_SHOWN_OBSERVER_H_
