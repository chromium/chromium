// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_VIRTUAL_FILE_PROVIDER_CLIENT_H_
#define CHROMEOS_DBUS_VIRTUAL_FILE_PROVIDER_CLIENT_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "chromeos/dbus/dbus_client.h"

namespace chromeos {

// VirtualFileProviderClient is used to communicate with the VirtualFileProvider
// service. The VirtualFileProvider service provides file descriptors which
// forward read requests to Chrome. From the reading process's perspective, the
// file descriptor behaves like a regular file descriptor (unlike a pipe, it
// supports seek), while actually there is no real file associated with it.
class COMPONENT_EXPORT(CHROMEOS_DBUS) VirtualFileProviderClient
    : public DBusClient {
 public:
  using OpenFileCallback =
      base::OnceCallback<void(const std::string& id, base::ScopedFD fd)>;

  VirtualFileProviderClient();
  ~VirtualFileProviderClient() override;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static std::unique_ptr<VirtualFileProviderClient> Create();

  // Creates a new file descriptor and returns it with a unique ID.
  // |size| will be used to perform boundary check when FD is seeked.
  // When the FD is read, the read request is forwarded to the request handler.
  virtual void OpenFile(int64_t size, OpenFileCallback callback) = 0;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_VIRTUAL_FILE_PROVIDER_CLIENT_H_
