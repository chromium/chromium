// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_AMBIENT_AMBIENT_SIGNIN_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEBAUTHN_AMBIENT_AMBIENT_SIGNIN_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "content/public/browser/document_user_data.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace ambient_signin {

// This class is responsible for displaying sign-in methods such as passkeys in
// an ambient permission prompt over the document. Its lifetime is bound to the
// blink document that it is tied to. It will be gone when the RenderFrameHost
// is deleted.
class AmbientSigninController
    : public AuthenticatorRequestDialogModel::Observer,
      public content::DocumentUserData<AmbientSigninController> {
 public:
  ~AmbientSigninController() override;

  // Shows the Ambient UI with the provided credentials.
  void Show(AuthenticatorRequestDialogModel* model);

  base::WeakPtr<AmbientSigninController> GetWeakPtr();

 private:
  // content::DocumentUserData<AmbientSigninController>:
  explicit AmbientSigninController(content::RenderFrameHost* render_frame_host);
  friend class content::DocumentUserData<AmbientSigninController>;
  DOCUMENT_USER_DATA_KEY_DECL();

  void OnMechanismSelected(size_t index);
  void Close();

  // AuthenticatorRequestDialogModel::Observer
  void OnRequestComplete() override;
  void OnModelDestroyed(AuthenticatorRequestDialogModel* model) override;

  raw_ptr<AuthenticatorRequestDialogModel> model_ = nullptr;
  std::vector<size_t> credential_indices_;

  base::WeakPtrFactory<AmbientSigninController> weak_ptr_factory_{this};
};

}  // namespace ambient_signin

#endif  // CHROME_BROWSER_UI_WEBAUTHN_AMBIENT_AMBIENT_SIGNIN_CONTROLLER_H_
