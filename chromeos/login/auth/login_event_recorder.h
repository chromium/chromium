// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LOGIN_AUTH_LOGIN_EVENT_RECORDER_H_
#define CHROMEOS_LOGIN_AUTH_LOGIN_EVENT_RECORDER_H_

#include "base/component_export.h"
#include "base/macros.h"

namespace chromeos {

// LoginEventRecorder records the bootimes of Chrome OS.
// Actual implementation is handled by delegate.
class COMPONENT_EXPORT(CHROMEOS_LOGIN_AUTH) LoginEventRecorder {
 public:
  class Delegate {
   public:
    // Add a time marker for login. A timeline will be dumped to
    // /tmp/login-times-sent after login is done. If |send_to_uma| is true
    // the time between this marker and the last will be sent to UMA with
    // the identifier BootTime.|marker_name|.
    virtual void AddLoginTimeMarker(const char* marker_name,
                                    bool send_to_uma) = 0;

    // Record events for successful authentication.
    virtual void RecordAuthenticationSuccess() = 0;

    // Record events for authentication failure.
    virtual void RecordAuthenticationFailure() = 0;
  };
  LoginEventRecorder();

  LoginEventRecorder(const LoginEventRecorder&) = delete;
  LoginEventRecorder& operator=(const LoginEventRecorder&) = delete;

  virtual ~LoginEventRecorder();

  static LoginEventRecorder* Get();

  void SetDelegate(Delegate* delegate);

  // Add a time marker for login. A timeline will be dumped to
  // /tmp/login-times-sent after login is done. If |send_to_uma| is true
  // the time between this marker and the last will be sent to UMA with
  // the identifier BootTime.|marker_name|.
  void AddLoginTimeMarker(const char* marker_name, bool send_to_uma);

  // Record events for successful authentication.
  void RecordAuthenticationSuccess();

  // Record events for authentication failure.
  void RecordAuthenticationFailure();

 private:
  Delegate* delegate_;
};

}  // namespace chromeos

#endif  // CHROMEOS_LOGIN_AUTH_LOGIN_EVENT_RECORDER_H_
