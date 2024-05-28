// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_SETUP_CONFIGURE_APP_CONTAINER_SANDBOX_H_
#define CHROME_INSTALLER_SETUP_CONFIGURE_APP_CONTAINER_SANDBOX_H_

#include "base/containers/span.h"

namespace base {
class FilePath;
}  // namespace base

namespace installer {

// Adds AppContainer ACEs to paths in support of the AppContainer sandbox.
bool ConfigureAppContainerSandbox(base::span<const base::FilePath*> paths);

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_CONFIGURE_APP_CONTAINER_SANDBOX_H_
