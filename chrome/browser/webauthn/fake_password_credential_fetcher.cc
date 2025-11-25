// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/fake_password_credential_fetcher.h"

#include <utility>

FakePasswordCredentialFetcher::FakePasswordCredentialFetcher(
    content::RenderFrameHost* rfh)
    : PasswordCredentialFetcher(rfh) {}

FakePasswordCredentialFetcher::~FakePasswordCredentialFetcher() = default;

void FakePasswordCredentialFetcher::FetchPasswords(
    const GURL& url,
    PasswordCredentialsReceivedCallback callback) {
  fetch_passwords_called_ = true;
  callback_ = std::move(callback);
  if (call_callback_immediately_) {
    InvokeCallback();
  }
}

void FakePasswordCredentialFetcher::SetPasswords(
    PasswordCredentials passwords) {
  passwords_ = std::move(passwords);
}

void FakePasswordCredentialFetcher::SetCallCallbackImmediately(
    bool call_immediately) {
  call_callback_immediately_ = call_immediately;
}

void FakePasswordCredentialFetcher::UpdateDateLastUsed(
    const std::u16string& username,
    const std::u16string& password) {
  if (update_date_last_used_called_ptr_) {
    *update_date_last_used_called_ptr_ = true;
  }
}

void FakePasswordCredentialFetcher::InvokeCallback() {
  if (callback_) {
    std::move(callback_).Run(std::move(passwords_));
  }
}
