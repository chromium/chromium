// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/ambient/ambient_signin_bubble_view.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/webauthn/ambient/ambient_signin_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/common/password_manager_constants.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/color/color_id.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"

namespace ambient_signin {

namespace {

constexpr int kBubbleWidth = 320;
constexpr int kIconSize = 20;

std::unique_ptr<views::ImageView> GetSecondaryIconForRow(
    AmbientSigninBubbleView::Mode mode) {
  return mode == AmbientSigninBubbleView::Mode::kSingleCredential
             ? nullptr
             : std::make_unique<views::ImageView>(
                   ui::ImageModel::FromVectorIcon(
                       vector_icons::kSubmenuArrowChromeRefreshIcon,
                       ui::kColorIcon, kIconSize));
}

}  // namespace

BEGIN_METADATA(AmbientSigninBubbleView)
END_METADATA

AmbientSigninBubbleView::AmbientSigninBubbleView(
    View* anchor_view,
    AmbientSigninController* controller)
    : BubbleDialogDelegateView(anchor_view,
                               views::BubbleBorder::Arrow::TOP_RIGHT),
      controller_(controller) {
  SetShowCloseButton(true);
  UseCompactMargins();
  set_close_on_deactivate(false);
  SetShowIcon(true);
  SetIcon(ui::ImageModel::FromVectorIcon(GooglePasswordManagerVectorIcon()));
  SetShowTitle(true);
  // TODO(crbug.com/358119268): Make sure to  translate the final string.
  SetTitle(l10n_util::GetStringFUTF16(IDS_WEBAUTHN_AMBIENT_BUBBLE_TITLE,
                                      controller_->GetRpId()));

  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical);
  layout->set_cross_axis_alignment(views::LayoutAlignment::kStretch);
  SetLayoutManager(std::move(layout));
}

AmbientSigninBubbleView::~AmbientSigninBubbleView() = default;

void AmbientSigninBubbleView::ShowCredentials(
    const std::vector<password_manager::PasskeyCredential>& credentials,
    const std::vector<std::unique_ptr<password_manager::PasswordForm>>& forms) {
  SetModeByCredentialCount(credentials.size() + forms.size());
  for (const auto& passkey : credentials) {
    AddChildView(CreatePasskeyRow(passkey));
  }

  for (const auto& form : forms) {
    // TODO(ambient): For now we ignore federated credentials, but these will
    // likely need to be displayed in the future.
    if (form->IsFederatedCredential()) {
      continue;
    }
    AddChildView(CreatePasswordRow(form.get()));
  }
  SetButtonArea();

  Show();
}

void AmbientSigninBubbleView::Show() {
  if (!widget_) {
    widget_ = BubbleDialogDelegateView::CreateBubble(this)->GetWeakPtr();
    widget_->AddObserver(controller_);
  }
  widget_->Show();
}

void AmbientSigninBubbleView::Hide() {
  if (!widget_) {
    return;
  }
  widget_->Hide();
}

void AmbientSigninBubbleView::Close() {
  widget_->Close();
}

void AmbientSigninBubbleView::NotifyWidgetDestroyed() {
  widget_->RemoveObserver(controller_);
  controller_ = nullptr;
  BubbleDialogDelegateView::OnWidgetDestroying(widget_.get());
}

void AmbientSigninBubbleView::OnPasskeySelected(
    const std::vector<uint8_t>& account_id,
    const ui::Event& event) {
  if (!controller_) {
    return;
  }
  controller_->OnPasskeySelected(account_id);
  Hide();
}

void AmbientSigninBubbleView::OnPasswordSelected(
    const password_manager::PasswordForm* form,
    const ui::Event& event) {
  if (!controller_) {
    return;
  }
  controller_->OnPasswordSelected(form);
  Hide();
}

void AmbientSigninBubbleView::SetModeByCredentialCount(
    size_t credential_count) {
  mode_ =
      credential_count == 1 ? Mode::kSingleCredential : Mode::kAllCredentials;
}

void AmbientSigninBubbleView::SetButtonArea() {
  int buttons;
  switch (mode_) {
    case AmbientSigninBubbleView::Mode::kAllCredentials:
      buttons = static_cast<int>(ui::mojom::DialogButton::kNone);
      break;
    case AmbientSigninBubbleView::Mode::kSingleCredential:
      buttons = static_cast<int>(ui::mojom::DialogButton::kCancel) |
                static_cast<int>(ui::mojom::DialogButton::kOk);
      SetButtonLabel(ui::mojom::DialogButton::kCancel,
                     l10n_util::GetStringUTF16(IDS_NOT_NOW));
      SetCancelCallback(base::BindOnce(&AmbientSigninBubbleView::Close,
                                       weak_ptr_factory_.GetWeakPtr()));
      SetButtonLabel(ui::mojom::DialogButton::kOk,
                     l10n_util::GetStringUTF16(
                         IDS_PASSWORD_MANAGER_ACCOUNT_CHOOSER_SIGN_IN));
      SetAcceptCallback(controller_->GetSignInCallback());
      break;
  }
  SetButtons(buttons);
  set_fixed_width(kBubbleWidth);
}

std::unique_ptr<views::View> AmbientSigninBubbleView::CreatePasskeyRow(
    const password_manager::PasskeyCredential& passkey) {
  auto row = std::make_unique<HoverButton>(
      base::BindRepeating(&AmbientSigninBubbleView::OnPasskeySelected,
                          weak_ptr_factory_.GetWeakPtr(),
                          passkey.credential_id()),
      /*icon_view=*/
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          vector_icons::kPasskeyIcon, ui::kColorIcon, kIconSize)),
      /*title=*/base::UTF8ToUTF16(passkey.username()),
      /*subtitle=*/passkey.GetAuthenticatorLabel(),
      /*secondary_view=*/GetSecondaryIconForRow(mode_));
  return row;
}

std::unique_ptr<views::View> AmbientSigninBubbleView::CreatePasswordRow(
    const password_manager::PasswordForm* form) {
  auto row = std::make_unique<HoverButton>(
      base::BindRepeating(&AmbientSigninBubbleView::OnPasswordSelected,
                          weak_ptr_factory_.GetWeakPtr(), form),
      /*icon_view=*/
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          kPasswordFieldIcon, ui::kColorIcon, kIconSize)),
      /*title=*/form->username_value,
      /*subtitle=*/
      std::u16string(form->password_value.length(),
                     password_manager::constants::kPasswordReplacementChar),
      /*secondary_view=*/GetSecondaryIconForRow(mode_));
  return row;
}

}  // namespace ambient_signin
