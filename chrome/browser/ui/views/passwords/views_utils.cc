// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/views_utils.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/password_form.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_combobox_model.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/editable_combobox/editable_combobox.h"
#include "ui/views/controls/editable_combobox/editable_password_combobox.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/flex_layout.h"

namespace {
// Create a vector which contains only the values in |items| and no elements.
std::vector<std::u16string> ToValues(
    const password_manager::AlternativeElementVector& items) {
  std::vector<std::u16string> passwords;
  passwords.reserve(items.size());
  for (const auto& item : items) {
    passwords.push_back(item.value);
  }
  return passwords;
}

std::unique_ptr<views::View> CreateRow() {
  auto row = std::make_unique<views::View>();
  views::FlexLayout* row_layout =
      row->SetLayoutManager(std::make_unique<views::FlexLayout>());
  row_layout->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetIgnoreDefaultMainAxisMargins(true)
      .SetCollapseMargins(true)
      .SetDefault(
          views::kMarginsKey,
          gfx::Insets::VH(0, ChromeLayoutProvider::Get()->GetDistanceMetric(
                                 views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));
  return row;
}
}  // namespace

std::unique_ptr<views::StyledLabel> CreateGooglePasswordManagerLabel(
    int text_message_id,
    int link_message_id,
    const std::u16string& email,
    base::RepeatingClosure open_link_closure,
    int context) {
  const std::u16string link = l10n_util::GetStringUTF16(link_message_id);

  std::vector<size_t> offsets;
  std::u16string text =
      l10n_util::GetStringFUTF16(text_message_id, link, email, &offsets);

  auto label = std::make_unique<views::StyledLabel>();
  label->SetText(text);
  label->SetTextContext(context);
  label->SetDefaultTextStyle(views::style::STYLE_SECONDARY);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  label->AddStyleRange(
      gfx::Range(offsets.at(0), offsets.at(0) + link.length()),
      views::StyledLabel::RangeStyleInfo::CreateForLink(open_link_closure));

  return label;
}

std::unique_ptr<views::StyledLabel> CreateGooglePasswordManagerLabel(
    int text_message_id,
    int link_message_id,
    base::RepeatingClosure open_link_closure,
    int context) {
  const std::u16string link = l10n_util::GetStringUTF16(link_message_id);

  size_t link_offset;
  std::u16string text =
      l10n_util::GetStringFUTF16(text_message_id, link, &link_offset);

  auto label = std::make_unique<views::StyledLabel>();
  label->SetText(text);
  label->SetTextContext(context);
  label->SetDefaultTextStyle(views::style::STYLE_SECONDARY);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  label->AddStyleRange(
      gfx::Range(link_offset, link_offset + link.length()),
      views::StyledLabel::RangeStyleInfo::CreateForLink(open_link_closure));

  return label;
}

std::unique_ptr<views::Label> CreateUsernameLabel(
    const password_manager::PasswordForm& form) {
  auto label = std::make_unique<views::Label>(
      GetDisplayUsername(form), views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return label;
}

std::unique_ptr<views::Label> CreatePasswordLabel(
    const password_manager::PasswordForm& form) {
  std::unique_ptr<views::Label> label = std::make_unique<views::Label>(
      GetDisplayPassword(form), views::style::CONTEXT_DIALOG_BODY_TEXT);
  if (form.federation_origin.opaque()) {
    label->SetTextStyle(STYLE_SECONDARY_MONOSPACED);
    label->SetObscured(true);
    label->SetElideBehavior(gfx::TRUNCATE);
  } else {
    label->SetTextStyle(views::style::STYLE_SECONDARY);
    label->SetElideBehavior(gfx::ELIDE_HEAD);
  }
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return label;
}

int ComboboxIconSize() {
  // Use the line height of the body small text. This allows the icons to adapt
  // if the user changes the font size.
  return views::TypographyProvider::Get().GetLineHeight(
      views::style::CONTEXT_MENU, views::style::STYLE_PRIMARY);
}

// Builds a credential row, adds the given elements to the layout.
// |destination_field| is nullptr if the destination field shouldn't be shown.
void BuildCredentialRows(views::View* parent_view,
                         std::unique_ptr<views::View> destination_field,
                         std::unique_ptr<views::View> username_field,
                         std::unique_ptr<views::View> password_field) {
  std::unique_ptr<views::Label> username_label(new views::Label(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_USERNAME_LABEL),
      views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY));
  username_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  std::unique_ptr<views::Label> password_label(new views::Label(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_PASSWORD_LABEL),
      views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY));
  password_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  int labels_width = std::max({username_label->GetPreferredSize().width(),
                               password_label->GetPreferredSize().width()});
  int fields_height = std::max({username_field->GetPreferredSize().height(),
                                password_field->GetPreferredSize().height()});

  username_label->SetPreferredSize(gfx::Size(labels_width, fields_height));
  password_label->SetPreferredSize(gfx::Size(labels_width, fields_height));

  // Destination row.
  if (destination_field) {
    std::unique_ptr<views::View> destination_row = CreateRow();

    destination_field->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                 views::MaximumFlexSizeRule::kUnbounded));
    destination_row->AddChildView(std::move(destination_field));

    parent_view->AddChildView(std::move(destination_row));
  }

  // Username row.
  std::unique_ptr<views::View> username_row = CreateRow();
  username_row->AddChildView(std::move(username_label));
  username_field->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded));
  username_row->AddChildView(std::move(username_field));

  parent_view->AddChildView(std::move(username_row));

  // Password row.
  std::unique_ptr<views::View> password_row = CreateRow();
  password_row->AddChildView(std::move(password_label));
  password_field->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded));
  password_row->AddChildView(std::move(password_field));

  parent_view->AddChildView(std::move(password_row));
}

// Creates an EditableCombobox from |PasswordForm.all_alternative_usernames| or
// even just |PasswordForm.username_value|.
std::unique_ptr<views::EditableCombobox> CreateUsernameEditableCombobox(
    const password_manager::PasswordForm& form) {
  std::vector<std::u16string> usernames = {form.username_value};
  for (const password_manager::AlternativeElement& other_possible_username :
       form.all_alternative_usernames) {
    if (other_possible_username.value != form.username_value) {
      usernames.push_back(other_possible_username.value);
    }
  }
  base::EraseIf(usernames, [](const std::u16string& username) {
    return username.empty();
  });
  const bool kDisplayArrow = usernames.size() > 1;
  auto combobox = std::make_unique<views::EditableCombobox>(
      std::make_unique<ui::SimpleComboboxModel>(
          std::vector<ui::SimpleComboboxModel::Item>(usernames.begin(),
                                                     usernames.end())),
      /*filter_on_edit=*/false, /*show_on_empty=*/true,
      views::style::CONTEXT_BUTTON, views::style::STYLE_PRIMARY, kDisplayArrow);
  combobox->SetText(form.username_value);
  combobox->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_USERNAME_LABEL));
  // In case of long username, ensure that the beginning of value is visible.
  combobox->SelectRange(gfx::Range(0));
  return combobox;
}

// Creates an EditablePasswordCombobox from
// `PasswordForm.all_alternative_passwords` or even just
// `PasswordForm.password_value`.
std::unique_ptr<views::EditablePasswordCombobox> CreateEditablePasswordCombobox(
    const password_manager::PasswordForm& form,
    views::Button::PressedCallback reveal_password_callback) {
  DCHECK(!form.IsFederatedCredential());
  std::vector<std::u16string> passwords =
      form.all_alternative_passwords.empty()
          ? std::vector<std::u16string>(/*n=*/1, form.password_value)
          : ToValues(form.all_alternative_passwords);
  base::EraseIf(passwords, [](const std::u16string& password) {
    return password.empty();
  });
  const bool kDisplayArrow = passwords.size() > 1;
  auto combobox = std::make_unique<views::EditablePasswordCombobox>(
      std::make_unique<ui::SimpleComboboxModel>(
          std::vector<ui::SimpleComboboxModel::Item>(passwords.begin(),
                                                     passwords.end())),
      views::style::CONTEXT_BUTTON, STYLE_PRIMARY_MONOSPACED, kDisplayArrow,
      std::move(reveal_password_callback));
  combobox->SetText(form.password_value);
  combobox->SetPasswordIconTooltips(
      l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_SHOW_PASSWORD),
      l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_HIDE_PASSWORD));
  combobox->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_PASSWORD_LABEL));
  return combobox;
}

std::unique_ptr<views::Combobox> CreateDestinationCombobox(
    std::u16string primary_account_email,
    ui::ImageModel primary_account_avatar,
    bool is_using_account_store) {
  ui::ImageModel computer_image = ui::ImageModel::FromVectorIcon(
      kComputerWithCircleBackgroundIcon, ui::kColorIcon, ComboboxIconSize());

  ui::SimpleComboboxModel::Item account_destination(
      /*text=*/l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_DESTINATION_DROPDOWN_SAVE_TO_ACCOUNT),
      /*dropdown_secondary_text=*/primary_account_email,
      /*icon=*/primary_account_avatar);

  ui::SimpleComboboxModel::Item device_destination(
      /*text=*/l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_DESTINATION_DROPDOWN_SAVE_TO_DEVICE),
      /*dropdown_secondary_text=*/std::u16string(),
      /*icon=*/computer_image);

  auto combobox = std::make_unique<views::Combobox>(
      std::make_unique<ui::SimpleComboboxModel>(
          std::vector<ui::SimpleComboboxModel::Item>{
              std::move(account_destination), std::move(device_destination)}));
  if (is_using_account_store) {
    combobox->SetSelectedRow(0);
  } else {
    combobox->SetSelectedRow(1);
  }

  combobox->SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_DESTINATION_DROPDOWN_ACCESSIBLE_NAME));
  combobox->SetProperty(views::kElementIdentifierKey,
                        kSavePasswordComboboxElementId);
  return combobox;
}
