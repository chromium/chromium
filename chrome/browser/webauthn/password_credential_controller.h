// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_PASSWORD_CREDENTIAL_CONTROLLER_H_
#define CHROME_BROWSER_WEBAUTHN_PASSWORD_CREDENTIAL_CONTROLLER_H_

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/functional/callback.h"
#include "components/password_manager/core/browser/form_fetcher.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/render_frame_host.h"

namespace webauthn {

using content::AuthenticatorRequestClientDelegate;
using password_manager::PasswordForm;

// This class is responsible for fetching `PasswordCredentials` for the given
// `render_frame_host` and responding with the found credentials for a `url`.
class PasswordCredentialController {
 public:
  using PasswordPair = std::pair<std::u16string, std::u16string>;
  using PasswordCredentials = std::vector<std::unique_ptr<PasswordForm>>;
  using PasswordCredentialsReceivedCallback =
      base::OnceCallback<void(PasswordCredentials)>;

  virtual ~PasswordCredentialController() = default;

  virtual void FetchPasswords(const GURL& url,
                              PasswordCredentialsReceivedCallback callback);

  // Returns `true` if the user is required to pass screen lock before using a
  // credential.
  virtual bool IsAuthRequired();

  virtual void SetPasswordSelectedCallback(
      AuthenticatorRequestClientDelegate::PasswordSelectedCallback callback);
};

}  // namespace webauthn

#endif  // CHROME_BROWSER_WEBAUTHN_PASSWORD_CREDENTIAL_CONTROLLER_H_
