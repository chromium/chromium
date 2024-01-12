// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/mock_webauthn_credentials_delegate.h"

namespace password_manager {

MockWebAuthnCredentialsDelegate::MockWebAuthnCredentialsDelegate() = default;

MockWebAuthnCredentialsDelegate::~MockWebAuthnCredentialsDelegate() = default;

base::WeakPtr<WebAuthnCredentialsDelegate>
MockWebAuthnCredentialsDelegate::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace password_manager
