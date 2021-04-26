// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_COMMON_WIN_INSTALL_UTILS_H_
#define CLOUD_PRINT_COMMON_WIN_INSTALL_UTILS_H_

#include <wtypes.h>
#include <string>

#include "base/files/file_path.h"

namespace cloud_print {

// Sets Google Update registry keys after install or update.
void SetGoogleUpdateKeys(const std::wstring& product_id,
                         const std::wstring& product_name);

// Sets custom error message to show by Google Update installer
void SetGoogleUpdateError(const std::wstring& product_id,
                          const std::wstring& message);

// Sets custom system error code to show by Google Update installer
void SetGoogleUpdateError(const std::wstring& product_id, HRESULT hr);

// Deletes Google Update reg keys on product uninstall.
void DeleteGoogleUpdateKeys(const std::wstring& product_id);

// Creates control panel uninstall item.
void CreateUninstallKey(const std::wstring& uninstall_id,
                        const std::wstring& product_name,
                        const std::string& uninstall_switch);

// Deletes control panel uninstall item.
void DeleteUninstallKey(const std::wstring& uninstall_id);

// Returns install location retrieved from control panel uninstall key.
base::FilePath GetInstallLocation(const std::wstring& uninstall_id);

// Returns install location retrieved from control panel uninstall key.
void DeleteProgramDir(const std::string& delete_switch);

// Returns true if path is part of program files.
bool IsProgramsFilesParent(const base::FilePath& path);

}  // namespace cloud_print

#endif  // CLOUD_PRINT_COMMON_WIN_INSTALL_UTILS_H_
