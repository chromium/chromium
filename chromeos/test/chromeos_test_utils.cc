// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/test/chromeos_test_utils.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"

namespace chromeos {
namespace test_utils {

bool GetTestDataPath(const std::string& component,
                     const std::string& filename,
                     base::FilePath* data_dir) {
  base::FilePath path;
  if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &path)) {
    return false;
  }
  path = path.Append(FILE_PATH_LITERAL("chromeos"));
  path = path.Append(FILE_PATH_LITERAL("test"));
  path = path.Append(FILE_PATH_LITERAL("data"));
  {
    base::ScopedAllowBlockingForTesting allow_io;
    if (!base::PathExists(path))  // We don't want to create this.
      return false;
  }
  DCHECK(data_dir);
  path = path.Append(component);
  *data_dir = path.Append(filename);
  return true;
}

}  // namespace test_utils
}  // namespace chromeos
