// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_ARC_FAKE_ARC_APPFUSE_PROVIDER_CLIENT_H_
#define CHROMEOS_DBUS_ARC_FAKE_ARC_APPFUSE_PROVIDER_CLIENT_H_

#include "chromeos/dbus/arc/arc_appfuse_provider_client.h"

namespace chromeos {

// A fake implementation of ArcAppfuseProviderClient.
class COMPONENT_EXPORT(CHROMEOS_DBUS_ARC) FakeArcAppfuseProviderClient
    : public ArcAppfuseProviderClient {
 public:
  FakeArcAppfuseProviderClient();

  FakeArcAppfuseProviderClient(const FakeArcAppfuseProviderClient&) = delete;
  FakeArcAppfuseProviderClient& operator=(const FakeArcAppfuseProviderClient&) =
      delete;

  ~FakeArcAppfuseProviderClient() override;

  // DBusClient override.
  void Init(dbus::Bus* bus) override;

  // ArcAppfuseProviderClient override:
  void Mount(uint32_t uid,
             int32_t mount_id,
             DBusMethodCallback<base::ScopedFD> callback) override;
  void Unmount(uint32_t uid,
               int32_t mount_id,
               VoidDBusMethodCallback callback) override;
  void OpenFile(uint32_t uid,
                int32_t mount_id,
                int32_t file_id,
                int32_t flags,
                DBusMethodCallback<base::ScopedFD> callback) override;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_ARC_FAKE_ARC_APPFUSE_PROVIDER_CLIENT_H_
