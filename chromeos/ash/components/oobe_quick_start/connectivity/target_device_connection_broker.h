// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OOBE_QUICK_START_CONNECTIVITY_TARGET_DEVICE_CONNECTION_BROKER_H_
#define CHROMEOS_ASH_COMPONENTS_OOBE_QUICK_START_CONNECTIVITY_TARGET_DEVICE_CONNECTION_BROKER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"

namespace ash::quick_start {

// TargetDeviceConnectionBroker is the entrypoint for consuming the Quick Start
// connectivity component. Calling code is expected to get an instance of this
// class using the TargetDeviceConnectionBrokerFactory and interact with the
// component using the public interface of this class.
//
// All references to "target device" imply this device (Chromebook). All
// references to "source device" imply the remote Android phone, which is the
// source for Gaia and WiFi credentials.
class TargetDeviceConnectionBroker {
 public:
  using ResultCallback = base::OnceCallback<void(bool success)>;

  enum class FeatureSupportStatus {
    kUndetermined = 0,
    kNotSupported,
    kSupported
  };

  class Connection {};

  // Represents a new incoming connection that has not yet been accepted by the
  // source device.
  class IncomingConnection : public Connection {};

  // Represents an accepted (authenticated) connection.
  class AcceptedConnection : public Connection {};

  // Clients of TargetDeviceConnectionBroker should implement this interface,
  // and provide a self-reference when calling TargetDeviceConnectionBroker::
  // StartAdvertising().
  //
  // This interface is a simplification of
  // location::nearby::connections::mojom::ConnectionLifecycleListener, for ease
  // of client use.
  class ConnectionLifecycleListener {
   public:
    ConnectionLifecycleListener() = default;
    virtual ~ConnectionLifecycleListener() = default;

    // A basic encrypted channel has been created between this target device and
    // the remote source device. The connection has been blindly accepted by
    // this target device, but it is the responsibility of the source device to
    // make an informed choice to accept. The user of the source device makes
    // this decision by inspecting the UI of this target device, which is
    // expected to display the metadata that the IncomingConnection object
    // provides (QR Code or shapes/PIN matching).
    //
    // The IncomingConnection pointer may be cached, but will become invalid
    // after either OnConnectionAccepted(), OnConnectionRejected(), or
    // OnConnectionClosed() are called.
    //
    // Use source_device_id to understand which connection
    // OnConnectionAccepted(), OnConnectionRejected(), or OnConnectionClosed()
    // refers to.
    virtual void OnIncomingConnectionInitiated(
        const std::string& source_device_id,
        base::WeakPtr<IncomingConnection> connection) = 0;

    // Called after both sides have accepted the connection.
    //
    // This connection may be a "resumed" connection that was previously
    // "paused" before this target device performed a Critical Update and
    // rebooted.
    //
    // The AcceptedConnection pointer may be cached, but will become invalid
    // after OnConnectionClosed() is called.
    //
    // Use source_device_id to understand which connection
    // OnConnectionClosed() refers to.
    virtual void OnConnectionAccepted(
        const std::string& source_device_id,
        base::WeakPtr<AcceptedConnection> connection) = 0;

    // Called if the source device rejected the connection.
    virtual void OnConnectionRejected(const std::string& source_device_id) = 0;

    // Called when the source device is disconnected or has become unreachable.
    virtual void OnConnectionClosed(const std::string& source_device_id) = 0;
  };

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
  // |ConnectionLifecycleListener::OnIncomingConnectionInitiated()|.
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
