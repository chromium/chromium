// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_PERMISSION_BROKER_PERMISSION_BROKER_CLIENT_H_
#define CHROMEOS_DBUS_PERMISSION_BROKER_PERMISSION_BROKER_CLIENT_H_

#include <stdint.h>

#include <string>

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback.h"

namespace dbus {
class Bus;
}

namespace chromeos {

// PermissionBrokerClient is used to communicate with the permission broker, a
// process that allows requesting permission to access specific device nodes.
// For example, one place that this client is used is within the USB extension
// API code, where it is used to request explicit access to USB peripherals
// which the user the browser runs under normally wouldn't have access to. For
// more details on the permission broker see:
// http://git.chromium.org/gitweb/?p=chromiumos/platform/permission_broker.git
class COMPONENT_EXPORT(PERMISSION_BROKER) PermissionBrokerClient {
 public:
  // The ResultCallback's boolean parameter represents the result of the
  // operation that it was submitted alongside.
  using ResultCallback = base::OnceCallback<void(bool)>;

  // An OpenPathCallback callback is run when an OpenPath request is completed.
  using OpenPathCallback = base::OnceCallback<void(base::ScopedFD)>;

  // An OpenPathAndRegisterClientCallback callback is run when an
  // OpenPathAndRegisterClient request is completed.
  using OpenPathAndRegisterClientCallback =
      base::OnceCallback<void(const std::string& client_id, base::ScopedFD)>;

  // An ErrorCallback callback is run when an error is returned by the
  // permission broker.
  using ErrorCallback = base::OnceCallback<void(const std::string& error_name,
                                                const std::string& message)>;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance which must have been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static PermissionBrokerClient* Get();

  PermissionBrokerClient(const PermissionBrokerClient&) = delete;
  PermissionBrokerClient& operator=(const PermissionBrokerClient&) = delete;

  // CheckPathAccess requests a hint from the permission broker about whether
  // a later call to RequestPathAccess will be successful. It presumes that
  // the |interface_id| value passed to RequestPathAccess will be
  // UsbDevicePermissionsData::ANY_INTERFACE).
  virtual void CheckPathAccess(const std::string& path,
                               ResultCallback callback) = 0;

  // OpenPath requests that the permission broker open the device node
  // identified by |path| and return the resulting file descriptor. One of
  // |callback| or |error_callback| is called.
  virtual void OpenPath(const std::string& path,
                        OpenPathCallback callback,
                        ErrorCallback error_callback) = 0;

  // ClaimDevicePath requests that the permission broker open
  // the device node identified by |path| and set of USB interfaces that can be
  // claimed |allowed_interfaces_mask|, returning the resulting file descriptor.
  // The interface number 0 corresponds to the LSB of |allowed_interfaces_mask|.
  // For example, a device which has an ADB interface and other interfaces for
  // Camera or Storage may be opened purely as an ADB device using a mask that
  // zeros out the Camera and Storage interface number bit positions.
  // One of |callback| or |error_callback| is called. |lifeline_fd| is the
  // read side of a pipe that is is watched by permission broker. When this
  // pipe closes, any kernel drivers removed from the device are reattached.
  virtual void ClaimDevicePath(const std::string& path,
                               uint32_t allowed_interfaces_mask,
                               int lifeline_fd,
                               OpenPathCallback callback,
                               ErrorCallback error_callback) = 0;

  // This API is for a client to register with the Permission Broker to
  // make requests to detach/reattach USB device interfaces in the future.
  // The |drop_privileges_mask| is a bit mask indicating which interface
  // numbers of a USB device are allowed. The interface number 0 corresponds
  // to the LSB of the mask. A device which has an ADB interface and other
  // interfaces for Camera or Storage may be opened purely as an ADB device
  // using a mask that zeros out the Camera and Storage interface number
  // bit positions.
  // The |path| is the USB device path the client wants to access.
  // The |lifeline_fd| is a file descriptor for monitoring the client's
  // lifetime and reattaching detached interfaces when the client terminates.
  // The method returns |fd| which is a file descriptor opened at |path|,
  // and |client_id| which is an unique id for a registered client.
  virtual void OpenPathAndRegisterClient(
      const std::string& path,
      uint32_t allowed_interfaces_mask,
      int lifeline_fd,
      OpenPathAndRegisterClientCallback callback,
      ErrorCallback error_callback) = 0;

  // This API is for the client with |client_id| to detach the interface
  // |iface_num| at the USB device associated with it.
  virtual void DetachInterface(const std::string& client_id,
                               uint8_t iface_num,
                               ResultCallback callback) = 0;

  // This API is for the client with |client_id| to reattach the interface
  // |iface_num| at the USB device associated with it.
  virtual void ReattachInterface(const std::string& client_id,
                                 uint8_t iface_num,
                                 ResultCallback callback) = 0;

  // Requests the |port| be opened on the firewall for incoming TCP/IP
  // connections received on |interface| (an empty string indicates all
  // interfaces). One end of an open pipe must be passed as |lifeline_fd| so
  // that the permission broker can monitor the lifetime of the calling process
  // by being notified when the other end is closed. This method duplicates
  // |lifeline_fd| so it's OK to close it without waiting for the result.
  virtual void RequestTcpPortAccess(uint16_t port,
                                    const std::string& interface,
                                    int lifeline_fd,
                                    ResultCallback callback) = 0;

  // Requests the |port| be opened on the firewall for incoming UDP packets
  // received on |interface| (an empty string indicates all interfaces). One end
  // of an open pipe must be passed as |lifeline_fd| so that the permission
  // broker can monitor the lifetime of the calling process by being notified
  // when the other end is closed. This method duplicates |lifeline_fd| so it's
  // OK to close it without waiting for the result.
  virtual void RequestUdpPortAccess(uint16_t port,
                                    const std::string& interface,
                                    int lifeline_fd,
                                    ResultCallback callback) = 0;

  // Releases a request for an open firewall port for TCP/IP connections. The
  // |port| and |interface| parameters must be the same as a previous call to
  // RequestTcpPortAccess.
  virtual void ReleaseTcpPort(uint16_t port,
                              const std::string& interface,
                              ResultCallback callback) = 0;

  // Releases a request for an open firewall port for UDP packets. The |port|
  // and |interface| parameters must be the same as a previous call to
  // RequestUdpPortAccess.
  virtual void ReleaseUdpPort(uint16_t port,
                              const std::string& interface,
                              ResultCallback callback) = 0;

  // Requests that |in_port| on |in_interface| be opened for and forward TCP
  // traffic to |dst_ip| on |dst_port|. One end of an open pipe must be passed
  // as |lifeline_fd| so that the permission broker can monitor the lifetime of
  // the calling process by being notified when the other end is closed. This
  // method duplicates |lifeline_fd| so it's OK to close it without waiting for
  // the result.
  // See PortTracker::ValidatePortRule in permission_broker for the restrictions
  // on port forwarding requests.
  virtual void RequestTcpPortForward(uint16_t in_port,
                                     const std::string& in_interface,
                                     const std::string& dst_ip,
                                     uint16_t dst_port,
                                     int lifeline_fd,
                                     ResultCallback callback) = 0;

  // Requests that |in_port| on |in_interface| be opened for and forward UDP
  // traffic to |dst_ip| on |dst_port|. One end of an open pipe must be passed
  // as |lifeline_fd| so that the permission broker can monitor the lifetime of
  // the calling process by being notified when the other end is closed. This
  // method duplicates |lifeline_fd| so it's OK to close it without waiting for
  // the result.
  // See PortTracker::ValidatePortRule in permission_broker for the restrictions
  // on port forwarding requests.
  virtual void RequestUdpPortForward(uint16_t in_port,
                                     const std::string& in_interface,
                                     const std::string& dst_ip,
                                     uint16_t dst_port,
                                     int lifeline_fd,
                                     ResultCallback callback) = 0;

  // Releases a request for an open forwarding rule for TCP packets. The
  // |in_port| and |in_interface| parameters must be the same as a previous call
  // to RequestUdpPortForward.
  virtual void ReleaseTcpPortForward(uint16_t in_port,
                                     const std::string& in_interface,
                                     ResultCallback callback) = 0;

  // Releases a request for an open forwarding rule for UDP packets. The
  // |in_port| and |in_interface| parameters must be the same as a previous call
  // to RequestUdpPortForward.
  virtual void ReleaseUdpPortForward(uint16_t in_port,
                                     const std::string& in_interface,
                                     ResultCallback callback) = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  PermissionBrokerClient();
  virtual ~PermissionBrokerClient();
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_PERMISSION_BROKER_PERMISSION_BROKER_CLIENT_H_
