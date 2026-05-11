// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_WEBAUTHN_AMBIENT_AMBIENT_SIGNIN_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEBAUTHN_AMBIENT_AMBIENT_SIGNIN_CONTROLLER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "content/public/browser/document_user_data.h"

struct AuthenticatorRequestDialogModel;

namespace base {
class CallbackListSubscription;
}  // namespace base

namespace content {
class RenderFrameHost;
}  // namespace content

namespace page_actions {
class PageActionController;
}  // namespace page_actions

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace ambient_signin {

class AmbientSigninBubbleView;

// This class is responsible for displaying sign-in methods such as passkeys in
// a bubble like view over the document. Its lifetime is bound to the blink
// document that it is tied to. It will be gone when the RenderFrameHost is
// deleted.
// TODO(ambient): Move this class to c/b/ui/ambient and include other types of
// sign-in methods (e.g. FedCM)
class AmbientSigninController
    : public AuthenticatorRequestDialogModel::Observer,
      public content::DocumentUserData<AmbientSigninController> {
 public:
  using PasskeyCredentialSelectionCallback =
      base::OnceCallback<void(const std::vector<uint8_t>)>;
  using PasswordCredentialSelectionCallback =
      base::OnceCallback<void(PasswordCredentialPair)>;

  ~AmbientSigninController() override;

  // Shows the Ambient UI with the provided credentials.
  void Show(AuthenticatorRequestDialogModel* model);

  void TriggerPageActionSignIn();

  // Called when a mechanism is selected.
  void OnMechanismSelected(size_t index);

  std::u16string GetRpIdForDisplay() const;
  base::OnceClosure GetSignInCallback();
  void OnBubbleViewDestroyed();

  void SetPageActionControllerForTesting(
      page_actions::PageActionController* controller);

  base::WeakPtr<AmbientSigninController> GetWeakPtr();

 private:
  enum class UiType {
    kNone,
    kBubble,
    kPageAction,
  };

  // content::DocumentUserData<AmbientSigninController>:
  explicit AmbientSigninController(content::RenderFrameHost* render_frame_host);
  friend class content::DocumentUserData<AmbientSigninController>;
  DOCUMENT_USER_DATA_KEY_DECL();

  void ShowBubbleView();
  void ShowPageAction();

  void Close();

  // AuthenticatorRequestDialogModel::Observer
  void OnRequestComplete() override;
  void OnModelDestroyed(AuthenticatorRequestDialogModel* model) override;

  // tabs::TabInterface related overrides:
  void TabWillEnterBackground(tabs::TabInterface* tab_interface);
  void TabDidEnterForeground(tabs::TabInterface* tab_interface);

  page_actions::PageActionController* GetPageActionController();

  std::vector<base::CallbackListSubscription> tab_subscriptions_;
  raw_ptr<AmbientSigninBubbleView> ambient_signin_bubble_view_;

  raw_ptr<AuthenticatorRequestDialogModel> model_;
  std::vector<size_t> credential_indices_;

  // Set when `Show()` is called. Retains the UI type until `Show()` is called
  // again.
  UiType ui_type_ = UiType::kNone;

  raw_ptr<page_actions::PageActionController>
      page_action_controller_test_override_ = nullptr;

  base::WeakPtrFactory<AmbientSigninController> weak_ptr_factory_{this};
};

}  // namespace ambient_signin

#endif  // CHROME_BROWSER_UI_WEBAUTHN_AMBIENT_AMBIENT_SIGNIN_CONTROLLER_H_
