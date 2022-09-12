// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_ARC_FAKE_ARC_OBB_MOUNTER_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_ARC_FAKE_ARC_OBB_MOUNTER_CLIENT_H_

#include <string>

#include "chromeos/ash/components/dbus/arc/arc_obb_mounter_client.h"

namespace ash {

// A fake implementation of ArcObbMounterClient.
class COMPONENT_EXPORT(ASH_DBUS_ARC) FakeArcObbMounterClient
    : public ArcObbMounterClient {
 public:
  FakeArcObbMounterClient();

  FakeArcObbMounterClient(const FakeArcObbMounterClient&) = delete;
  FakeArcObbMounterClient& operator=(const FakeArcObbMounterClient&) = delete;

  ~FakeArcObbMounterClient() override;

  // chromeos::DBusClient override.
  void Init(dbus::Bus* bus) override;

  // ArcObbMounterClient override:
  void MountObb(const std::string& obb_file,
                const std::string& mount_path,
                int32_t owner_gid,
                chromeos::VoidDBusMethodCallback callback) override;
  void UnmountObb(const std::string& mount_path,
                  chromeos::VoidDBusMethodCallback callback) override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_ARC_FAKE_ARC_OBB_MOUNTER_CLIENT_H_
