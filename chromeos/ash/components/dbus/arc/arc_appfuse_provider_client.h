// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_ARC_ARC_APPFUSE_PROVIDER_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_ARC_ARC_APPFUSE_PROVIDER_CLIENT_H_

#include <stdint.h>

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "chromeos/dbus/common/dbus_callback.h"
#include "chromeos/dbus/common/dbus_client.h"

namespace ash {

// ArcAppfuseProviderClient is used to communicate with the ArcAppfuseProvider
// service which provides ProxyFileDescriptor (aka appfuse) feature for ARC. All
// methods should be called from the origin thread (UI thread) which initializes
// the DBusThreadManager instance.
class COMPONENT_EXPORT(ASH_DBUS_ARC) ArcAppfuseProviderClient
    : public chromeos::DBusClient {
 public:
  // Returns the global instance if initialized. May return null.
  static ArcAppfuseProviderClient* Get();

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance.
  static void InitializeFake();

  // Destroys the global instance if it has been initialized.
  static void Shutdown();

  // Mounts a new appfuse file system and returns a filtered /dev/fuse FD
  // associated with the mounted file system.
  virtual void Mount(uint32_t uid,
                     int32_t mount_id,
                     chromeos::DBusMethodCallback<base::ScopedFD> callback) = 0;

  // Unmounts the specified appfuse file system.
  virtual void Unmount(uint32_t uid,
                       int32_t mount_id,
                       chromeos::VoidDBusMethodCallback callback) = 0;

  // Opens a file under the specified appfuse file system.
  virtual void OpenFile(
      uint32_t uid,
      int32_t mount_id,
      int32_t file_id,
      int32_t flags,
      chromeos::DBusMethodCallback<base::ScopedFD> callback) = 0;

 protected:
  ArcAppfuseProviderClient();
  ~ArcAppfuseProviderClient() override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_ARC_ARC_APPFUSE_PROVIDER_CLIENT_H_
