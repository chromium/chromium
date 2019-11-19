// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/error_codes.h"

#include <errno.h>
#include <fcntl.h>

#include <string>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chromecast/base/path_utils.h"

namespace chromecast {

namespace {

const char kInitialErrorFile[] = "initial_error";

base::FilePath GetInitialErrorFilePath() {
  return GetHomePathASCII(kInitialErrorFile);
}

}  // namespace

ErrorCode GetInitialErrorCode() {
  std::string initial_error_code_str;
  if (!base::ReadFileToString(GetInitialErrorFilePath(),
                              &initial_error_code_str)) {
    return NO_ERROR;
  }

  int initial_error_code = 0;
  if (base::StringToInt(initial_error_code_str, &initial_error_code) &&
      initial_error_code >= NO_ERROR && initial_error_code <= ERROR_UNKNOWN) {
    DVLOG(1) << "Initial error from " << GetInitialErrorFilePath().value()
             << ": " << initial_error_code;
    return static_cast<ErrorCode>(initial_error_code);
  }

  LOG(ERROR) << "Unknown initial error code: " << initial_error_code_str;
  return NO_ERROR;
}

bool SetInitialErrorCode(ErrorCode initial_error_code) {
  // Note: Do not use Chromium IO methods in this function. When cast_shell
  // crashes, this function can be called by any thread.
  const std::string error_file_path = GetInitialErrorFilePath().value();

  if (initial_error_code > NO_ERROR && initial_error_code <= ERROR_UNKNOWN) {
    const std::string initial_error_code_str(
        base::NumberToString(initial_error_code));
    int fd = creat(error_file_path.c_str(), 0640);
    if (fd < 0) {
      PLOG(ERROR) << "Could not open error code file";
      return false;
    }

    int written =
        write(fd, initial_error_code_str.data(), initial_error_code_str.size());

    if (written != static_cast<int>(initial_error_code_str.size())) {
      PLOG(ERROR) << "Could not write error code to file: written=" << written
                  << ", expected=" << initial_error_code_str.size();
      close(fd);
      return false;
    }

    close(fd);
    return true;
  }

  // Remove initial error file if no error.
  if (unlink(error_file_path.c_str()) == 0 || errno == ENOENT)
    return true;

  PLOG(ERROR) << "Failed to remove error file";
  return false;
}

}  // namespace chromecast
