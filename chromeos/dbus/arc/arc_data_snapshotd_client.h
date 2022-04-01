// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_ARC_ARC_DATA_SNAPSHOTD_CLIENT_H_
#define CHROMEOS_DBUS_ARC_ARC_DATA_SNAPSHOTD_CLIENT_H_

#include <string>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "chromeos/dbus/common/dbus_client.h"
#include "chromeos/dbus/common/dbus_method_call_status.h"
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

  using LoadSnapshotMethodCallback = base::OnceCallback<void(bool, bool)>;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static std::unique_ptr<ArcDataSnapshotdClient> Create();

  // Generate key pair for an upcoming session.
  // Should be called before the user session started.
  virtual void GenerateKeyPair(VoidDBusMethodCallback callback) = 0;

  // Remove a snapshot. If |last|, remove the last generated snapshot,
  // otherwise the previous one.
  virtual void ClearSnapshot(bool last, VoidDBusMethodCallback callback) = 0;

  // Take the ARC data/ snapshot of the current session.
  // MGS is a current active session with |account_id|.
  virtual void TakeSnapshot(const std::string& account_id,
                            VoidDBusMethodCallback callback) = 0;

  // Load the ARC data/ snapshot to the current active MGS with |account_id|.
  virtual void LoadSnapshot(const std::string& account_id,
                            LoadSnapshotMethodCallback callback) = 0;

  // Update a progress bar on a UI screen.
  // |percent| is a percentage of installed required ARC apps [0..100].
  virtual void Update(int percent, VoidDBusMethodCallback callback) = 0;

  // Connects callbacks to D-Bus signal |UiCancelled| sent by
  // arc-data-snapshotd.
  virtual void ConnectToUiCancelledSignal(
      base::RepeatingClosure signal_callback,
      base::OnceCallback<void(bool)> on_connected_callback) = 0;

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
