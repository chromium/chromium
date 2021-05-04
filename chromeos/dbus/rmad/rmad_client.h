// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_RMAD_RMAD_CLIENT_H_
#define CHROMEOS_DBUS_RMAD_RMAD_CLIENT_H_

#include "base/component_export.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/rmad/rmad.pb.h"

namespace dbus {
class Bus;
}

namespace chromeos {

// RmadClient is responsible for receiving D-bus signals from the RmaDaemon
// service. The RmaDaemon is the underlying service that informs us whenever
// a shimless RMA is in progress and manages its state.
// Shimless RMA implements repair finalization for devices without the use of
// the USB shim. See go/cros-shimless-rma for details.
class COMPONENT_EXPORT(RMAD) RmadClient {
 public:
  // Creates and initializes a global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static RmadClient* Get();

  // Asynchronously gets the current RMA state.
  // The response contains an error code and the current state of the RMA
  // process.
  virtual void GetCurrentState(
      DBusMethodCallback<rmad::GetStateReply> callback) = 0;
  // Asynchronously attempts to transition to the next RMA state.
  // The response contains an error code and the current state of the RMA
  // process.
  virtual void TransitionNextState(
      const rmad::RmadState& state,
      DBusMethodCallback<rmad::GetStateReply> callback) = 0;
  // Asynchronously attempts to transition to the previous RMA state.
  // The response contains an error code and the current state of the RMA
  // process.
  virtual void TransitionPreviousState(
      DBusMethodCallback<rmad::GetStateReply> callback) = 0;

  // Request the RMA process be cancelled.
  // There is no guarantee the callback is called if abort is successful because
  // the device will reboot.
  // Returns RMAD_ERROR_OK on success or an error code.
  virtual void AbortRma(DBusMethodCallback<rmad::AbortRmaReply> callback) = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  RmadClient();

  RmadClient(const RmadClient&) = delete;
  RmadClient& operator=(const RmadClient&) = delete;
  virtual ~RmadClient();
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_RMAD_RMAD_CLIENT_H_
