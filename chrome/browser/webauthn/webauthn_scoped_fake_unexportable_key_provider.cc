// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/webauthn_scoped_fake_unexportable_key_provider.h"

#include <vector>

#include "chrome/browser/webauthn/unexportable_key_utils.h"
#include "crypto/unexportable_key.h"

// This file contains test overrides for unexportable keys used in the context
// of WebAuthn. These should be preferred to using the global //crypto
// overrides on browser tests, as other parts of Chrome may access those while
// your test runs and cause undesired behaviour.

namespace {

std::unique_ptr<crypto::UnexportableKeyProvider>
GetWebAuthnUnexportableKeyProviderFake() {
  return crypto::GetSoftwareUnsecureUnexportableKeyProvider();
}

std::unique_ptr<crypto::UnexportableKeyProvider>
GetWebAuthnUnexportableKeyProviderNull() {
  return nullptr;
}

}  // namespace

WebAuthnScopedFakeUnexportableKeyProvider::
    WebAuthnScopedFakeUnexportableKeyProvider() {
  SetWebAuthnUnexportableKeyProviderForTesting(
      GetWebAuthnUnexportableKeyProviderFake);
}

WebAuthnScopedFakeUnexportableKeyProvider::
    ~WebAuthnScopedFakeUnexportableKeyProvider() {
  SetWebAuthnUnexportableKeyProviderForTesting(nullptr);
}

WebAuthnScopedNullUnexportableKeyProvider::
    WebAuthnScopedNullUnexportableKeyProvider() {
  SetWebAuthnUnexportableKeyProviderForTesting(
      GetWebAuthnUnexportableKeyProviderNull);
}

WebAuthnScopedNullUnexportableKeyProvider::
    ~WebAuthnScopedNullUnexportableKeyProvider() {
  SetWebAuthnUnexportableKeyProviderForTesting(nullptr);
}
