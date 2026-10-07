// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/ambient/ambient_signin_controller.h"

#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webauthn/ambient/ambient_login_permission_controller.h"
#include "chrome/browser/ui/webauthn/ambient/ambient_login_permission_request.h"
#include "chrome/browser/ui/webauthn/webauthn_ui_helpers.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"

using content::RenderFrameHost;

namespace ambient_signin {

AmbientSigninController::~AmbientSigninController() {
  if (model_) {
    model_->observers.RemoveObserver(this);
    model_ = nullptr;
    Close();
  }
}

void AmbientSigninController::Show(AuthenticatorRequestDialogModel* model) {
  if (!model_) {
    model_ = model;
    model_->observers.AddObserver(this);
  } else {
    CHECK(model == model_);
  }

  credential_indices_.clear();
  for (size_t i = 0; i < model_->mechanisms.size(); ++i) {
    const auto& type = model_->mechanisms[i].type;
    if (std::holds_alternative<
            AuthenticatorRequestDialogModel::Mechanism::Credential>(type) ||
        std::holds_alternative<
            AuthenticatorRequestDialogModel::Mechanism::Password>(type)) {
      credential_indices_.push_back(i);
    }
  }

  CHECK(!credential_indices_.empty());

  auto* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  if (!web_contents) {
    return;
  }

  std::vector<ambient_signin::PasskeyOrPasswordCredential> credentials;
  credentials.reserve(credential_indices_.size());
  for (size_t index : credential_indices_) {
    const auto& mechanism = model_->mechanisms.at(index);
    ambient_signin::CredentialType cred_type =
        std::holds_alternative<
            AuthenticatorRequestDialogModel::Mechanism::Credential>(
            mechanism.type)
            ? ambient_signin::CredentialType::kPasskey
            : ambient_signin::CredentialType::kPassword;
    credentials.push_back({
        /*username=*/mechanism.name,
        /*provider_name=*/mechanism.description,
        /*type=*/cred_type,
        /*display_name=*/mechanism.display_name,
    });
  }

  auto* permission_controller =
      AmbientLoginPermissionController::GetOrCreateForPage(
          render_frame_host().GetPage());
  auto request = std::make_unique<AmbientLoginPermissionRequest>(
      render_frame_host().GetLastCommittedOrigin().GetURL(),
      render_frame_host().GetMainFrame()->GetLastCommittedOrigin().GetURL(),
      std::move(credentials),
      base::BindOnce(&AmbientSigninController::OnMechanismSelected,
                     GetWeakPtr()));
  permission_controller->RequestPermission(&render_frame_host(),
                                           std::move(request));
}

void AmbientSigninController::TriggerPageActionSignIn() {
  Close();
  OnMechanismSelected(0);
}

AmbientSigninController::AmbientSigninController(
    RenderFrameHost* render_frame_host)
    : content::DocumentUserData<AmbientSigninController>(render_frame_host) {}

void AmbientSigninController::OnMechanismSelected(size_t index) {
  CHECK(index < credential_indices_.size());
  if (model_) {
    model_->mechanisms.at(credential_indices_.at(index)).callback.Run();
  }
}

void AmbientSigninController::Close() {
  if (auto* permission_controller =
          AmbientLoginPermissionController::GetForPage(
              render_frame_host().GetPage())) {
    // TODO(crbug.com/532206357): Tell the permission controller that
    // passkey/password options are no longer available. If it is showing FedCM
    // it can remain open.
    permission_controller->Reset();
  }
}

void AmbientSigninController::OnRequestComplete() {
  if (model_) {
    model_->observers.RemoveObserver(this);
    model_ = nullptr;
  }
  Close();
}

void AmbientSigninController::OnModelDestroyed(
    AuthenticatorRequestDialogModel* model) {
  CHECK(model == model_);
  model_->observers.RemoveObserver(this);
  model_ = nullptr;
  Close();
}

base::WeakPtr<AmbientSigninController> AmbientSigninController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

DOCUMENT_USER_DATA_KEY_IMPL(AmbientSigninController);

}  // namespace ambient_signin
