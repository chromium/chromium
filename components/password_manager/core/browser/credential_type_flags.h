// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_TYPE_FLAGS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_TYPE_FLAGS_H_

namespace password_manager {

// Keep this class in sync with the CredentialTypeFlags enum in
// third_party/blink/public/mojom/credentialmanagement/credential_type_flags.mojom.
enum class CredentialTypeFlags {
  kNone = 0,

  kPassword = 1,   // 1 << 0
  kFederated = 2,  // 1 << 1
  kPublicKey = 4,  // 1 << 2

  kAll = -1,
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_TYPE_FLAGS_H_
