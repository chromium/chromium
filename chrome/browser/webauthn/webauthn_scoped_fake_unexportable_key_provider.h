// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_WEBAUTHN_SCOPED_FAKE_UNEXPORTABLE_KEY_PROVIDER_H_
#define CHROME_BROWSER_WEBAUTHN_WEBAUTHN_SCOPED_FAKE_UNEXPORTABLE_KEY_PROVIDER_H_

// `WebAuthnScopedFakeUnexportableKeyProvider` causes
// `GetWebAuthnUnexportableKeyProvider` to return a fake, software-based
// implementation of `UnexportableKeyProvider` while it is in scope.
class WebAuthnScopedFakeUnexportableKeyProvider {
 public:
  WebAuthnScopedFakeUnexportableKeyProvider();
  WebAuthnScopedFakeUnexportableKeyProvider(
      const WebAuthnScopedFakeUnexportableKeyProvider&) = delete;
  WebAuthnScopedFakeUnexportableKeyProvider(
      WebAuthnScopedFakeUnexportableKeyProvider&&) = delete;
  ~WebAuthnScopedFakeUnexportableKeyProvider();
};

// `WebAuthnScopedNullUnexportableKeyProvider` causes
// `GetWebAuthnUnexportableKeyProvider` to return a nullptr, emulating the key
// provider not being supported.
class WebAuthnScopedNullUnexportableKeyProvider {
 public:
  WebAuthnScopedNullUnexportableKeyProvider();
  WebAuthnScopedNullUnexportableKeyProvider(
      const WebAuthnScopedNullUnexportableKeyProvider&) = delete;
  WebAuthnScopedNullUnexportableKeyProvider(
      WebAuthnScopedNullUnexportableKeyProvider&&) = delete;
  ~WebAuthnScopedNullUnexportableKeyProvider();
};

#endif  // CHROME_BROWSER_WEBAUTHN_WEBAUTHN_SCOPED_FAKE_UNEXPORTABLE_KEY_PROVIDER_H_
