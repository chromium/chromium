// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_CHROMEBOX_FOR_MEETINGS_CFM_HOTLINE_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_CHROMEBOX_FOR_MEETINGS_CFM_HOTLINE_CLIENT_H_

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback_forward.h"
#include "dbus/object_proxy.h"

#include "chromeos/ash/components/dbus/chromebox_for_meetings/cfm_observer.h"

namespace dbus {
class Bus;
}

namespace ash {

// CfmHotlineClient is used to communicate with the hotline system daemon.
// The only purpose of the D-Bus service is to bootstrap a Mojo IPC
// connection. All methods should be called from the origin thread (UI thread)
// which initializes the DBusThreadManager instance.
class COMPONENT_EXPORT(CFM_HOTLINE_CLIENT) CfmHotlineClient {
 public:
  using BootstrapMojoConnectionCallback = base::OnceCallback<void(bool)>;

  CfmHotlineClient(const CfmHotlineClient&) = delete;
  CfmHotlineClient& operator=(const CfmHotlineClient&) = delete;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Checks if initialization was performed
  static bool IsInitialized();

  // Returns the global instance which may be null if not initialized.
  static CfmHotlineClient* Get();

  // Registers |callback| to run when the hotline daemon becomes available.
  // If the daemon is already available, or if connecting to the name-owner-
  // changed signal fails, |callback| will be run once asynchronously.
  // Otherwise, |callback| will be run once in the future after the service
  // becomes available.
  virtual void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) = 0;

  // Passes the file descriptor |fd| over D-Bus to the hotline daemon.
  // * The daemon expects a Mojo invitation in |fd| with an attached Mojo pipe.
  // * The daemon will bind the Mojo pipe to an implementation of
  //   chromeos::cfm::mojom::CfmServiceContext
  // * Upon completion of the D-Bus call, |result_callback| will be invoked to
  //   returning if the operation was successful or not.
  // * This method will first wait for the daemon to become
  //   available.
  virtual void BootstrapMojoConnection(
      base::ScopedFD fd,
      BootstrapMojoConnectionCallback result_callback) = 0;

  // Adds an observer instance to the observers list to listen on changes like
  // DLC state change, etc.
  virtual void AddObserver(cfm::CfmObserver* observer) = 0;

  // Removes an observer from observers list.
  virtual void RemoveObserver(cfm::CfmObserver* observer) = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  CfmHotlineClient();
  virtual ~CfmHotlineClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_CHROMEBOX_FOR_MEETINGS_CFM_HOTLINE_CLIENT_H_
