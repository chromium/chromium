// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_SYNC_OS_CRYPT_METRICS_H_
#define COMPONENTS_OS_CRYPT_SYNC_OS_CRYPT_METRICS_H_

namespace os_crypt {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class EncryptionPrefixVersion {
  // Data did not have any version header and so was returned to the caller
  // without being decrypted.
  kNoVersion = 0,
  // Data starts with `v10`. This is supported on all platforms.
  kVersion10 = 1,
  // Data starts with `v11`. This is only supported on Linux.
  kVersion11 = 2,
  kMaxValue = kVersion11,
};

void LogEncryptionVersion(EncryptionPrefixVersion version);

}  // namespace os_crypt

#endif  // COMPONENTS_OS_CRYPT_SYNC_OS_CRYPT_METRICS_H_
