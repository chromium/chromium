// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/paths.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"

namespace viz {

bool PathProvider(int key, base::FilePath* result) {
  base::FilePath cur;
  switch (key) {
    // The following are only valid in the development environment, and
    // will fail if executed from an installed executable (because the
    // generated path won't exist).
    case Paths::DIR_TEST_DATA:
      if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &cur)) {
        return false;
      }
      cur = cur.Append(FILE_PATH_LITERAL("components"));
      cur = cur.Append(FILE_PATH_LITERAL("test"));
      cur = cur.Append(FILE_PATH_LITERAL("data"));
      cur = cur.Append(FILE_PATH_LITERAL("viz"));
      if (!base::PathExists(cur))  // we don't want to create this
        return false;
      break;
    default:
      return false;
  }

  *result = cur;
  return true;
}

// This cannot be done as a static initializer sadly since Visual Studio will
// eliminate this object file if there is no direct entry point into it.
void Paths::RegisterPathProvider() {
  base::PathService::RegisterProvider(PathProvider, PATH_START, PATH_END);
}

}  // namespace viz
