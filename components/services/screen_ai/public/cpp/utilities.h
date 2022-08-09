// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_SCREEN_AI_PUBLIC_CPP_UTILITIES_H_
#define COMPONENTS_SERVICES_SCREEN_AI_PUBLIC_CPP_UTILITIES_H_

#include "base/files/file_path.h"

namespace screen_ai {

// Get the absolute path of the ScreenAI library.
base::FilePath GetLatestLibraryFilePath();

// Returns the install directory relative to components folder.
base::FilePath GetRelativeInstallDir();

// Returns the folder in which ScreenAI component is installed.
base::FilePath GetComponentPath();

// Stores the path to the library binary. This value is kept in memory and is
// not kept between sessions or shared between processes.
void StoreLibraryBinaryPath(const base::FilePath& path);

// Returns the library binary path if it is already stored by
// |StoreLibraryBinaryPath|.
base::FilePath GetStoredLibraryBinaryPath();

}  // namespace screen_ai
#endif  // COMPONENTS_SERVICES_SCREEN_AI_PUBLIC_CPP_UTILITIES_H_
