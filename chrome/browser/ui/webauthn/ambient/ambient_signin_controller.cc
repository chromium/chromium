// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/ambient/ambient_signin_controller.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/webauthn/ambient/ambient_signin_bubble_view.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/widget/widget.h"

namespace ambient_signin {

using content::RenderFrameHost;
using content::WebContents;

AmbientSigninController::~AmbientSigninController() {
  if (model_) {
    model_->observers.RemoveObserver(this);
    model_ = nullptr;
  }
  if (ambient_signin_bubble_view_) {
    ambient_signin_bubble_view_->NotifyWidgetDestroyed();
    ambient_signin_bubble_view_ = nullptr;
  }
}

void AmbientSigninController::AddAndShowWebAuthnMethods(
    AuthenticatorRequestDialogModel* model,
    const std::vector<password_manager::PasskeyCredential>& credentials,
    CredentialSelectionCallback callback) {
  if (!model_) {
    model_ = model;
    model_->observers.AddObserver(this);
  } else {
    CHECK(model == model_);
  }

  passkey_selection_callback_ = std::move(callback);
  // TODO: double check how this behaves if a conditional request is made while
  // the tab is in background.
  auto* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  if (!web_contents) {
    return;
  }
  auto* browser = chrome::FindBrowserWithTab(web_contents);
  ToolbarButtonProvider* button_provider =
      BrowserView::GetBrowserViewForBrowser(browser)->toolbar_button_provider();
  auto* anchor_view = button_provider->GetAnchorView(std::nullopt);

  if (!ambient_signin_bubble_view_) {
    ambient_signin_bubble_view_ =
        new AmbientSigninBubbleView(anchor_view, this);
    ambient_signin_bubble_view_->ShowPasskeys(credentials);
  } else {
    ambient_signin_bubble_view_->Update();
  }
}

AmbientSigninController::AmbientSigninController(
    RenderFrameHost* render_frame_host)
    : content::DocumentUserData<AmbientSigninController>(render_frame_host) {
  tabs::TabInterface* tab_interface_ = tabs::TabInterface::GetFromContents(
      WebContents::FromRenderFrameHost(render_frame_host));
  tab_subscriptions_.push_back(
      tab_interface_->RegisterWillEnterBackground(base::BindRepeating(
          &AmbientSigninController::TabWillEnterBackground, GetWeakPtr())));
  tab_subscriptions_.push_back(
      tab_interface_->RegisterDidEnterForeground(base::BindRepeating(
          &AmbientSigninController::TabDidEnterForeground, GetWeakPtr())));
}

void AmbientSigninController::OnPasskeySelected(
    const std::vector<uint8_t>& account_id,
    const ui::Event& event) {
  std::move(passkey_selection_callback_).Run(account_id);
}

void AmbientSigninController::OnWidgetDestroying(views::Widget* widget) {
  ambient_signin_bubble_view_->NotifyWidgetDestroyed();
  ambient_signin_bubble_view_ = nullptr;
}

void AmbientSigninController::OnRequestComplete() {
  if (!ambient_signin_bubble_view_) {
    return;
  }
  ambient_signin_bubble_view_->Close();
}

void AmbientSigninController::OnModelDestroyed(
    AuthenticatorRequestDialogModel* model) {
  CHECK(model == model_);
  model_->observers.RemoveObserver(this);
  model_ = nullptr;
}

void AmbientSigninController::TabWillEnterBackground(
    tabs::TabInterface* tab_interface) {
  if (!ambient_signin_bubble_view_) {
    return;
  }
  ambient_signin_bubble_view_->Hide();
}

void AmbientSigninController::TabDidEnterForeground(
    tabs::TabInterface* tab_interface) {
  if (!ambient_signin_bubble_view_) {
    return;
  }
  ambient_signin_bubble_view_->Show();
}

base::WeakPtr<AmbientSigninController> AmbientSigninController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

DOCUMENT_USER_DATA_KEY_IMPL(AmbientSigninController);

}  // namespace ambient_signin
