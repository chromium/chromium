// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/common/features.h"

#include "base/feature_list.h"

namespace os_crypt_async {

BASE_FEATURE(kOSCryptAsyncPreventFallbackToSync,
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace os_crypt_async
