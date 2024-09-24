// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_save_unsynced_credentials_locally_view.h"

#include <numeric>
#include <string>
#include <utility>

#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/passwords/views_utils.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/window/dialog_delegate.h"

PasswordSaveUnsyncedCredentialsLocallyView::
    PasswordSaveUnsyncedCredentialsLocallyView(
        content::WebContents* web_contents,
        views::View* anchor_view)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*easily_dismissable=*/false),
      controller_(PasswordsModelDelegateFromWebContents(web_contents)) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk) |
             static_cast<int>(ui::mojom::DialogButton::kCancel));
  SetAcceptCallback(
      base::BindOnce(&PasswordSaveUnsyncedCredentialsLocallyView::OnSaveClicked,
                     base::Unretained(this)));
  SetButtonLabel(
      ui::mojom::DialogButton::kOk,
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_SAVE_UNSYNCED_CREDENTIALS_BUTTON_GPM));
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 l10n_util::GetStringUTF16(
                     IDS_PASSWORD_MANAGER_DISCARD_UNSYNCED_CREDENTIALS_BUTTON));
  SetCancelCallback(base::BindOnce(
      &SaveUnsyncedCredentialsLocallyBubbleController::OnCancelClicked,
      base::Unretained(&controller_)));
  SetShowIcon(true);
  CreateLayout();
}

PasswordSaveUnsyncedCredentialsLocallyView::
    ~PasswordSaveUnsyncedCredentialsLocallyView() = default;

PasswordBubbleControllerBase*
PasswordSaveUnsyncedCredentialsLocallyView::GetController() {
  return &controller_;
}

const PasswordBubbleControllerBase*
PasswordSaveUnsyncedCredentialsLocallyView::GetController() const {
  return &controller_;
}

ui::ImageModel PasswordSaveUnsyncedCredentialsLocallyView::GetWindowIcon() {
  return ui::ImageModel::FromVectorIcon(GooglePasswordManagerVectorIcon(),
                                        ui::kColorIcon);
}

void PasswordSaveUnsyncedCredentialsLocallyView::CreateLayout() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  auto description = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_UNSYNCED_CREDENTIALS_BUBBLE_DESCRIPTION_GPM),
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_HINT);
  description->SetMultiLine(true);
  description->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  description->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(0, 0,
                        ChromeLayoutProvider::Get()->GetDistanceMetric(
                            DISTANCE_RELATED_CONTROL_VERTICAL_SMALL),
                        0)));
  AddChildView(std::move(description));

  DCHECK(!controller_.unsynced_credentials().empty());
  for (const password_manager::PasswordForm& form :
       controller_.unsynced_credentials()) {
    auto* row_view = AddChildView(std::make_unique<views::View>());
    auto* checkbox = row_view->AddChildView(std::make_unique<views::Checkbox>(
        std::u16string(), views::Button::PressedCallback()));
    checkbox->SetCallback(base::BindRepeating(
        &PasswordSaveUnsyncedCredentialsLocallyView::ButtonPressed,
        base::Unretained(this), base::Unretained(checkbox)));
    checkbox->SetBorder(views::CreateEmptyBorder(
        gfx::Insets::TLBR(0, 0, 0,
                          ChromeLayoutProvider::Get()->GetDistanceMetric(
                              DISTANCE_RELATED_CONTROL_HORIZONTAL_SMALL))));
    // Usually all passwords should be saved, so they're selected by default.
    checkbox->SetChecked(true);
    num_selected_checkboxes_++;
    auto* username_label = row_view->AddChildView(CreateUsernameLabel(form));
    checkbox->GetViewAccessibility().SetName(*username_label);
    auto* password_label = row_view->AddChildView(CreatePasswordLabel(form));
    auto* row_layout =
        row_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal));
    row_layout->SetFlexForView(username_label, 1);
    row_layout->SetFlexForView(password_label, 1);

    checkboxes_.push_back(checkbox);
  }
}

void PasswordSaveUnsyncedCredentialsLocallyView::ButtonPressed(
    views::Checkbox* checkbox) {
  num_selected_checkboxes_ += checkbox->GetChecked() ? 1 : -1;
  GetOkButton()->SetState(num_selected_checkboxes_
                              ? views::Button::ButtonState::STATE_NORMAL
                              : views::Button::ButtonState::STATE_DISABLED);
}

void PasswordSaveUnsyncedCredentialsLocallyView::OnSaveClicked() {
  std::vector<bool> was_credential_selected;
  for (const views::Checkbox* checkbox : checkboxes_) {
    was_credential_selected.push_back(checkbox->GetChecked());
  }
  controller_.OnSaveClicked(was_credential_selected);
}

BEGIN_METADATA(PasswordSaveUnsyncedCredentialsLocallyView)
END_METADATA
