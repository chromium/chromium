// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_PRIVATE_COMPUTING_PRIVATE_COMPUTING_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_PRIVATE_COMPUTING_PRIVATE_COMPUTING_CLIENT_H_

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "chromeos/ash/components/dbus/private_computing/private_computing_service.pb.h"
#include "dbus/object_proxy.h"

namespace dbus {
class Bus;
}  // namespace dbus

namespace ash {

// PrivateComputingClient is used to communicate with the Chrome OS Private
// Computing DBus daemon.
// For example, this client will communicate over DBus to write to the
// preserved file - last active dates, which the chronos browser user normally
// doesn't have access to.
class COMPONENT_EXPORT(PRIVATE_COMPUTING) PrivateComputingClient {
 public:
  using SaveStatusCallback = base::OnceCallback<void(
      const private_computing::SaveStatusResponse response)>;
  using GetStatusCallback = base::OnceCallback<void(
      const private_computing::GetStatusResponse response)>;

  using SaveStatusCall =
      base::RepeatingCallback<void(const private_computing::SaveStatusRequest,
                                   SaveStatusCallback)>;
  using GetStatusCall = base::RepeatingCallback<void(GetStatusCallback)>;

  // Interface with testing functionality. Accessed through
  // GetTestInterface(), only implemented in the fake implementation.
  class TestInterface {
   public:
    // Sets SaveStatusResponse proto.
    virtual void SetSaveLastPingDatesStatusResponse(
        private_computing::SaveStatusResponse response) = 0;

    // Sets GetStatusResponse proto.
    virtual void SetGetLastPingDatesStatusResponse(
        private_computing::GetStatusResponse response) = 0;

   protected:
    virtual ~TestInterface() = default;
  };

  PrivateComputingClient(const PrivateComputingClient&) = delete;
  PrivateComputingClient& operator=(const PrivateComputingClient&) = delete;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance which must have been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static PrivateComputingClient* Get();

  // SaveLastPingDatesStatus requests the private computing DBus daemon to
  // write to the preserved file path to store last ping UTC dates by use
  // case. Returns a proto representing whether operation was successful.
  virtual void SaveLastPingDatesStatus(
      const private_computing::SaveStatusRequest& request,
      SaveStatusCallback callback) = 0;

  // GetLastPingDatesStatus requests the private computing DBus daemon to
  // retrieve the last ping UTC dates for each use case.
  // Returns a response proto containing the last ping UTC dates or a string
  // representing the error.
  virtual void GetLastPingDatesStatus(GetStatusCallback callback) = 0;

  // Returns an interface for testing (fake only), or returns nullptr.
  virtual TestInterface* GetTestInterface() = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  PrivateComputingClient();
  virtual ~PrivateComputingClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_PRIVATE_COMPUTING_PRIVATE_COMPUTING_CLIENT_H_
