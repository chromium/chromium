// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_MINI_INSTALLER_EXIT_CODE_H_
#define CHROME_INSTALLER_MINI_INSTALLER_EXIT_CODE_H_

namespace mini_installer {

// mini_installer process exit codes (the underlying type is uint32_t).
enum ExitCode {
  SUCCESS_EXIT_CODE = 0,
  GENERIC_ERROR = 1,
  // The next three generic values are here for historic reasons. New additions
  // should have values strictly greater than them. This is to prevent
  // collisions with setup.exe's installer::InstallStatus enum since the two are
  // surfaced similarly by Google Update.
  GENERIC_INITIALIZATION_FAILURE = 101,
  // DEPRECATED_UNPACKING_FAILURE = 102,
  // DEPRECATED_SETUP_FAILURE = 103,
  // NO_PREVIOUS_SETUP_PATH = 104,
  COMMAND_STRING_OVERFLOW = 105,
  // COULD_NOT_CREATE_PROCESS = 106,
  WAIT_FOR_PROCESS_FAILED = 107,
  PATH_STRING_OVERFLOW = 108,
  UNABLE_TO_GET_WORK_DIRECTORY = 109,
  UNABLE_TO_FIND_REGISTRY_KEY = 110,
  PATCH_NOT_FOR_INSTALLED_VERSION = 111,
  UNABLE_TO_EXTRACT_CHROME_ARCHIVE = 112,
  // UNABLE_TO_EXTRACT_SETUP_BL = 113,
  // UNABLE_TO_EXTRACT_SETUP_BN = 114,
  UNABLE_TO_EXTRACT_SETUP_EXE = 115,
  UNABLE_TO_EXTRACT_SETUP = 116,
  UNABLE_TO_SET_DIRECTORY_ACL = 117,
  INVALID_OPTION = 118,
  // These next codes specifically mean that the attempt to launch the previous
  // setup.exe to patch it failed.
  SETUP_PATCH_FAILED_FILE_NOT_FOUND = 119,            // ERROR_FILE_NOT_FOUND.
  SETUP_PATCH_FAILED_PATH_NOT_FOUND = 120,            // ERROR_PATH_NOT_FOUND.
  SETUP_PATCH_FAILED_COULD_NOT_CREATE_PROCESS = 121,  // All other errors.
  // As above, but for running the new setup.exe to handle an install.
  RUN_SETUP_FAILED_FILE_NOT_FOUND = 122,            // ERROR_FILE_NOT_FOUND.
  RUN_SETUP_FAILED_PATH_NOT_FOUND = 123,            // ERROR_PATH_NOT_FOUND.
  RUN_SETUP_FAILED_COULD_NOT_CREATE_PROCESS = 124,  // All other errors.
};

}  // namespace mini_installer

#endif  // CHROME_INSTALLER_MINI_INSTALLER_EXIT_CODE_H_
