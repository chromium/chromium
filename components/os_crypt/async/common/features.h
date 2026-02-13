// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_ASYNC_COMMON_FEATURES_H_
#define COMPONENTS_OS_CRYPT_ASYNC_COMMON_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace os_crypt_async {

// Prevents falling back to OSCrypt Sync if OSCrypt Async is unable to encrypt
// or decrypt data.
COMPONENT_EXPORT(OS_CRYPT_ASYNC)
BASE_DECLARE_FEATURE(kOSCryptAsyncPreventFallbackToSync);

}  // namespace os_crypt_async

#endif  // COMPONENTS_OS_CRYPT_ASYNC_COMMON_FEATURES_H_
