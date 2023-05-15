// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/drivefs/drivefs_mojom_traits.h"
#include "components/drive/file_errors.h"

namespace mojo {

drivefs::mojom::FileError
EnumTraits<drivefs::mojom::FileError, drive::FileError>::ToMojom(
    drive::FileError input) {
  switch (input) {
    case drive::FILE_ERROR_OK:
      return drivefs::mojom::FileError::kOk;
    case drive::FILE_ERROR_FAILED:
      return drivefs::mojom::FileError::kFailed;
    case drive::FILE_ERROR_IN_USE:
      return drivefs::mojom::FileError::kInUse;
    case drive::FILE_ERROR_EXISTS:
      return drivefs::mojom::FileError::kExists;
    case drive::FILE_ERROR_NOT_FOUND:
      return drivefs::mojom::FileError::kNotFound;
    case drive::FILE_ERROR_ACCESS_DENIED:
      return drivefs::mojom::FileError::kAccessDenied;
    case drive::FILE_ERROR_TOO_MANY_OPENED:
      return drivefs::mojom::FileError::kTooManyOpened;
    case drive::FILE_ERROR_NO_MEMORY:
      return drivefs::mojom::FileError::kNoMemory;
    case drive::FILE_ERROR_NO_SERVER_SPACE:
      return drivefs::mojom::FileError::kNoServerSpace;
    case drive::FILE_ERROR_NOT_A_DIRECTORY:
      return drivefs::mojom::FileError::kNotADirectory;
    case drive::FILE_ERROR_INVALID_OPERATION:
      return drivefs::mojom::FileError::kInvalidOperation;
    case drive::FILE_ERROR_SECURITY:
      return drivefs::mojom::FileError::kSecurity;
    case drive::FILE_ERROR_ABORT:
      return drivefs::mojom::FileError::kAbort;
    case drive::FILE_ERROR_NOT_A_FILE:
      return drivefs::mojom::FileError::kNotAFile;
    case drive::FILE_ERROR_NOT_EMPTY:
      return drivefs::mojom::FileError::kNotEmpty;
    case drive::FILE_ERROR_INVALID_URL:
      return drivefs::mojom::FileError::kInvalidUrl;
    case drive::FILE_ERROR_NO_CONNECTION:
      return drivefs::mojom::FileError::kNoConnection;
    case drive::FILE_ERROR_NO_LOCAL_SPACE:
      return drivefs::mojom::FileError::kNoLocalSpace;
    case drive::FILE_ERROR_SERVICE_UNAVAILABLE:
      return drivefs::mojom::FileError::kServiceUnavailable;
    case drive::FILE_ERROR_OK_WITH_MORE_RESULTS:
      return drivefs::mojom::FileError::kOkWithMoreResults;
  }
  return drivefs::mojom::FileError::kFailed;
}

bool EnumTraits<drivefs::mojom::FileError, drive::FileError>::FromMojom(
    drivefs::mojom::FileError input,
    drive::FileError* output) {
  switch (input) {
    case drivefs::mojom::FileError::kOk:
      *output = drive::FILE_ERROR_OK;
      return true;
    case drivefs::mojom::FileError::kFailed:
      *output = drive::FILE_ERROR_FAILED;
      return true;
    case drivefs::mojom::FileError::kInUse:
      *output = drive::FILE_ERROR_IN_USE;
      return true;
    case drivefs::mojom::FileError::kExists:
      *output = drive::FILE_ERROR_EXISTS;
      return true;
    case drivefs::mojom::FileError::kNotFound:
      *output = drive::FILE_ERROR_NOT_FOUND;
      return true;
    case drivefs::mojom::FileError::kAccessDenied:
      *output = drive::FILE_ERROR_ACCESS_DENIED;
      return true;
    case drivefs::mojom::FileError::kTooManyOpened:
      *output = drive::FILE_ERROR_TOO_MANY_OPENED;
      return true;
    case drivefs::mojom::FileError::kNoMemory:
      *output = drive::FILE_ERROR_NO_MEMORY;
      return true;
    case drivefs::mojom::FileError::kNoServerSpace:
      *output = drive::FILE_ERROR_NO_SERVER_SPACE;
      return true;
    case drivefs::mojom::FileError::kNotADirectory:
      *output = drive::FILE_ERROR_NOT_A_DIRECTORY;
      return true;
    case drivefs::mojom::FileError::kInvalidOperation:
      *output = drive::FILE_ERROR_INVALID_OPERATION;
      return true;
    case drivefs::mojom::FileError::kSecurity:
      *output = drive::FILE_ERROR_SECURITY;
      return true;
    case drivefs::mojom::FileError::kAbort:
      *output = drive::FILE_ERROR_ABORT;
      return true;
    case drivefs::mojom::FileError::kNotAFile:
      *output = drive::FILE_ERROR_NOT_A_FILE;
      return true;
    case drivefs::mojom::FileError::kNotEmpty:
      *output = drive::FILE_ERROR_NOT_EMPTY;
      return true;
    case drivefs::mojom::FileError::kInvalidUrl:
      *output = drive::FILE_ERROR_INVALID_URL;
      return true;
    case drivefs::mojom::FileError::kNoConnection:
      *output = drive::FILE_ERROR_NO_CONNECTION;
      return true;
    case drivefs::mojom::FileError::kNoLocalSpace:
      *output = drive::FILE_ERROR_NO_LOCAL_SPACE;
      return true;
    case drivefs::mojom::FileError::kServiceUnavailable:
      *output = drive::FILE_ERROR_SERVICE_UNAVAILABLE;
      return true;
    case drivefs::mojom::FileError::kOkWithMoreResults:
      *output = drive::FILE_ERROR_OK_WITH_MORE_RESULTS;
      return true;
  }
  return false;
}

}  // namespace mojo
