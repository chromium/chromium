// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ENTERPRISE_COMPANION_CONSTANTS_H_
#define CHROME_ENTERPRISE_COMPANION_CONSTANTS_H_

namespace enterprise_companion {

// Errors defined by the enterprise companion app. Ordinals are transmitted
// across IPC and network boundaries. Entries must not be removed or reordered.
enum class ApplicationError {
  // An action failed due to the client not being registered.
  kRegistrationPreconditionFailed,
  // DMStorage reports that it is not capable of persisting policies.
  kPolicyPersistenceImpossible,
  // DMStorage failed to persist the policies.
  kPolicyPersistenceFailed,
  // The global singleton lock could not be acquired.
  kCannotAcquireLock,
  // An IPC connection could not be established.
  kMojoConnectionFailed,
  // Installation or uninstallation failed.
  kInstallationFailed,
  // The IPC caller is not allowed to perform the requested action.
  kIpcCallerNotAllowed,
  // Failed to initialize COM on Windows.
  kCOMInitializationFailed,
  // The CloudPolicyClient timed out.
  kCloudPolicyClientTimeout,
  // The enrollment token is malformed.
  kInvalidEnrollmentToken,
};

inline constexpr int kStatusOk = 0;
inline constexpr int kStatusApplicationError = 3;

}  // namespace enterprise_companion

#endif  // CHROME_ENTERPRISE_COMPANION_CONSTANTS_H_
