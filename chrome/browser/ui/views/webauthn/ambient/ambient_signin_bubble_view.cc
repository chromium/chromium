// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/ambient/ambient_signin_bubble_view.h"

#include <cstddef>
#include <cstdint>
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
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/gfx/font_list.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/style/typography.h"
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
                       features::IsRoundedIconsEnabled()
                           ? vector_icons::kKeyboardArrowRightFlippableIcon
                           : vector_icons::kSubmenuArrowChromeRefreshOldIcon,
                       ui::kColorIcon, kIconSize));
}

}  // namespace

BEGIN_METADATA(AmbientSigninBubbleView)
END_METADATA

AmbientSigninBubbleView::AmbientSigninBubbleView(
    views::BubbleAnchor anchor,
    AmbientSigninController* controller)
    : BubbleDialogDelegateView(anchor, views::BubbleBorder::Arrow::TOP_RIGHT),
      controller_(controller) {
  SetShowCloseButton(true);
  UseCompactMargins();
  set_close_on_deactivate(false);
  SetShowIcon(true);
  SetIcon(ui::ImageModel::FromVectorIcon(GooglePasswordManagerVectorIcon()));
  SetShowTitle(true);
  SetTitle(
      l10n_util::GetStringFUTF16(IDS_WEBAUTHN_SIGN_IN_TO_WEBSITE_DIALOG_TITLE,
                                 controller_->GetRpIdForDisplay()));

  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical);
  layout->set_cross_axis_alignment(views::LayoutAlignment::kStretch);
  SetLayoutManager(std::move(layout));
}

AmbientSigninBubbleView::~AmbientSigninBubbleView() {
  if (controller_) {
    controller_->OnBubbleViewDestroyed();
    controller_ = nullptr;
  }
}

void AmbientSigninBubbleView::ShowCredentials(
    const std::vector<AuthenticatorRequestDialogModel::Mechanism>& mechanisms,
    const std::vector<size_t>& indices) {
  SetModeByCredentialCount(indices.size());
  for (size_t i = 0; i < indices.size(); ++i) {
    AddChildView(CreateRow(mechanisms.at(indices.at(i)), indices.at(i)));
  }
  SetButtonArea();

  Show();
}

void AmbientSigninBubbleView::Show() {
  if (!widget_) {
    widget_ = views::BubbleDialogDelegate::CreateBubbleDeprecated(
                  this, views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET)
                  ->GetWeakPtr();
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
  if (widget_) {
    widget_->Close();
  }
}

void AmbientSigninBubbleView::DisconnectController() {
  controller_ = nullptr;
  Close();
}

void AmbientSigninBubbleView::OnMechanismSelected(size_t index,
                                                  const ui::Event& event) {
  if (!controller_) {
    return;
  }
  controller_->OnMechanismSelected(index);
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

std::unique_ptr<views::View> AmbientSigninBubbleView::CreateRow(
    const AuthenticatorRequestDialogModel::Mechanism& mechanism,
    size_t index) {
  auto row = std::make_unique<HoverButton>(
      base::BindRepeating(&AmbientSigninBubbleView::OnMechanismSelected,
                          weak_ptr_factory_.GetWeakPtr(), index),
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          *mechanism.icon, ui::kColorIcon, kIconSize)),
      mechanism.name, mechanism.description, GetSecondaryIconForRow(mode_));
  return row;
}

}  // namespace ambient_signin
