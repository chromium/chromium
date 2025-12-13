// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_ENCRYPTION_CORE_FEATURES_H_
#define COMPONENTS_ENTERPRISE_ENCRYPTION_CORE_FEATURES_H_

#include "base/feature_list.h"

namespace enterprise_encryption {

// Controls enabling the use of on-disk encryption for the HTTP cache.
// The `CacheEncryptionEnabled` enterprise policy must also be true for
// encryption to be active.
BASE_DECLARE_FEATURE(kEnableCacheEncryption);

}  // namespace enterprise_encryption

#endif  // COMPONENTS_ENTERPRISE_ENCRYPTION_CORE_FEATURES_H_
