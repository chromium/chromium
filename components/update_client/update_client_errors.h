// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_UPDATE_CLIENT_ERRORS_H_
#define COMPONENTS_UPDATE_CLIENT_UPDATE_CLIENT_ERRORS_H_

namespace update_client {

// Errors generated as a result of calling UpdateClient member functions.
// These errors are not sent in pings.
enum class Error {
  NONE = 0,
  UPDATE_IN_PROGRESS = 1,
  UPDATE_CANCELED = 2,
  RETRY_LATER = 3,
  SERVICE_ERROR = 4,
  UPDATE_CHECK_ERROR = 5,
  CRX_NOT_FOUND = 6,
  INVALID_ARGUMENT = 7,
  BAD_CRX_DATA_CALLBACK = 8,
  MAX_VALUE,
};

// These errors are sent in pings. Add new values only to the bottom of
// the enums below; the order must be kept stable.
enum class ErrorCategory {
  kNone = 0,
  kDownload = 1,
  kUnpack = 2,
  kInstall = 3,
  kService = 4,  // Runtime errors which occur in the service itself.
  kUpdateCheck = 5,
  // kUnknown = 6, defined in `updater_service.mojom`.
  kInstaller = 7,
};

// These errors are returned with the `kDownload` error category. This category
// could include other errors such as the errors defined by the Chrome net
// stack.
enum class CrxDownloaderError {
  NONE = 0,
  NO_URL = 10,
  NO_HASH = 11,
  BAD_HASH = 12,  // The downloaded file fails the hash verification.
  DISK_FULL = 13,
  CANCELLED = 14,
  NO_DOWNLOAD_DIR = 15,

  // The Windows BITS queue contains to many update client jobs. The value is
  // chosen so that it can be reported as a custom COM error on this platform.
  BITS_TOO_MANY_JOBS = 0x0200,
  // Errors 11XX are reserved for Mac background downloader errors.
  MAC_BG_CANNOT_CREATE_DOWNLOAD_CACHE = 1101,
  MAC_BG_MOVE_TO_CACHE_FAIL = 1102,
  MAC_BG_MISSING_COMPLETION_DATA = 1103,
  MAC_BG_DUPLICATE_DOWNLOAD = 1104,
  MAC_BG_SESSION_INVALIDATED = 1105,
  MAC_BG_SESSION_TOO_MANY_TASKS = 1106,
  GENERIC_ERROR = -1
};

// These errors are returned with the |kUnpack| error category and
// indicate unpacker or patcher error.
enum class UnpackerError {
  kNone = 0,
  kInvalidParams = 1,
  kInvalidFile = 2,
  kUnzipPathError = 3,
  kUnzipFailed = 4,
  // kNoManifest = 5,         // Deprecated. Never used.
  kBadManifest = 6,
  kBadExtension = 7,
  // kInvalidId = 8,          // Deprecated. Combined with kInvalidFile.
  // kInstallerError = 9,     // Deprecated. Don't use.
  kIoError = 10,
  kDeltaVerificationFailure = 11,
  kDeltaBadCommands = 12,
  kDeltaUnsupportedCommand = 13,
  kDeltaOperationFailure = 14,
  kDeltaPatchProcessFailure = 15,
  kDeltaMissingExistingFile = 16,
  // kFingerprintWriteFailed = 17,    // Deprecated. Don't use.
  kPuffinMissingPreviousCrx = 18,
  kFailedToAddToCache = 19,
  kFailedToCreateCacheDir = 20,
  kCrxCacheNotProvided = 21,
};

// These errors are returned with the |kInstall| error category and
// are returned by the component installers.
enum class InstallError {
  NONE = 0,
  FINGERPRINT_WRITE_FAILED = 2,
  BAD_MANIFEST = 3,
  GENERIC_ERROR = 9,  // Matches kInstallerError for compatibility.
  MOVE_FILES_ERROR = 10,
  SET_PERMISSIONS_FAILED = 11,
  INVALID_VERSION = 12,
  VERSION_NOT_UPGRADED = 13,
  NO_DIR_COMPONENT_USER = 14,
  CLEAN_INSTALL_DIR_FAILED = 15,
  INSTALL_VERIFICATION_FAILED = 16,
  MISSING_INSTALL_PARAMS = 17,
  // If LaunchProcess is attempted on unsupported non-desktop skus e.g. xbox
  LAUNCH_PROCESS_FAILED = 18,
  CUSTOM_ERROR_BASE = 100,  // Specific installer errors go above this value.
};

// These errors are returned with the |kService| error category and
// indicate critical or configuration errors in the update service.
enum class ServiceError {
  NONE = 0,
  SERVICE_WAIT_FAILED = 1,
  UPDATE_DISABLED = 2,
  CANCELLED = 3,

  // Returned when a `CheckForUpdate` call is made, the server returns a
  // update response indicating an update is available, and updates are enabled.
  CHECK_FOR_UPDATE_ONLY = 4,
};

// These errors are related to serialization, deserialization, and parsing of
// protocol requests.
// The begin value for this enum is chosen not to conflict with network errors
// defined by net/base/net_error_list.h. The callers don't have to handle this
// error in any meaningful way, but this value may be reported in UMA stats,
// therefore avoiding collisions with known network errors is desirable.
enum class ProtocolError : int {
  NONE = 0,
  RESPONSE_NOT_TRUSTED = -10000,
  MISSING_PUBLIC_KEY = -10001,
  MISSING_URLS = -10002,
  PARSE_FAILED = -10003,
  UPDATE_RESPONSE_NOT_FOUND = -10004,
  URL_FETCHER_FAILED = -10005,
  UNKNOWN_APPLICATION = -10006,
  RESTRICTED_APPLICATION = -10007,
  INVALID_APPID = -10008,
  OS_NOT_SUPPORTED = -10009,
  HW_NOT_SUPPORTED = -10010,
  NO_HASH = -10011,
  UNSUPPORTED_PROTOCOL = -10012,
  INTERNAL = -10013,
};

struct CategorizedError {
  ErrorCategory category_ = ErrorCategory::kNone;
  int code_ = 0;
  int extra_ = 0;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_UPDATE_CLIENT_ERRORS_H_
