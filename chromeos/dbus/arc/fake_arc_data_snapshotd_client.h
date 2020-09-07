// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_ARC_FAKE_ARC_DATA_SNAPSHOTD_CLIENT_H_
#define CHROMEOS_DBUS_ARC_FAKE_ARC_DATA_SNAPSHOTD_CLIENT_H_

#include "chromeos/dbus/arc/arc_data_snapshotd_client.h"

namespace chromeos {

// A fake implementation of ArcDataSnapshotdClient.
class COMPONENT_EXPORT(CHROMEOS_DBUS_ARC) FakeArcDataSnapshotdClient
    : public ArcDataSnapshotdClient {
 public:
  FakeArcDataSnapshotdClient() = default;
  FakeArcDataSnapshotdClient(const FakeArcDataSnapshotdClient&) = delete;
  FakeArcDataSnapshotdClient& operator=(const FakeArcDataSnapshotdClient&) =
      delete;

  ~FakeArcDataSnapshotdClient() override = default;

  // DBusClient override:
  void Init(dbus::Bus* bus) override;

  // ArcDataSnapshotdClient override:
  void GenerateKeyPair(VoidDBusMethodCallback callback) override;

  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) override;

  void set_available(bool is_available) { is_available_ = is_available; }

 private:
  // True if the D-Bus service is available.
  bool is_available_ = false;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_ARC_FAKE_ARC_DATA_SNAPSHOTD_CLIENT_H_
