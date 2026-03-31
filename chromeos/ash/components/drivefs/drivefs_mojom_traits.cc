// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/drivefs/drivefs_mojom_traits.h"

#include "base/notreached.h"
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

drive::FileError
EnumTraits<drivefs::mojom::FileError, drive::FileError>::FromMojom(
    drivefs::mojom::FileError input) {
  switch (input) {
    case drivefs::mojom::FileError::kOk:
      return drive::FILE_ERROR_OK;
    case drivefs::mojom::FileError::kFailed:
      return drive::FILE_ERROR_FAILED;
    case drivefs::mojom::FileError::kInUse:
      return drive::FILE_ERROR_IN_USE;
    case drivefs::mojom::FileError::kExists:
      return drive::FILE_ERROR_EXISTS;
    case drivefs::mojom::FileError::kNotFound:
      return drive::FILE_ERROR_NOT_FOUND;
    case drivefs::mojom::FileError::kAccessDenied:
      return drive::FILE_ERROR_ACCESS_DENIED;
    case drivefs::mojom::FileError::kTooManyOpened:
      return drive::FILE_ERROR_TOO_MANY_OPENED;
    case drivefs::mojom::FileError::kNoMemory:
      return drive::FILE_ERROR_NO_MEMORY;
    case drivefs::mojom::FileError::kNoServerSpace:
      return drive::FILE_ERROR_NO_SERVER_SPACE;
    case drivefs::mojom::FileError::kNotADirectory:
      return drive::FILE_ERROR_NOT_A_DIRECTORY;
    case drivefs::mojom::FileError::kInvalidOperation:
      return drive::FILE_ERROR_INVALID_OPERATION;
    case drivefs::mojom::FileError::kSecurity:
      return drive::FILE_ERROR_SECURITY;
    case drivefs::mojom::FileError::kAbort:
      return drive::FILE_ERROR_ABORT;
    case drivefs::mojom::FileError::kNotAFile:
      return drive::FILE_ERROR_NOT_A_FILE;
    case drivefs::mojom::FileError::kNotEmpty:
      return drive::FILE_ERROR_NOT_EMPTY;
    case drivefs::mojom::FileError::kInvalidUrl:
      return drive::FILE_ERROR_INVALID_URL;
    case drivefs::mojom::FileError::kNoConnection:
      return drive::FILE_ERROR_NO_CONNECTION;
    case drivefs::mojom::FileError::kNoLocalSpace:
      return drive::FILE_ERROR_NO_LOCAL_SPACE;
    case drivefs::mojom::FileError::kServiceUnavailable:
      return drive::FILE_ERROR_SERVICE_UNAVAILABLE;
    case drivefs::mojom::FileError::kOkWithMoreResults:
      return drive::FILE_ERROR_OK_WITH_MORE_RESULTS;
  }
  NOTREACHED();
}

}  // namespace mojo
