// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_VIRTUAL_FILE_PROVIDER_VIRTUAL_FILE_PROVIDER_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_VIRTUAL_FILE_PROVIDER_VIRTUAL_FILE_PROVIDER_CLIENT_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback_forward.h"
#include "chromeos/dbus/common/dbus_client.h"

namespace ash {

// VirtualFileProviderClient is used to communicate with the VirtualFileProvider
// service. The VirtualFileProvider service provides file descriptors which
// forward read requests to Chrome. From the reading process's perspective, the
// file descriptor behaves like a regular file descriptor (unlike a pipe, it
// supports seek), while actually there is no real file associated with it.
class COMPONENT_EXPORT(ASH_DBUS_VIRTUAL_FILE_PROVIDER) VirtualFileProviderClient
    : public chromeos::DBusClient {
 public:
  using GenerateVirtualFileIdCallback =
      base::OnceCallback<void(const std::optional<std::string>& id)>;
  using OpenFileByIdCallback = base::OnceCallback<void(base::ScopedFD fd)>;

  // Returns the global instance if initialized. May return null.
  static VirtualFileProviderClient* Get();

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance used on Linux desktop, if
  // no instance already exists.
  static void InitializeFake();

  // Destroys the global instance if it has been initialized.
  static void Shutdown();

  // Generates and returns a unique ID, to be used by OpenFileById() for FD
  // creation. |size| will be used to perform boundary check when FD is seeked.
  virtual void GenerateVirtualFileId(
      int64_t size,
      GenerateVirtualFileIdCallback callback) = 0;

  // Given a unique ID, creates a new file descriptor. When the FD is read,
  // the read request is forwarded to the request handler.
  virtual void OpenFileById(const std::string& id,
                            OpenFileByIdCallback callback) = 0;

 protected:
  VirtualFileProviderClient();
  ~VirtualFileProviderClient() override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_VIRTUAL_FILE_PROVIDER_VIRTUAL_FILE_PROVIDER_CLIENT_H_
