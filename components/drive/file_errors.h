// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DRIVE_FILE_ERRORS_H_
#define COMPONENTS_DRIVE_FILE_ERRORS_H_

#include "base/files/file.h"
#include "google_apis/common/api_error_codes.h"

namespace drive {

// These values are persisted to UMA histograms (see UMA enum DriveFileError).
// The histogram values are positive (1=OK, 2=FAILED, 3=IN_USE, etc).
// Entries should not be renumbered and numeric values should never be reused.
//
// Consider using `IsFileErrorOk` to check for successful operations, as it
// includes both `FILE_ERROR_OK` and `FILE_ERROR_OK_WITH_MORE_RESULTS`.
enum FileError {
  FILE_ERROR_OK = 0,
  FILE_ERROR_FAILED = -1,
  FILE_ERROR_IN_USE = -2,
  FILE_ERROR_EXISTS = -3,
  FILE_ERROR_NOT_FOUND = -4,
  FILE_ERROR_ACCESS_DENIED = -5,
  FILE_ERROR_TOO_MANY_OPENED = -6,
  FILE_ERROR_NO_MEMORY = -7,
  FILE_ERROR_NO_SERVER_SPACE = -8,
  FILE_ERROR_NOT_A_DIRECTORY = -9,
  FILE_ERROR_INVALID_OPERATION = -10,
  FILE_ERROR_SECURITY = -11,
  FILE_ERROR_ABORT = -12,
  FILE_ERROR_NOT_A_FILE = -13,
  FILE_ERROR_NOT_EMPTY = -14,
  FILE_ERROR_INVALID_URL = -15,
  FILE_ERROR_NO_CONNECTION = -16,
  FILE_ERROR_NO_LOCAL_SPACE = -17,
  FILE_ERROR_SERVICE_UNAVAILABLE = -18,
  FILE_ERROR_OK_WITH_MORE_RESULTS = -19,
  // Put new entries here and adjust FILE_ERROR_MAX.
  FILE_ERROR_MAX = FILE_ERROR_OK_WITH_MORE_RESULTS,
};

std::ostream& operator<<(std::ostream& out, FileError error);

// Returns whether a `FileError` represents a successful operation.
bool IsFileErrorOk(FileError error);

// Returns a string representation of FileError.
std::string FileErrorToString(FileError error);

// Returns a base::File::Error that corresponds to the FileError provided.
base::File::Error FileErrorToBaseFileError(FileError error);

// Converts GData error code into Drive file error code.
FileError GDataToFileError(google_apis::ApiErrorCode status);

}  // namespace drive

#endif  // COMPONENTS_DRIVE_FILE_ERRORS_H_
