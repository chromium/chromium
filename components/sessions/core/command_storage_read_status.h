// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CORE_COMMAND_STORAGE_READ_STATUS_H_
#define COMPONENTS_SESSIONS_CORE_COMMAND_STORAGE_READ_STATUS_H_

namespace sessions {

// Statuses that can occur when reading a file using
// CommandStorageBackend::ReadLastSessionCommands().
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(CommandStorageReadStatus)
enum class CommandStorageReadStatus {
  kUnknown = 0,
  kSuccess = 1,  // The file was read successfully.
  kNoFile = 2,   // No file exists for the last session (not an error).
  kFileInvalid = 3,
  kFileEmpty = 4,
  kInvalidHeader = 5,
  kInvalidCommand = 6,
  kUnsupportedVersion = 7,
  kDecryptionUnavailable = 8,  // OSCrypt lacked permission to decrypt.
  kMaxValue = kDecryptionUnavailable,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/session/enums.xml:CommandStorageReadStatus)

// Returns true if `status` describes a read failure. kNoFile is not an error:
// it means no session was ever written.
constexpr bool IsCommandStorageReadError(CommandStorageReadStatus status) {
  switch (status) {
    case CommandStorageReadStatus::kSuccess:
    case CommandStorageReadStatus::kNoFile:
      return false;
    case CommandStorageReadStatus::kUnknown:
    case CommandStorageReadStatus::kFileInvalid:
    case CommandStorageReadStatus::kFileEmpty:
    case CommandStorageReadStatus::kInvalidHeader:
    case CommandStorageReadStatus::kInvalidCommand:
    case CommandStorageReadStatus::kUnsupportedVersion:
    case CommandStorageReadStatus::kDecryptionUnavailable:
      return true;
  }
}

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CORE_COMMAND_STORAGE_READ_STATUS_H_
