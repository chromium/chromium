// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_AMBIENT_AMBIENT_SIGNIN_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEBAUTHN_AMBIENT_AMBIENT_SIGNIN_CONTROLLER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "content/public/browser/document_user_data.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace permissions {
struct PermissionPromptDecision;
struct PermissionRequestData;
}  // namespace permissions

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

  void OnMechanismSelected(size_t index, bool from_expanded_list);
  void OnBubbleShown();
  void OnBubbleDismissed();
  void OnPermissionDecided(
      const permissions::PermissionPromptDecision& decision,
      const permissions::PermissionRequestData& request_data);
  void FinalizeRequestMetrics();
  void Close();

  // AuthenticatorRequestDialogModel::Observer
  void OnStartOver() override;
  void OnRequestComplete() override;
  void OnModelDestroyed(AuthenticatorRequestDialogModel* model) override;

  raw_ptr<AuthenticatorRequestDialogModel> model_ = nullptr;
  std::vector<size_t> credential_indices_;

  // Information about request flow that is tracked for metrics collection.
  ukm::SourceId ukm_source_id_ = ukm::kInvalidSourceId;
  bool is_restarting_ = false;
  bool request_active_ = false;
  bool request_started_as_bubble_ = false;
  bool bubble_shown_ = false;
  bool bubble_dismissed_ = false;
  bool credential_selected_ = false;
  bool selected_from_expanded_list_ = false;

  base::WeakPtrFactory<AmbientSigninController> weak_ptr_factory_{this};
};

}  // namespace ambient_signin

#endif  // CHROME_BROWSER_UI_WEBAUTHN_AMBIENT_AMBIENT_SIGNIN_CONTROLLER_H_
