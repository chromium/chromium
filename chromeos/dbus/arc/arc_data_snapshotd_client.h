// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_ARC_ARC_DATA_SNAPSHOTD_CLIENT_H_
#define CHROMEOS_DBUS_ARC_ARC_DATA_SNAPSHOTD_CLIENT_H_

#include "base/component_export.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "dbus/object_proxy.h"

namespace chromeos {

// ArcDataSnapshotdClient is used to delegate ARC data/ snapshot related tasks
// to arc-data-snapshotd daemon in Chrome OS.
class COMPONENT_EXPORT(CHROMEOS_DBUS_ARC) ArcDataSnapshotdClient
    : public DBusClient {
 public:
  ~ArcDataSnapshotdClient() override;

  ArcDataSnapshotdClient(const ArcDataSnapshotdClient&) = delete;
  ArcDataSnapshotdClient& operator=(const ArcDataSnapshotdClient&) = delete;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static std::unique_ptr<ArcDataSnapshotdClient> Create();

  // Generate key pair for an upcoming session.
  // Should be called before the user session started.
  virtual void GenerateKeyPair(VoidDBusMethodCallback callback) = 0;

  // Registers |callback| to run when the arc-data-snapshotd becomes available.
  // If the service is already available, or if connecting to the name-owner-
  // changed signal fails, |callback| will be run once asynchronously.
  // Otherwise, |callback| will be run once in the future after the service
  // becomes available.
  virtual void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) = 0;

 protected:
  // Create() should be used instead.
  ArcDataSnapshotdClient();
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_ARC_ARC_DATA_SNAPSHOTD_CLIENT_H_
