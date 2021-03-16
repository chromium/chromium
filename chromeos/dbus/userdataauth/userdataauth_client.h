// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_USERDATAAUTH_USERDATAAUTH_CLIENT_H_
#define CHROMEOS_DBUS_USERDATAAUTH_USERDATAAUTH_CLIENT_H_

#include "base/callback.h"
#include "base/component_export.h"
#include "base/observer_list_types.h"
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
  class Observer : public base::CheckedObserver {
   public:
    // Called when LowDiskSpace signal is received, when the cryptohome
    // partition is running out of disk space.
    virtual void LowDiskSpace(const ::user_data_auth::LowDiskSpace& status) {}

    // Called when DircryptoMigrationProgress signal is received.
    // Typically, called periodically during a migration is performed by
    // cryptohomed, as well as to notify the completion of migration.
    virtual void DircryptoMigrationProgress(
        const ::user_data_auth::DircryptoMigrationProgress& progress) {}
  };

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

  // Adds an observer.
  virtual void AddObserver(Observer* observer) = 0;

  // Removes an observer if added.
  virtual void RemoveObserver(Observer* observer) = 0;

  // Actual DBus Methods:

  // Runs the callback as soon as the service becomes available.
  virtual void WaitForServiceToBeAvailable(
      WaitForServiceToBeAvailableCallback callback) = 0;

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
