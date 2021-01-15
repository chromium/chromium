// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/utils.h"

#include <utility>

#include "base/check_op.h"
#include "base/files/file_util.h"
#include "base/path_service.h"

namespace chromeos {
namespace assistant {

// Get the root path for assistant files.
base::FilePath GetRootPath() {
  base::FilePath home_dir;
  CHECK(base::PathService::Get(base::DIR_HOME, &home_dir));
  // Ensures DIR_HOME is overridden after primary user sign-in.
  CHECK_NE(base::GetHomeDir(), home_dir);
  return home_dir;
}

base::FilePath GetBaseAssistantDir() {
  return GetRootPath().Append(FILE_PATH_LITERAL("google-assistant-library"));
}

}  // namespace assistant
}  // namespace chromeos
