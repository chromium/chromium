// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/passkey_credential.h"

namespace password_manager {

PasskeyCredential::PasskeyCredential(const Username& username,
                                     const BackendId& backend_id)
    : username_(username), backend_id_(backend_id) {}

PasskeyCredential::~PasskeyCredential() = default;

PasskeyCredential::PasskeyCredential(const PasskeyCredential&) = default;
PasskeyCredential& PasskeyCredential::operator=(const PasskeyCredential&) =
    default;

PasskeyCredential::PasskeyCredential(PasskeyCredential&&) = default;
PasskeyCredential& PasskeyCredential::operator=(PasskeyCredential&&) = default;

bool operator==(const PasskeyCredential& lhs, const PasskeyCredential& rhs) {
  return lhs.username() == rhs.username() && lhs.id() == rhs.id();
}

}  // namespace password_manager
