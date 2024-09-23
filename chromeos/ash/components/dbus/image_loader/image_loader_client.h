// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_IMAGE_LOADER_IMAGE_LOADER_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_IMAGE_LOADER_IMAGE_LOADER_CLIENT_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "chromeos/dbus/common/dbus_callback.h"
#include "chromeos/dbus/common/dbus_client.h"

namespace ash {

// ImageLoaderClient is used to communicate with the ImageLoader service, which
// registers and loads component updates on Chrome OS.
class COMPONENT_EXPORT(ASH_DBUS_IMAGE_LOADER) ImageLoaderClient
    : public chromeos::DBusClient {
 public:
  // Returns the global instance if initialized. May return null.
  static ImageLoaderClient* Get();

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance.
  static void InitializeFake();

  // Destroys the global instance if it has been initialized.
  static void Shutdown();

  ImageLoaderClient(const ImageLoaderClient&) = delete;
  ImageLoaderClient& operator=(const ImageLoaderClient&) = delete;

  // Registers a component by copying from |component_folder_abs_path| into its
  // internal storage, if and only if, the component passes verification.
  virtual void RegisterComponent(
      const std::string& name,
      const std::string& version,
      const std::string& component_folder_abs_path,
      chromeos::DBusMethodCallback<bool> callback) = 0;

  // Mounts a component given the |name| and return the mount point (if call is
  // successful).
  virtual void LoadComponent(
      const std::string& name,
      chromeos::DBusMethodCallback<std::string> callback) = 0;

  // Mounts a component given the |name| and install path |path|, then returns
  // the mount point (if call is successful).
  virtual void LoadComponentAtPath(
      const std::string& name,
      const base::FilePath& path,
      chromeos::DBusMethodCallback<base::FilePath> callback) = 0;

  // Requests the currently registered version of the given component |name|.
  virtual void RequestComponentVersion(
      const std::string& name,
      chromeos::DBusMethodCallback<std::string> callback) = 0;

  // Removes a component and returns true (if call is successful).
  virtual void RemoveComponent(const std::string& name,
                               chromeos::DBusMethodCallback<bool> callback) = 0;

  // Unmounts all mount points given component |name|.
  virtual void UnmountComponent(
      const std::string& name,
      chromeos::DBusMethodCallback<bool> callback) = 0;

 protected:
  // Initialize() should be used instead.
  ImageLoaderClient();
  ~ImageLoaderClient() override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_IMAGE_LOADER_IMAGE_LOADER_CLIENT_H_
