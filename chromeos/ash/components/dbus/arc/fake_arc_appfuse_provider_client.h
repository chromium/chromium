// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_ARC_FAKE_ARC_APPFUSE_PROVIDER_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_ARC_FAKE_ARC_APPFUSE_PROVIDER_CLIENT_H_

#include "chromeos/ash/components/dbus/arc/arc_appfuse_provider_client.h"

namespace ash {

// A fake implementation of ArcAppfuseProviderClient.
class COMPONENT_EXPORT(ASH_DBUS_ARC) FakeArcAppfuseProviderClient
    : public ArcAppfuseProviderClient {
 public:
  FakeArcAppfuseProviderClient();

  FakeArcAppfuseProviderClient(const FakeArcAppfuseProviderClient&) = delete;
  FakeArcAppfuseProviderClient& operator=(const FakeArcAppfuseProviderClient&) =
      delete;

  ~FakeArcAppfuseProviderClient() override;

  // chromeos::DBusClient override.
  void Init(dbus::Bus* bus) override;

  // ArcAppfuseProviderClient override:
  void Mount(uint32_t uid,
             int32_t mount_id,
             chromeos::DBusMethodCallback<base::ScopedFD> callback) override;
  void Unmount(uint32_t uid,
               int32_t mount_id,
               chromeos::VoidDBusMethodCallback callback) override;
  void OpenFile(uint32_t uid,
                int32_t mount_id,
                int32_t file_id,
                int32_t flags,
                chromeos::DBusMethodCallback<base::ScopedFD> callback) override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_ARC_FAKE_ARC_APPFUSE_PROVIDER_CLIENT_H_
