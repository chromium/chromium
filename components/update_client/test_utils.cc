// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/test_utils.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"

namespace update_client {

base::FilePath GetTestFilePath(const char* file_name) {
  base::FilePath test_data_root;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_root);
  return test_data_root.AppendASCII("components")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("update_client")
      .AppendASCII(file_name);
}

}  // namespace update_client
