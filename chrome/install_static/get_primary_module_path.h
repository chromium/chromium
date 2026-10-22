// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALL_STATIC_GET_PRIMARY_MODULE_PATH_H_
#define CHROME_INSTALL_STATIC_GET_PRIMARY_MODULE_PATH_H_

#include "base/files/file_path.h"

namespace install_static {

// Returns the path to the process's primary module, as dictated by the loader,
// or an empty path on error. No guarantees are made on the format of the path.
base::FilePath GetPrimaryModulePath();

}  // namespace install_static

#endif  // CHROME_INSTALL_STATIC_GET_PRIMARY_MODULE_PATH_H_
