// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_IMAGE_LOADER_CLIENT_H_
#define CHROMEOS_DBUS_IMAGE_LOADER_CLIENT_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"

namespace chromeos {

// ImageLoaderClient is used to communicate with the ImageLoader service, which
// registers and loads component updates on Chrome OS.
class COMPONENT_EXPORT(CHROMEOS_DBUS) ImageLoaderClient : public DBusClient {
 public:
  ~ImageLoaderClient() override;

  // Registers a component by copying from |component_folder_abs_path| into its
  // internal storage, if and only if, the component passes verification.
  virtual void RegisterComponent(const std::string& name,
                                 const std::string& version,
                                 const std::string& component_folder_abs_path,
                                 DBusMethodCallback<bool> callback) = 0;

  // Mounts a component given the |name| and return the mount point (if call is
  // successful).
  virtual void LoadComponent(const std::string& name,
                             DBusMethodCallback<std::string> callback) = 0;

  // Mounts a component given the |name| and install path |path|, then returns
  // the mount point (if call is successful).
  virtual void LoadComponentAtPath(
      const std::string& name,
      const base::FilePath& path,
      DBusMethodCallback<base::FilePath> callback) = 0;

  // Requests the currently registered version of the given component |name|.
  virtual void RequestComponentVersion(
      const std::string& name,
      DBusMethodCallback<std::string> callback) = 0;

  // Removes a component and returns true (if call is successful).
  virtual void RemoveComponent(const std::string& name,
                               DBusMethodCallback<bool> callback) = 0;

  // Unmounts all mount points given component |name|.
  virtual void UnmountComponent(const std::string& name,
                                DBusMethodCallback<bool> callback) = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static std::unique_ptr<ImageLoaderClient> Create();

 protected:
  // Create() should be used instead.
  ImageLoaderClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(ImageLoaderClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_IMAGE_LOADER_CLIENT_H_
