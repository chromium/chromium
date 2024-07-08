// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/common/encryptor_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace os_crypt_async::features {

#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kProtectEncryptionKey,
             "ProtectEncryptionKey",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)

}  // namespace os_crypt_async::features
