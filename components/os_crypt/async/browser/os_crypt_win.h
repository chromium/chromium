// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_ASYNC_BROWSER_OS_CRYPT_WIN_H_
#define COMPONENTS_OS_CRYPT_ASYNC_BROWSER_OS_CRYPT_WIN_H_

#include "base/component_export.h"

class PrefRegistrySimple;
class PrefService;

namespace os_crypt_async {

// Initializes the Windows v10 key for OSCrypt. This should be called early in
// browser startup before any OSCrypt operations are performed.
// Returns true if initialization was successful.
COMPONENT_EXPORT(OS_CRYPT_ASYNC) bool Init(PrefService* local_state);

enum class InitResult {
  kSuccess,
  kKeyDoesNotExist,
  kInvalidKeyFormat,
  kDecryptionFailed,
};

// Initializes the Windows v10 key for OSCrypt using an existing key from
// local state. Does not generate a new key if one does not exist.
COMPONENT_EXPORT(OS_CRYPT_ASYNC)
InitResult InitWithExistingKey(PrefService* local_state);

// Registers the local state preferences used by the Windows v10 key.
COMPONENT_EXPORT(OS_CRYPT_ASYNC)
void RegisterLocalPrefs(PrefRegistrySimple* registry);

}  // namespace os_crypt_async

#endif  // COMPONENTS_OS_CRYPT_ASYNC_BROWSER_OS_CRYPT_WIN_H_
