// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FUSEBOX_FUSEBOX_REVERSE_CLIENT_H_
#define CHROMEOS_DBUS_FUSEBOX_FUSEBOX_REVERSE_CLIENT_H_

#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "chromeos/dbus/fusebox/fusebox.pb.h"

namespace dbus {
class Bus;
}  // namespace dbus

namespace chromeos {

// Chrome interface used to call into ChromeOS' /usr/bin/fusebox process (which
// provides the org.chromium.FuseBoxReverseService D-Bus interface). These
// calls instruct fusebox to add and remove a filesystem directory on its FUSE
// interface to the Linux filesystem, send a list of DirEntry proto to fusebox
// to resolve FUSE layer readdir(2) requests, and to notify fusebox of backend
// filesystem changes.
class COMPONENT_EXPORT(FUSEBOX_REVERSE_CLIENT) FuseBoxReverseClient {
 public:
  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static FuseBoxReverseClient* Get();

  // Storage result: |error| is a POSIX errno value.
  using StorageResult = base::OnceCallback<void(int error)>;

  // Attach fusebox storage.
  virtual void AttachStorage(const std::string& name,
                             StorageResult callback) = 0;

  // Detach fusebox storage.
  virtual void DetachStorage(const std::string& name,
                             StorageResult callback) = 0;

  // Sends a DirEntryListProto to fusebox.
  virtual void ReplyToReadDir(uint64_t handle,
                              int32_t error_code,
                              fusebox::DirEntryListProto dir_entry_list_proto,
                              bool has_more) = 0;

 protected:
  // Use Initialize/Shutdown instead.
  FuseBoxReverseClient();
  FuseBoxReverseClient(const FuseBoxReverseClient&) = delete;
  FuseBoxReverseClient& operator=(const FuseBoxReverseClient&) = delete;
  virtual ~FuseBoxReverseClient();
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when moved to ash.
namespace ash {
using ::chromeos::FuseBoxReverseClient;
}  // namespace ash

#endif  // CHROMEOS_DBUS_FUSEBOX_FUSEBOX_REVERSE_CLIENT_H_
