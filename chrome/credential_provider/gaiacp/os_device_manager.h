// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_OS_DEVICE_MANAGER_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_OS_DEVICE_MANAGER_H_

#include <string>

#include "base/win/scoped_handle.h"

namespace credential_provider {

// This class is used to open OS devices.
class [[clang::lto_visibility_public]] OSDeviceManager {
 public:
  static OSDeviceManager* Get();

  virtual ~OSDeviceManager();

  // Opens a device handle for the given device path.
  virtual base::win::ScopedHandle OpenDevice(const std::wstring& device_path);

  // Gets the usage page for the given device handle.
  virtual uint16_t GetUsagePage(HANDLE device_handle);

  static void SetInstanceForTesting(OSDeviceManager* instance);

 protected:
  OSDeviceManager() = default;

  static OSDeviceManager** GetInstanceStorage();
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_OS_DEVICE_MANAGER_H_
