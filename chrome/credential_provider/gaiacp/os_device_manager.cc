// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/os_device_manager.h"

#include <windows.h>

#include <hidsdi.h>

#include <memory>

#include "base/scoped_generic.h"
#include "services/device/public/mojom/hid.mojom.h"

namespace credential_provider {

namespace {

struct PreparsedDataScopedTraits {
  static PHIDP_PREPARSED_DATA InvalidValue() { return nullptr; }
  static void Free(PHIDP_PREPARSED_DATA h) { HidD_FreePreparsedData(h); }
};

using ScopedPreparsedData =
    base::ScopedGeneric<PHIDP_PREPARSED_DATA, PreparsedDataScopedTraits>;

}  // namespace

// static
OSDeviceManager** OSDeviceManager::GetInstanceStorage() {
  static OSDeviceManager* instance = new OSDeviceManager();
  return &instance;
}

// static
OSDeviceManager* OSDeviceManager::Get() {
  return *GetInstanceStorage();
}

// static
void OSDeviceManager::SetInstanceForTesting(OSDeviceManager* instance) {
  *GetInstanceStorage() = instance;
}

OSDeviceManager::~OSDeviceManager() = default;

base::win::ScopedHandle OSDeviceManager::OpenDevice(
    const std::wstring& device_path) {
  // LINT.IfChange
  base::win::ScopedHandle file;
  constexpr DWORD kDesiredAccessModes[] = {
      // Request read and write access.
      GENERIC_WRITE | GENERIC_READ,
      // Request read-only access.
      GENERIC_READ,
      // Don't request read or write access.
      0,
  };
  for (const auto& desired_access : kDesiredAccessModes) {
    file.Set(CreateFile(device_path.c_str(), desired_access,
                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                        /*lpSecurityAttributes=*/nullptr, OPEN_EXISTING,
                        FILE_FLAG_OVERLAPPED, /*hTemplateFile=*/nullptr));
    if (file.is_valid() || GetLastError() != ERROR_ACCESS_DENIED) {
      break;
    }
  }
  return file;
  // LINT.ThenChange(//services/device/hid/hid_service_win.cc)
}

uint16_t OSDeviceManager::GetUsagePage(HANDLE device_handle) {
  if (device_handle == INVALID_HANDLE_VALUE) {
    return 0;
  }

  ScopedPreparsedData preparsed_data;
  if (!HidD_GetPreparsedData(
          device_handle, ScopedPreparsedData::Receiver(preparsed_data).get())) {
    return 0;
  }

  HIDP_CAPS caps;
  if (HidP_GetCaps(preparsed_data.get(), &caps) != HIDP_STATUS_SUCCESS) {
    return 0;
  }

  return caps.UsagePage;
}

}  // namespace credential_provider
