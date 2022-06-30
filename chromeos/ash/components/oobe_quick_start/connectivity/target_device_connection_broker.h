// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OOBE_QUICK_START_CONNECTIVITY_TARGET_DEVICE_CONNECTION_BROKER_H_
#define CHROMEOS_ASH_COMPONENTS_OOBE_QUICK_START_CONNECTIVITY_TARGET_DEVICE_CONNECTION_BROKER_H_

#include "base/callback.h"

namespace ash::quick_start {

// TargetDeviceConnectionBroker is the entrypoint for consuming the Quick Start
// connectivity component. Calling code is expected to get an instance of this
// class using the TargetDeviceConnectionBrokerFactory and interact with the
// component using the public interface of this class.
class TargetDeviceConnectionBroker {
 public:
  using ResultCallback = base::OnceCallback<void(bool success)>;

  enum class FeatureSupportStatus {
    kUndetermined = 0,
    kNotSupported,
    kSupported
  };

  // Clients of TargetDeviceConnectionBroker should implement this interface,
  // and provide a self-reference when calling TargetDeviceConnectionBroker::
  // StartAdvertising().
  //
  // This interface is a simplification of
  // location::nearby::connections::mojom::ConnectionLifecycleListener, for ease
  // of client use.
  //
  // TODO(b/234655072): Define this interface
  class ConnectionLifecycleListener {};

  TargetDeviceConnectionBroker() = default;
  virtual ~TargetDeviceConnectionBroker() = default;

  // Checks to see whether the feature can be supported on the device's
  // hardware. The feature is supported if Bluetooth is supported and an adapter
  // is present.
  virtual FeatureSupportStatus GetFeatureSupportStatus() const = 0;

  // Will kick off Fast Pair and Nearby Connections advertising.
  // Clients can use the result of |on_start_advertising_callback| to
  // immediately understand if advertising succeeded, and can then wait for the
  // source device to connect via
  // |ConnectionLifecycleListener::OnUnacceptedConnectionInitiated()|.
  //
  // If the caller paused a connection previously, the connection to the
  // source device will resume via OnConnectionAccepted().
  // Clients should check  GetFeatureSupportStatus()  before calling
  // StartAdvertising().
  virtual void StartAdvertising(
      ConnectionLifecycleListener* listener,
      ResultCallback on_start_advertising_callback) = 0;

  // Clients are responsible for calling this once they have accepted their
  // desired connection, or in error/edge cases, e.g., the user exits the UI.
  virtual void StopAdvertising(
      base::OnceClosure on_stop_advertising_callback) = 0;
};

}  // namespace ash::quick_start

#endif  // CHROMEOS_ASH_COMPONENTS_OOBE_QUICK_START_CONNECTIVITY_TARGET_DEVICE_CONNECTION_BROKER_H_
