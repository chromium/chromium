// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_SCREEN_AI_PUBLIC_CPP_UTILITIES_H_
#define COMPONENTS_SERVICES_SCREEN_AI_PUBLIC_CPP_UTILITIES_H_

#include "base/files/file_path.h"

namespace screen_ai {

// Get the absolute path of the ScreenAI library.
const base::FilePath GetLibraryFilePath();

// Returns the install directory relative to components folder.
const base::FilePath GetRelativeInstallDir();

}  // namespace screen_ai
#endif  // COMPONENTS_SERVICES_SCREEN_AI_PUBLIC_CPP_UTILITIES_H_
