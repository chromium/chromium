// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_USERDATAAUTH_USERDATAAUTH_CLIENT_H_
#define CHROMEOS_DBUS_USERDATAAUTH_USERDATAAUTH_CLIENT_H_

#include "base/callback.h"
#include "base/component_export.h"
#include "chromeos/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/dbus/dbus_method_call_status.h"

namespace dbus {
class Bus;
}

namespace chromeos {

// UserDataAuthClient is used to communicate with the org.chromium.UserDataAuth
// service exposed by cryptohomed. All method should be called from the origin
// thread (UI thread) which initializes the DBusThreadManager instance.
class COMPONENT_EXPORT(USERDATAAUTH_CLIENT) UserDataAuthClient {
 public:
  using IsMountedCallback =
      DBusMethodCallback<::user_data_auth::IsMountedReply>;

  // Not copyable or movable.
  UserDataAuthClient(const UserDataAuthClient&) = delete;
  UserDataAuthClient& operator=(const UserDataAuthClient&) = delete;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static UserDataAuthClient* Get();

  // Actual DBus Methods:

  // Queries if user's vault is mounted.
  virtual void IsMounted(const ::user_data_auth::IsMountedRequest& request,
                         IsMountedCallback callback) = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  UserDataAuthClient();
  virtual ~UserDataAuthClient();
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_USERDATAAUTH_USERDATAAUTH_CLIENT_H_
