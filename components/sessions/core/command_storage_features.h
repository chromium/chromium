// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CORE_COMMAND_STORAGE_FEATURES_H_
#define COMPONENTS_SESSIONS_CORE_COMMAND_STORAGE_FEATURES_H_

#include "base/metrics/field_trial_params.h"
#include "components/sessions/core/sessions_export.h"

namespace sessions {

// Encrypt session storage on most platforms.
SESSIONS_EXPORT BASE_DECLARE_FEATURE(kEncryptSessionStorage);

// A string corresponding to one of the EncryptSessionStorageStage enum values.
// See the conversion in |GetEncryptSessionStorageStage|.
extern const base::FeatureParam<std::string> kEncryptSessionStorageStageParam;

namespace internal {

// String values for the `kEncryptSessionStorageStageParam` field trial
// parameter. These are implementation details and should not be used outside of
// command_storage_features.cc and its unit tests.
inline constexpr char kEncryptSessionStorageStageWriteBothReadOnlyClear[] =
    "write_both_read_only_clear";
inline constexpr char
    kEncryptSessionStorageStageWriteBothReadPreferEncrypted[] =
        "write_both_read_prefer_encrypted";
inline constexpr char
    kEncryptSessionStorageStageWriteEncryptedReadPreferEncrypted[] =
        "write_encrypted_read_prefer_encrypted";

}  // namespace internal

// These stages track the process of transitioning from cleartext storage of
// session data to encrypted storage.  See crbug.com/479420496.
enum class EncryptSessionStorageStage {
  kClearOnly,  // Session storage is cleartext, not encrypted.
               // This occurs when the kEncryptSessionStorage feature is
               // disabled, or the value of kEncryptSessionStorageStageParam
               // is empty or unrecognized.
  kWriteBothReadOnlyClear,  // Write both cleartext and encrypted versions of
                            // the session storage, but only read from the
                            // cleartext version. This is helpful for testing
                            // the performance and stability of encryption.
  kWriteBothReadPreferEncrypted,  // Write both cleartext and encrypted versions
                                  // of the session storage, and prefer to read
                                  // from the encrypted version.  Keeping the
                                  // cleartext version allows fallback if needed
                                  // (e.g., a bug in reading or writing the
                                  // encrypted version).
  kWriteEncryptedReadPreferEncrypted,  // Write only the encrypted version of
                                       // the session storage, and prefer to
                                       // read from the encrypted version.
                                       // Older files are deleted (see
                                       // DeleteLastSessionFiles). The ability
                                       // to read cleartext is maintained for
                                       // backwards compatibility.
};

EncryptSessionStorageStage GetEncryptSessionStorageStage();

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CORE_COMMAND_STORAGE_FEATURES_H_
