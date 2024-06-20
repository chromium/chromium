// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_UNEXPORTABLE_KEY_UTILS_H_
#define CHROME_BROWSER_WEBAUTHN_UNEXPORTABLE_KEY_UTILS_H_

#include <memory>

namespace crypto {
class UnexportableKeyProvider;
}  // namespace crypto

std::unique_ptr<crypto::UnexportableKeyProvider>
GetWebAuthnUnexportableKeyProvider();

#endif  // CHROME_BROWSER_WEBAUTHN_UNEXPORTABLE_KEY_UTILS_H_
