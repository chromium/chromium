// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_ARC_OBB_MOUNTER_CLIENT_H_
#define CHROMEOS_DBUS_ARC_OBB_MOUNTER_CLIENT_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"

namespace chromeos {

// ArcObbMounterClient is used to communicate with the ArcObbMounter service
// which mounts OBB (opaque binary blob - https://goo.gl/ja8aN1) files.
// All method should be called from the origin thread (UI thread) which
// initializes the DBusThreadManager instance.
class COMPONENT_EXPORT(CHROMEOS_DBUS) ArcObbMounterClient : public DBusClient {
 public:
  ArcObbMounterClient();
  ~ArcObbMounterClient() override;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static std::unique_ptr<ArcObbMounterClient> Create();

  // Mounts the specified OBB at the specified mount path, with the owner GID
  // set to the given value.
  virtual void MountObb(const std::string& obb_file,
                        const std::string& mount_path,
                        int32_t owner_gid,
                        VoidDBusMethodCallback callback) = 0;

  // Unmounts the OBB mounted at the specified path.
  virtual void UnmountObb(const std::string& mount_path,
                          VoidDBusMethodCallback callback) = 0;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_ARC_OBB_MOUNTER_CLIENT_H_
