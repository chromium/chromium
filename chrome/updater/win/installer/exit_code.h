// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_INSTALLER_EXIT_CODE_H_
#define CHROME_UPDATER_WIN_INSTALLER_EXIT_CODE_H_

namespace updater {

// Installer process exit codes (the underlying type is uint32_t).
enum ExitCode {
  SUCCESS_EXIT_CODE = 0,
  GENERIC_INITIALIZATION_FAILURE = 101,
  COMMAND_STRING_OVERFLOW = 105,
  WAIT_FOR_PROCESS_FAILED = 107,
  PATH_STRING_OVERFLOW = 108,
  UNABLE_TO_GET_WORK_DIRECTORY = 109,
  UNABLE_TO_EXTRACT_ARCHIVE = 112,
  UNABLE_TO_SET_DIRECTORY_ACL = 117,
  INVALID_OPTION = 118,
  RUN_SETUP_FAILED_FILE_NOT_FOUND = 122,            // ERROR_FILE_NOT_FOUND.
  RUN_SETUP_FAILED_PATH_NOT_FOUND = 123,            // ERROR_PATH_NOT_FOUND.
  RUN_SETUP_FAILED_COULD_NOT_CREATE_PROCESS = 124,  // All other errors.
};

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_INSTALLER_EXIT_CODE_H_
