// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_PASSWORD_CREDENTIAL_CONTROLLER_H_
#define CHROME_BROWSER_WEBAUTHN_PASSWORD_CREDENTIAL_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "components/password_manager/core/browser/form_fetcher.h"
#include "components/password_manager/core/browser/form_fetcher_impl.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_digest.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"

namespace webauthn {

using content::RenderFrameHost;
using password_manager::FormFetcher;
using password_manager::FormFetcherImpl;
using password_manager::PasswordForm;
using password_manager::PasswordFormDigest;
using password_manager::PasswordManagerClient;

// This class is responsible for fetching `PasswordCredentials` for the given
// `render_frame_host` and responding with the found credentials for a `url`.
class PasswordCredentialController {
 public:
  using PasswordCredentials = std::vector<std::unique_ptr<PasswordForm>>;
  using PasswordCredentialsReceivedCallback =
      base::OnceCallback<void(PasswordCredentials)>;

  // Returns a `PasswordCredentialController` if the render frame host is the
  // main and parent frame, `nullptr` otherwise.
  static PasswordCredentialController* MaybeGet(
      RenderFrameHost* render_frame_host);

  virtual void FetchPasswords(const GURL& url,
                              PasswordCredentialsReceivedCallback callback);

  static void set_instance_for_testing(PasswordCredentialController* instance);

 private:
  // Should be owned by the test:
  static PasswordCredentialController* g_instance_for_testing_;
};

}  // namespace webauthn

#endif  // CHROME_BROWSER_WEBAUTHN_PASSWORD_CREDENTIAL_CONTROLLER_H_
