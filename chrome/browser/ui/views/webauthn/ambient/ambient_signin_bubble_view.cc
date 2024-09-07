// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/ambient/ambient_signin_bubble_view.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/i18n/rtl.h"
#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/webauthn/ambient/ambient_signin_controller.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/common/password_manager_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"

namespace ambient_signin {

BEGIN_METADATA(AmbientSigninBubbleView)
END_METADATA

AmbientSigninBubbleView::AmbientSigninBubbleView(
    View* anchor_view,
    AmbientSigninController* controller)
    : BubbleDialogDelegateView(anchor_view,
                               views::BubbleBorder::Arrow::TOP_RIGHT),
      controller_(controller) {
  set_fixed_width(375);
  set_close_on_deactivate(false);
  SetShowTitle(true);
  SetTitle(u"Select a passkey to sign in");
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));

  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical);
  layout->set_cross_axis_alignment(views::LayoutAlignment::kStart);
  SetLayoutManager(std::move(layout));
}

AmbientSigninBubbleView::~AmbientSigninBubbleView() = default;

void AmbientSigninBubbleView::ShowCredentials(
    const std::vector<password_manager::PasskeyCredential>& credentials,
    const std::vector<std::unique_ptr<password_manager::PasswordForm>>& forms) {
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

  Show();
}

void AmbientSigninBubbleView::Show() {
  if (!widget_) {
    widget_ = BubbleDialogDelegateView::CreateBubble(this)->GetWeakPtr();
    widget_->AddObserver(controller_);
  }
  widget_->Show();
}

void AmbientSigninBubbleView::Update() {
  NOTIMPLEMENTED();
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

std::unique_ptr<views::View> AmbientSigninBubbleView::CreatePasskeyRow(
    const password_manager::PasskeyCredential& passkey) {
  auto row = std::make_unique<HoverButton>(
      base::BindRepeating(&AmbientSigninController::OnPasskeySelected,
                          controller_->GetWeakPtr(), passkey.credential_id()),
      /*icon_view=*/nullptr,
      /*title=*/base::UTF8ToUTF16(passkey.username()),
      /*subtitle=*/passkey.GetAuthenticatorLabel());
  return row;
}

std::unique_ptr<views::View> AmbientSigninBubbleView::CreatePasswordRow(
    const password_manager::PasswordForm* form) {
  auto row = std::make_unique<HoverButton>(
      base::BindRepeating(&AmbientSigninController::OnPasswordSelected,
                          controller_->GetWeakPtr(), form),
      /*icon_view=*/nullptr,
      /*title=*/form->username_value,
      /*subtitle=*/
      std::u16string(form->password_value.length(),
                     password_manager::constants::kPasswordReplacementChar));
  return row;
}

}  // namespace ambient_signin
