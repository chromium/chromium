// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_add_username_view.h"

#include "base/functional/callback.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/passwords/views_utils.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/editable_combobox/editable_combobox.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"

namespace {
std::unique_ptr<views::Label> CreateBodyText(int margin_size) {
  auto body_text = std::make_unique<views::Label>();
  body_text->SetText(l10n_util::GetStringUTF16(IDS_ADD_USERNAME_BODY));
  body_text->SetMultiLine(true);
  body_text->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  body_text->SizeToFit(views::LayoutProvider::Get()->GetDistanceMetric(
                           views::DISTANCE_BUBBLE_PREFERRED_WIDTH) -
                       margin_size);
  return body_text;
}

std::unique_ptr<views::View> CreatePasswordLabelWithEyeIconView(
    std::unique_ptr<views::Label> password_label) {
  auto password_label_with_eye_icon_view =
      std::make_unique<views::BoxLayoutView>();
  auto* password_label_ptr = password_label_with_eye_icon_view->AddChildView(
      std::move(password_label));
  password_label_ptr->SetTextStyle(views::style::STYLE_PRIMARY);
  password_label_ptr->SetProperty(views::kBoxLayoutFlexKey,
                                  views::BoxLayoutFlexSpecification());

  auto* eye_icon = password_label_with_eye_icon_view->AddChildView(
      CreateVectorToggleImageButton(views::Button::PressedCallback()));
  eye_icon->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_SHOW_PASSWORD));
  eye_icon->SetToggledTooltipText(
      l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_HIDE_PASSWORD));
  eye_icon->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  views::SetImageFromVectorIconWithColorId(eye_icon, views::kEyeIcon,
                                           ui::kColorIcon, ui::kColorIcon);
  views::SetToggledImageFromVectorIconWithColorId(
      eye_icon, views::kEyeCrossedIcon, ui::kColorIcon, ui::kColorIcon);
  eye_icon->SetCallback(base::BindRepeating(
      [](views::ToggleImageButton* toggle_button,
         views::Label* password_label) {
        password_label->SetObscured(!password_label->GetObscured());
        toggle_button->SetToggled(!toggle_button->GetToggled());
      },
      eye_icon, password_label_ptr));

  return password_label_with_eye_icon_view;
}

// Adds empty border around the password field to make it have the same offsets
// as the username field above.
void AddEmptyBorder(views::View* password_field) {
  const views::LayoutProvider* provider = views::LayoutProvider::Get();
  password_field->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      provider->GetDistanceMetric(
          views::DISTANCE_CONTROL_VERTICAL_TEXT_PADDING),
      provider->GetDistanceMetric(
          views::DISTANCE_TEXTFIELD_HORIZONTAL_TEXT_PADDING),
      provider->GetDistanceMetric(
          views::DISTANCE_CONTROL_VERTICAL_TEXT_PADDING),
      provider->GetDistanceMetric(
          views::DISTANCE_TEXTFIELD_HORIZONTAL_TEXT_PADDING))));
}
}  // namespace

PasswordAddUsernameView::PasswordAddUsernameView(
    content::WebContents* web_contents,
    views::View* anchor_view,
    DisplayReason reason)
    : PasswordBubbleViewBase(web_contents, anchor_view, true),
      controller_(PasswordsModelDelegateFromWebContents(web_contents),
                  PasswordBubbleControllerBase::DisplayReason::kAutomatic) {
  CHECK_EQ(controller_.state(),
           password_manager::ui::GENERATED_PASSWORD_CONFIRMATION_STATE);
  const password_manager::PasswordForm& password_form =
      controller_.pending_password();

  // Set up layout:
  SetLayoutManager(std::make_unique<views::FillLayout>());
  views::View* root_view = AddChildView(std::make_unique<views::View>());
  views::FlexLayout* flex_layout =
      root_view->SetLayoutManager(std::make_unique<views::FlexLayout>());
  flex_layout->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetIgnoreDefaultMainAxisMargins(true)
      .SetCollapseMargins(true)
      .SetDefault(
          views::kMarginsKey,
          gfx::Insets::VH(ChromeLayoutProvider::Get()->GetDistanceMetric(
                              DISTANCE_CONTROL_LIST_VERTICAL),
                          0));

  std::unique_ptr<views::Label> body_text = CreateBodyText(margins().width());
  root_view->AddChildView(std::move(body_text));

  std::unique_ptr<views::EditableCombobox> username_dropdown =
      CreateUsernameEditableCombobox(password_form);
  username_dropdown->SetCallback(base::BindRepeating(
      &PasswordAddUsernameView::OnUsernameChanged, base::Unretained(this)));
  username_dropdown_ = username_dropdown.get();

  std::unique_ptr<views::Label> password_label =
      CreatePasswordLabel(password_form);
  std::unique_ptr<views::View> password_field =
      CreatePasswordLabelWithEyeIconView(std::move(password_label));
  AddEmptyBorder(password_field.get());

  BuildCredentialRows(root_view, std::move(username_dropdown),
                      std::move(password_field));

  SetAcceptCallback(
      base::BindOnce(&PasswordAddUsernameView::UpdateUsernameInModel,
                     base::Unretained(this))
          .Then(base::BindOnce(&AddUsernameBubbleController::OnSaveClicked,
                               base::Unretained(&controller_))));
  SetCancelCallback(
      base::BindOnce(&PasswordAddUsernameView::UpdateUsernameInModel,
                     base::Unretained(this))
          .Then(base::BindOnce(&AddUsernameBubbleController::OnNoThanksClicked,
                               base::Unretained(&controller_))));

  SetShowIcon(true);
  SetFootnoteView(CreateFooterView());
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk) |
             static_cast<int>(ui::mojom::DialogButton::kCancel));

  SetButtonEnabled(ui::mojom::DialogButton::kOk, false);
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(IDS_ADD_USERNAME));
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_CANCEL_BUTTON));

  SetTitle(controller_.GetTitle());
}

PasswordAddUsernameView::~PasswordAddUsernameView() = default;

PasswordBubbleControllerBase* PasswordAddUsernameView::GetController() {
  return &controller_;
}

const PasswordBubbleControllerBase* PasswordAddUsernameView::GetController()
    const {
  return &controller_;
}

views::View* PasswordAddUsernameView::GetInitiallyFocusedView() {
  return username_dropdown_;
}

ui::ImageModel PasswordAddUsernameView::GetWindowIcon() {
  return ui::ImageModel::FromVectorIcon(GooglePasswordManagerVectorIcon(),
                                        ui::kColorIcon);
}

void PasswordAddUsernameView::AddedToWidget() {
  SetBubbleHeader(IDR_SAVE_PASSWORD, IDR_SAVE_PASSWORD_DARK);
}

void PasswordAddUsernameView::UpdateUsernameInModel() {
  CHECK_EQ(controller_.state(),
           password_manager::ui::GENERATED_PASSWORD_CONFIRMATION_STATE);
  std::u16string new_username = username_dropdown_->GetText();
  base::TrimString(new_username, u" ", &new_username);

  controller_.OnCredentialEdited(std::move(new_username),
                                 controller_.pending_password().password_value);
}

std::unique_ptr<views::View> PasswordAddUsernameView::CreateFooterView() {
  CHECK_EQ(controller_.state(),
           password_manager::ui::GENERATED_PASSWORD_CONFIRMATION_STATE);
  base::RepeatingClosure open_password_manager_closure = base::BindRepeating(
      [](AddUsernameBubbleController* controller) {
        controller->OnGooglePasswordManagerLinkClicked(
            password_manager::ManagePasswordsReferrer::kAddUsernameBubble);
      },
      &controller_);

  return CreateGooglePasswordManagerLabel(
      /*text_message_id=*/
      IDS_PASSWORD_BUBBLES_FOOTER_SYNCED_TO_ACCOUNT,
      /*link_message_id=*/
      IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SYNCED_TO_ACCOUNT,
      controller_.GetPrimaryAccountEmail(), open_password_manager_closure);
}

void PasswordAddUsernameView::OnUsernameChanged() {
  SetButtonEnabled(ui::mojom::DialogButton::kOk,
                   !username_dropdown_->GetText().empty());
}

BEGIN_METADATA(PasswordAddUsernameView)
END_METADATA
