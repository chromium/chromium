// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_FAKE_PASSWORD_CREDENTIAL_FETCHER_H_
#define CHROME_BROWSER_WEBAUTHN_FAKE_PASSWORD_CREDENTIAL_FETCHER_H_

#include "base/functional/callback.h"
#include "chrome/browser/webauthn/password_credential_fetcher.h"

namespace content {
class RenderFrameHost;
}

class FakePasswordCredentialFetcher : public PasswordCredentialFetcher {
 public:
  explicit FakePasswordCredentialFetcher(content::RenderFrameHost* rfh);
  ~FakePasswordCredentialFetcher() override;

  void FetchPasswords(const GURL& url,
                      PasswordCredentialsReceivedCallback callback) override;
  bool fetch_passwords_called() const { return fetch_passwords_called_; }

  void SetPasswords(PasswordCredentials passwords);

  void SetCallCallbackImmediately(bool call_immediately);
  void InvokeCallback();

 private:
  PasswordCredentials passwords_;
  PasswordCredentialsReceivedCallback callback_;
  bool fetch_passwords_called_ = false;
  bool call_callback_immediately_ = false;
};

#endif  // CHROME_BROWSER_WEBAUTHN_FAKE_PASSWORD_CREDENTIAL_FETCHER_H_
