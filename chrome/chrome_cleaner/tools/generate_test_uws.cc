// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/chrome_cleaner/pup_data/test_uws.h"

int main(int argc, char** argv) {
  base::FilePath start_menu_folder;
  CHECK(base::PathService::Get(base::DIR_START_MENU, &start_menu_folder));
  base::FilePath startup_dir = start_menu_folder.Append(L"Startup");

  base::FilePath google_test_a =
      startup_dir.Append(chrome_cleaner::kTestUwsAFilename);
  if (base::WriteFile(google_test_a, chrome_cleaner::kTestUwsAFileContents,
                      chrome_cleaner::kTestUwsAFileContentsSize) == -1) {
    PLOG(ERROR) << "Failed to create test UwS at " << google_test_a;
    return 1;
  }

  base::FilePath google_test_b =
      startup_dir.Append(chrome_cleaner::kTestUwsBFilename);
  if (base::WriteFile(google_test_b, chrome_cleaner::kTestUwsBFileContents,
                      chrome_cleaner::kTestUwsBFileContentsSize) == -1) {
    PLOG(ERROR) << "Failed to create test UwS at " << google_test_b;
    return 1;
  }

  LOG(INFO) << "Test UwS successfully generated in " << startup_dir;
  return 0;
}
