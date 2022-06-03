// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_ARC_ARC_APPFUSE_PROVIDER_CLIENT_H_
#define CHROMEOS_DBUS_ARC_ARC_APPFUSE_PROVIDER_CLIENT_H_

#include <stdint.h>

#include <memory>

#include "base/component_export.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"

#include "base/files/scoped_file.h"

namespace chromeos {

// ArcAppfuseProviderClient is used to communicate with the ArcAppfuseProvider
// service which provides ProxyFileDescriptor (aka appfuse) feature for ARC. All
// methods should be called from the origin thread (UI thread) which initializes
// the DBusThreadManager instance.
class COMPONENT_EXPORT(CHROMEOS_DBUS_ARC) ArcAppfuseProviderClient
    : public DBusClient {
 public:
  ArcAppfuseProviderClient();
  ~ArcAppfuseProviderClient() override;

  // Factory function, creates a new instance.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static std::unique_ptr<ArcAppfuseProviderClient> Create();

  // Mounts a new appfuse file system and returns a filtered /dev/fuse FD
  // associated with the mounted file system.
  virtual void Mount(uint32_t uid,
                     int32_t mount_id,
                     DBusMethodCallback<base::ScopedFD> callback) = 0;

  // Unmounts the specified appfuse file system.
  virtual void Unmount(uint32_t uid,
                       int32_t mount_id,
                       VoidDBusMethodCallback callback) = 0;

  // Opens a file under the specified appfuse file system.
  virtual void OpenFile(uint32_t uid,
                        int32_t mount_id,
                        int32_t file_id,
                        int32_t flags,
                        DBusMethodCallback<base::ScopedFD> callback) = 0;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_ARC_ARC_APPFUSE_PROVIDER_CLIENT_H_
