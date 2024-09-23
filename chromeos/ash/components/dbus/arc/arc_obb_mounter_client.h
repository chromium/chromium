// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_ARC_ARC_OBB_MOUNTER_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_ARC_ARC_OBB_MOUNTER_CLIENT_H_

#include <stdint.h>

#include <string>

#include "base/component_export.h"
#include "chromeos/dbus/common/dbus_callback.h"
#include "chromeos/dbus/common/dbus_client.h"

namespace ash {

// ArcObbMounterClient is used to communicate with the ArcObbMounter service
// which mounts OBB (opaque binary blob) files. See:
// https://developer.android.com/google/play/expansion-files
// All method should be called from the origin thread (UI thread) which
// initializes the DBusThreadManager instance.
class COMPONENT_EXPORT(ASH_DBUS_ARC) ArcObbMounterClient
    : public chromeos::DBusClient {
 public:
  // Returns the global instance if initialized. May return null.
  static ArcObbMounterClient* Get();

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance.
  static void InitializeFake();

  // Destroys the global instance if it has been initialized.
  static void Shutdown();

  // Mounts the specified OBB at the specified mount path, with the owner GID
  // set to the given value.
  virtual void MountObb(const std::string& obb_file,
                        const std::string& mount_path,
                        int32_t owner_gid,
                        chromeos::VoidDBusMethodCallback callback) = 0;

  // Unmounts the OBB mounted at the specified path.
  virtual void UnmountObb(const std::string& mount_path,
                          chromeos::VoidDBusMethodCallback callback) = 0;

 protected:
  // Initialize() should be used instead.
  ArcObbMounterClient();
  ~ArcObbMounterClient() override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_ARC_ARC_OBB_MOUNTER_CLIENT_H_
