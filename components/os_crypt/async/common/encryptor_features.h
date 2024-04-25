// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_ASYNC_COMMON_ENCRYPTOR_FEATURES_H_
#define COMPONENTS_OS_CRYPT_ASYNC_COMMON_ENCRYPTOR_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "build/build_config.h"

namespace os_crypt_async::features {

#if BUILDFLAG(IS_WIN)
// Whether or not to encrypt the Encryptor key in memory using
// CryptProtectMemory.
COMPONENT_EXPORT(OS_CRYPT_ASYNC) BASE_DECLARE_FEATURE(kProtectEncryptionKey);
#endif  // BUILDFLAG(IS_WIN)

}  // namespace os_crypt_async::features

#endif  // COMPONENTS_OS_CRYPT_ASYNC_COMMON_ENCRYPTOR_FEATURES_H_
