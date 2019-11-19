// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_PERMISSION_BROKER_PERMISSION_BROKER_CLIENT_H_
#define CHROMEOS_DBUS_PERMISSION_BROKER_PERMISSION_BROKER_CLIENT_H_

#include <stdint.h>

#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"

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
  // The ResultCallback is used for both the RequestPathAccess and
  // RequestUsbAccess methods. Its boolean parameter represents the result of
  // the operation that it was submitted alongside.
  using ResultCallback = base::OnceCallback<void(bool)>;

  // An OpenPathCallback callback is run when an OpenPath request is completed.
  using OpenPathCallback = base::OnceCallback<void(base::ScopedFD)>;

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

 protected:
  // Initialize/Shutdown should be used instead.
  PermissionBrokerClient();
  virtual ~PermissionBrokerClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(PermissionBrokerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_PERMISSION_BROKER_PERMISSION_BROKER_CLIENT_H_
