// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_EXTENSION_EXTENSION_UTILS_H_
#define CHROME_CREDENTIAL_PROVIDER_EXTENSION_EXTENSION_UTILS_H_

#include <windows.h>

#include "base/files/file.h"

namespace credential_provider {

namespace extension {

// Updates the provided |service_status| parameter with the SERVICE_STATUS for
// GCPW extension. If retrieving SERVICE_STATUS fails, returns an error code
// other than ERROR_SUCCESS.
DWORD GetGCPWExtensionServiceStatus(SERVICE_STATUS* service_status);

// Returns true if GCPW extension is running.
bool IsGCPWExtensionRunning();

// Installs GCPW Extension service. If there is an already GCPW extension, it is
// stopped and deleted initially.
DWORD InstallGCPWExtension(const base::FilePath& extension_exe_path);

// Uninstalls GCPW Extension service by stopping and deleting the service.
DWORD UninstallGCPWExtension();

// Returns true if installation of GCPW extension is enabled.
bool IsGCPWExtensionEnabled();

// Returns the registry name from the provided |task_name| which is provided
// when registering the task with GCPW extension.
std::wstring GetLastSyncRegNameForTask(const std::wstring& task_name);

}  // namespace extension
}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_EXTENSION_EXTENSION_UTILS_H_
