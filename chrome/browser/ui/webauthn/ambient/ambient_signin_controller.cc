// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/ambient/ambient_signin_controller.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/action_ids.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/webauthn/ambient/ambient_signin_bubble_view.h"
#include "chrome/browser/ui/webauthn/webauthn_ui_helpers.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/credentialmanagement/credential_type_flags.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"

using blink::mojom::CredentialTypeFlags;
using content::RenderFrameHost;
using content::WebContents;

namespace ambient_signin {

AmbientSigninController::~AmbientSigninController() {
  if (model_) {
    model_->observers.RemoveObserver(this);
    model_ = nullptr;
  }
  if (ambient_signin_bubble_view_) {
    ambient_signin_bubble_view_->DisconnectController();
    ambient_signin_bubble_view_ = nullptr;
  }
}

void AmbientSigninController::Show(
    AuthenticatorRequestDialogModel* model,
    std::vector<password_manager::PasskeyCredential> credentials,
    std::vector<std::unique_ptr<password_manager::PasswordForm>> forms,
    PasskeyCredentialSelectionCallback passkey_callback,
    PasswordCredentialSelectionCallback password_callback) {
  CHECK(!credentials.empty() || !forms.empty());
  if (!model_) {
    model_ = model;
    model_->observers.AddObserver(this);
  } else {
    CHECK(model == model_);
  }

  passkey_selection_callback_ = std::move(passkey_callback);
  password_selection_callback_ = std::move(password_callback);
  passkey_credentials_.swap(credentials);
  password_forms_.swap(forms);

  // It is TBD how multiple credentials will work with Page Actions.
  // This is a heuristic for the prototype:
  // If there is only one passkey, show the Page Action, ignoring passwords if
  // any are present. Else if there are multiple passkeys, show the bubble with
  // all credentials. Else if there is only one password, show the Page Action.
  // Else show the bubble with the multiple passwords.
  if (passkey_credentials_.size() != 1 &&
      !(passkey_credentials_.empty() && password_forms_.size() == 1)) {
    ui_type_ = UiType::kBubble;
    ShowBubbleView();
    return;
  }

  ui_type_ = UiType::kPageAction;
  ShowPageAction();
}

void AmbientSigninController::ShowPageAction() {
  auto* controller = GetPageActionController();
  if (!controller) {
    return;
  }

  if (passkey_credentials_.empty()) {
    // Showing a password.
    controller->OverrideText(
        kActionWebAuthnAmbientSignin,
        l10n_util::GetStringFUTF16(IDS_WEBAUTHN_SIGN_IN_AS_PROMPT,
                                   password_forms_.at(0)->username_value));
    controller->OverrideImage(
        kActionWebAuthnAmbientSignin,
        ui::ImageModel::FromVectorIcon(kPasswordFieldIcon, ui::kColorIcon));
  } else {
    controller->OverrideText(
        kActionWebAuthnAmbientSignin,
        l10n_util::GetStringFUTF16(
            IDS_WEBAUTHN_SIGN_IN_AS_PROMPT,
            base::UTF8ToUTF16(passkey_credentials_.at(0).username())));
    controller->OverrideImage(kActionWebAuthnAmbientSignin,
                              ui::ImageModel::FromVectorIcon(
                                  vector_icons::kPasskeyIcon, ui::kColorIcon));
  }

  controller->Show(kActionWebAuthnAmbientSignin);
  controller->ShowSuggestionChip(kActionWebAuthnAmbientSignin);
}

void AmbientSigninController::TriggerPageActionSignIn() {
  Close();

  if (!passkey_credentials_.empty()) {
    OnPasskeySelected(passkey_credentials_.begin()->credential_id());
    return;
  }
  CHECK(!password_forms_.empty());
  OnPasswordSelected(password_forms_.begin()->get());
}

void AmbientSigninController::ShowBubbleView() {
  auto* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  if (!web_contents ||
      web_contents->GetVisibility() == content::Visibility::HIDDEN) {
    return;
  }
  BrowserWindowInterface* browser = chrome::FindBrowserWithTab(web_contents);
  ToolbarButtonProvider* button_provider =
      BrowserView::GetBrowserViewForBrowser(browser)->toolbar_button_provider();
  auto anchor = button_provider->GetBubbleAnchor(std::nullopt);

  if (!ambient_signin_bubble_view_) {
    ambient_signin_bubble_view_ = new AmbientSigninBubbleView(anchor, this);
    ambient_signin_bubble_view_->ShowCredentials(passkey_credentials_,
                                                 password_forms_);
  }
}

AmbientSigninController::AmbientSigninController(
    RenderFrameHost* render_frame_host)
    : content::DocumentUserData<AmbientSigninController>(render_frame_host) {
  // TODO(crbug.com/358119268): This crashes if a request happens from a
  // WebContents that is not inside a tab.
  tabs::TabInterface* tab_interface_ = tabs::TabInterface::GetFromContents(
      WebContents::FromRenderFrameHost(render_frame_host));
  tab_subscriptions_.push_back(
      tab_interface_->RegisterWillDeactivate(base::BindRepeating(
          &AmbientSigninController::TabWillEnterBackground, GetWeakPtr())));
  tab_subscriptions_.push_back(
      tab_interface_->RegisterDidActivate(base::BindRepeating(
          &AmbientSigninController::TabDidEnterForeground, GetWeakPtr())));
}

void AmbientSigninController::OnPasskeySelected(
    const std::vector<uint8_t>& account_id) {
  ui_type_ = UiType::kNone;
  std::move(passkey_selection_callback_).Run(account_id);
}

void AmbientSigninController::OnPasswordSelected(
    const password_manager::PasswordForm* form) {
  ui_type_ = UiType::kNone;
  std::move(password_selection_callback_)
      .Run(std::make_pair(form->username_value, form->password_value));
}

std::u16string AmbientSigninController::GetRpIdForDisplay() const {
  views::LayoutProvider* layout_provider = views::LayoutProvider::Get();
  int title_width =
      layout_provider->GetDistanceMetric(
          views::DISTANCE_BUBBLE_PREFERRED_WIDTH) -
      layout_provider->GetInsetsMetric(views::INSETS_DIALOG).width() -
      layout_provider->GetInsetsMetric(views::INSETS_DIALOG_TITLE).width();
  gfx::FontList font_list = views::TypographyProvider::Get().GetFont(
      views::style::CONTEXT_DIALOG_TITLE, views::style::STYLE_PRIMARY);
  return model_ ? webauthn_ui_helpers::RpIdToElidedHost(
                      model_->relying_party_id, title_width, font_list)
                : std::u16string();
}

base::OnceClosure AmbientSigninController::GetSignInCallback() {
  CHECK(password_forms_.size() + passkey_credentials_.size() == 1);
  if (!password_forms_.empty()) {
    return base::BindOnce(&AmbientSigninController::OnPasswordSelected,
                          GetWeakPtr(), password_forms_.begin()->get());
  }
  return base::BindOnce(&AmbientSigninController::OnPasskeySelected,
                        GetWeakPtr(),
                        passkey_credentials_.begin()->credential_id());
}

void AmbientSigninController::Close() {
  if (ambient_signin_bubble_view_) {
    ambient_signin_bubble_view_->DisconnectController();
    ambient_signin_bubble_view_ = nullptr;
  }
  if (auto* controller = GetPageActionController()) {
    controller->Hide(kActionWebAuthnAmbientSignin);
    controller->HideSuggestionChip(kActionWebAuthnAmbientSignin);
  }
}

void AmbientSigninController::OnBubbleViewDestroyed() {
  ambient_signin_bubble_view_ = nullptr;
}

void AmbientSigninController::OnRequestComplete() {
  Close();
}

void AmbientSigninController::OnModelDestroyed(
    AuthenticatorRequestDialogModel* model) {
  CHECK(model == model_);
  model_->observers.RemoveObserver(this);
  model_ = nullptr;
  Close();
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
  if (ui_type_ == UiType::kBubble) {
    ShowBubbleView();
  }
}

page_actions::PageActionController*
AmbientSigninController::GetPageActionController() {
  auto* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  if (!web_contents || web_contents->IsBeingDestroyed()) {
    return nullptr;
  }

  tabs::TabInterface* tab_interface =
      tabs::TabInterface::GetFromContents(web_contents);

  tabs::TabFeatures* tab_features = tab_interface->GetTabFeatures();
  if (!tab_features) {
    return nullptr;
  }

  return tab_features->page_action_controller();
}

base::WeakPtr<AmbientSigninController> AmbientSigninController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

DOCUMENT_USER_DATA_KEY_IMPL(AmbientSigninController);

}  // namespace ambient_signin
