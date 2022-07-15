// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_ARC_FAKE_ARC_DATA_SNAPSHOTD_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_ARC_FAKE_ARC_DATA_SNAPSHOTD_CLIENT_H_

#include <string>

#include "chromeos/ash/components/dbus/arc/arc_data_snapshotd_client.h"

namespace ash {

// A fake implementation of ArcDataSnapshotdClient.
class COMPONENT_EXPORT(ASH_DBUS_ARC) FakeArcDataSnapshotdClient
    : public ArcDataSnapshotdClient {
 public:
  FakeArcDataSnapshotdClient();
  FakeArcDataSnapshotdClient(const FakeArcDataSnapshotdClient&) = delete;
  FakeArcDataSnapshotdClient& operator=(const FakeArcDataSnapshotdClient&) =
      delete;

  ~FakeArcDataSnapshotdClient() override;

  // DBusClient override:
  void Init(dbus::Bus* bus) override;

  // ArcDataSnapshotdClient override:
  void GenerateKeyPair(VoidDBusMethodCallback callback) override;

  void ClearSnapshot(bool last, VoidDBusMethodCallback callback) override;

  void TakeSnapshot(const std::string& account_id,
                    VoidDBusMethodCallback callback) override;

  void LoadSnapshot(const std::string& account_id,
                    LoadSnapshotMethodCallback callback) override;

  void Update(int percent, VoidDBusMethodCallback callback) override;

  void ConnectToUiCancelledSignal(
      base::RepeatingClosure signal_callback,
      base::OnceCallback<void(bool)> on_connected_callback) override;

  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) override;

  void set_available(bool is_available) { is_available_ = is_available; }

  base::RepeatingClosure& signal_callback() { return signal_callback_; }

 private:
  // True if the D-Bus service is available.
  bool is_available_ = false;

  base::RepeatingClosure signal_callback_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_ARC_FAKE_ARC_DATA_SNAPSHOTD_CLIENT_H_
