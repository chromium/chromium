// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_PASSWORD_CREDENTIAL_FETCHER_H_
#define CHROME_BROWSER_WEBAUTHN_PASSWORD_CREDENTIAL_FETCHER_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "components/password_manager/core/browser/form_fetcher.h"
#include "components/password_manager/core/browser/password_form.h"
#include "content/public/browser/document_user_data.h"

namespace content {
class RenderFrameHost;
}

namespace password_manager {
class PasswordManagerClient;
}

// Fetches password credentials for a given RenderFrameHost and URL.
class PasswordCredentialFetcher
    : public password_manager::FormFetcher::Consumer {
 public:
  using PasswordCredentials =
      std::vector<std::unique_ptr<password_manager::PasswordForm>>;
  using PasswordCredentialsReceivedCallback =
      base::OnceCallback<void(PasswordCredentials)>;

  static std::unique_ptr<PasswordCredentialFetcher> Create(
      content::RenderFrameHost* rfh);
  static std::unique_ptr<PasswordCredentialFetcher> CreateForTesting(
      content::RenderFrameHost* rfh,
      std::unique_ptr<password_manager::FormFetcher> form_fetcher,
      password_manager::PasswordManagerClient* client);

  ~PasswordCredentialFetcher() override;

  PasswordCredentialFetcher(const PasswordCredentialFetcher&) = delete;
  PasswordCredentialFetcher& operator=(const PasswordCredentialFetcher&) =
      delete;

  // Fetches passwords for the given `url`. Invokes `callback` upon completion.
  // This may only be called once.
  virtual void FetchPasswords(const GURL& url,
                              PasswordCredentialsReceivedCallback callback);

  // Updates the `date_last_used` field of the password form matching `username`
  // and `password` to the current time. The update is persisted to the
  // password store.
  virtual void UpdateDateLastUsed(const std::u16string& username,
                                  const std::u16string& password);

  static void SetInstanceForTesting(PasswordCredentialFetcher* instance);

 private:
  friend class FakePasswordCredentialFetcher;

  explicit PasswordCredentialFetcher(content::RenderFrameHost* rfh);

  // FormFetcher::Consumer:
  void OnFetchCompleted() override;

  void CreateFormFetcher(const GURL& url);
  password_manager::PasswordManagerClient* GetPasswordManagerClient() const;

  raw_ptr<content::RenderFrameHost> rfh_;
  std::unique_ptr<password_manager::FormFetcher> form_fetcher_;
  PasswordCredentialsReceivedCallback callback_;

  raw_ptr<password_manager::PasswordManagerClient> pwm_client_for_testing_ =
      nullptr;

  // Owned by the test fixture.
  static PasswordCredentialFetcher* instance_for_testing_;
};

#endif  // CHROME_BROWSER_WEBAUTHN_PASSWORD_CREDENTIAL_FETCHER_H_
