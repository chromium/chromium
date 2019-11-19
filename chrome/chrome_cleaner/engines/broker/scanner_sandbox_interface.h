// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_BROKER_SCANNER_SANDBOX_INTERFACE_H_
#define CHROME_CHROME_CLEANER_ENGINES_BROKER_SCANNER_SANDBOX_INTERFACE_H_

#include <shlobj.h>
#include <stdint.h>
#include <windows.h>

#include <set>
#include <vector>

#include "base/files/file_path.h"
#include "base/process/process_handle.h"
#include "base/strings/string16.h"
#include "base/win/scoped_handle.h"
#include "chrome/chrome_cleaner/mojom/engine_requests.mojom.h"
#include "chrome/chrome_cleaner/os/task_scheduler.h"
#include "chrome/chrome_cleaner/strings/string16_embedded_nulls.h"

namespace chrome_cleaner_sandbox {

uint32_t SandboxFindFirstFile(const base::FilePath& file_name,
                              LPWIN32_FIND_DATAW lpFindFileData,
                              HANDLE* handle);

// This function can't just be called FindNextFile or the compiler thinks it is
// overloading the Windows version and gets unhappy.
uint32_t SandboxFindNextFile(HANDLE hFindFile,
                             LPWIN32_FIND_DATAW lpFindFileData);

// This function can't just be called FindClose or the compiler thinks it is
// overloading the Windows version and gets unhappy.
uint32_t SandboxFindClose(HANDLE hFindFile);

uint32_t SandboxGetFileAttributes(const base::FilePath& file_name,
                                  uint32_t* attributes);

bool SandboxGetKnownFolderPath(chrome_cleaner::mojom::KnownFolder folder_id,
                               base::FilePath* folder_path);

bool SandboxGetProcesses(std::vector<base::ProcessId>* processes);

bool SandboxGetTasks(
    std::vector<chrome_cleaner::TaskScheduler::TaskInfo>* task_list);

bool SandboxGetProcessImagePath(base::ProcessId pid,
                                base::FilePath* image_path);

bool SandboxGetLoadedModules(base::ProcessId pid,
                             std::set<base::string16>* module_names);

bool SandboxGetProcessCommandLine(base::ProcessId pid,
                                  base::string16* process_cmd);

bool SandboxGetUserInfoFromSID(
    const SID* const sid,
    chrome_cleaner::mojom::UserInformation* user_info);

base::win::ScopedHandle SandboxOpenReadOnlyFile(const base::FilePath& file_name,
                                                uint32_t dwFlagsAndAttributes);

uint32_t SandboxOpenReadOnlyRegistry(HANDLE root_key,
                                     const base::string16& sub_key,
                                     uint32_t dw_access,
                                     HKEY* registry_handle);

uint32_t SandboxNtOpenReadOnlyRegistry(
    HANDLE root_key,
    const chrome_cleaner::String16EmbeddedNulls& sub_key,
    uint32_t dw_access,
    HANDLE* registry_handle);

}  // namespace chrome_cleaner_sandbox

#endif  // CHROME_CHROME_CLEANER_ENGINES_BROKER_SCANNER_SANDBOX_INTERFACE_H_
