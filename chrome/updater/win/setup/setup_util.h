// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_SETUP_SETUP_UTIL_H_
#define CHROME_UPDATER_WIN_SETUP_SETUP_UTIL_H_

#include <guiddef.h>

#include <vector>

#include "base/strings/string16.h"
#include "base/win/windows_types.h"

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace updater {

bool RegisterWakeTask(const base::CommandLine& run_command);
void UnregisterWakeTask();

base::string16 GetComServerClsidRegistryPath(REFCLSID clsid);
base::string16 GetComServiceClsid();
base::string16 GetComServiceClsidRegistryPath();
base::string16 GetComServiceAppidRegistryPath();
base::string16 GetComIidRegistryPath(REFIID iid);
base::string16 GetComTypeLibRegistryPath(REFIID iid);

// Returns the resource index for the type library where the interface specified
// by the `iid` is defined. For encapsulation reasons, the updater interfaces
// are segregated in multiple IDL files, which get compiled to multiple type
// libraries. The type libraries are inserted in the compiled binary as
// resources with different resource indexes. The resource index becomes a
// suffix of the path to where the type library exists, such as
// `...\updater.exe\\1`. See the Windows SDK documentation for LoadTypeLib for
// details.
base::string16 GetComTypeLibResourceIndex(REFIID iid);

// Returns the interfaces ids of all interfaces declared in IDL of the updater.
std::vector<GUID> GetInterfaces();

// Parses the run time dependency file which contains all dependencies of
// the `updater` target. This file is a text file, where each line of
// text represents a single dependency. Some dependencies are not needed for
// updater to run, and are filtered out from the return value of this function.
std::vector<base::FilePath> ParseFilesFromDeps(const base::FilePath& deps);

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_SETUP_SETUP_UTIL_H_
