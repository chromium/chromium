// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_SETUP_CONFIGURE_APP_CONTAINER_SANDBOX_H_
#define CHROME_INSTALLER_SETUP_CONFIGURE_APP_CONTAINER_SANDBOX_H_

#include <optional>
#include <vector>

#include "base/containers/span.h"

namespace base {
class FilePath;
namespace win {
class Sid;
}  // namespace win
}  // namespace base

namespace installer {

// Returns the SIDs of the capabilities through which processes in Chrome's
// AppContainer sandboxes are granted access to Chrome's installation, or
// std::nullopt on error. A directory must grant these capabilities read and
// execute access for such processes to load modules from it.
std::optional<std::vector<base::win::Sid>> GetInstallFilesCapabilitySids();

// Adds AppContainer ACEs to paths in support of the AppContainer sandbox.
bool ConfigureAppContainerSandbox(
    base::span<const base::FilePath* const> paths);

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_CONFIGURE_APP_CONTAINER_SANDBOX_H_
