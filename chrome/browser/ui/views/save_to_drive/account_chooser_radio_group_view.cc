// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/save_to_drive/account_chooser_radio_group_view.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/save_to_drive/account_chooser_util.h"
#include "chrome/browser/ui/views/save_to_drive/account_chooser_view_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/cascading_property.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace save_to_drive {

AccountChooserRadioButtonRow::AccountChooserRadioButtonRow(
    AccountChooserRadioButtonDelegate* delegate,
    const AccountInfo& account)
    : delegate_(delegate), account_(account) {
  SetOrientation(views::LayoutOrientation::kHorizontal);
  SetIgnoreDefaultMainAxisMargins(true);
  SetInteriorMargin(gfx::Insets::VH(
      /*vertical=*/ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_EXTENSIONS_MENU_BUTTON_MARGIN),
      /*horizontal=*/0));
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  GetViewAccessibility().SetRole(ax::mojom::Role::kMenuItemRadio);
  GetViewAccessibility().SetName(
      base::StrCat({account.full_name, " ", account.email}));

  auto account_row_container = std::make_unique<views::FlexLayoutView>();
  auto account_row = CreateAccountRow(account);
  account_row_container->AddChildView(std::move(account_row));
  // Ensure the account row is left-aligned. kHorizontal and kUnbounded are
  // needed to ensure the account row expands as much as possible in the
  // horizontal direction.
  account_row_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded));
  AddChildView(std::move(account_row_container));

  auto radio_button = std::make_unique<views::RadioButton>();
  radio_button->GetViewAccessibility().SetName(
      base::UTF8ToUTF16(account.email));
  account_selected_subscription_ = radio_button->AddCheckedChangedCallback(
      base::BindRepeating(&AccountChooserRadioButtonRow::OnAccountSelected,
                          base::Unretained(this)));
  radio_button_ = AddChildView(std::move(radio_button));
}
AccountChooserRadioButtonRow::~AccountChooserRadioButtonRow() = default;

bool AccountChooserRadioButtonRow::HandleAccessibleAction(
    const ui::AXActionData& action_data) {
  return radio_button_->HandleAccessibleAction(action_data);
}

bool AccountChooserRadioButtonRow::OnMousePressed(const ui::MouseEvent& event) {
  if (event.IsLeftMouseButton()) {
    // OnAccountSelected runs as a result of selecting the radio button.
    SelectRadioButton();
  }
  // We do not need to listen for OnMouseDragged, OnMouseReleased, etc.
  return false;
}

void AccountChooserRadioButtonRow::OnAccountSelected() {
  if (radio_button_->GetChecked()) {
    delegate_->SelectAccount(account_);
  }
}

void AccountChooserRadioButtonRow::SelectRadioButton() {
  radio_button_->SetChecked(true);
}

BEGIN_METADATA(AccountChooserRadioButtonRow)
END_METADATA

AccountChooserRadioGroupView::AccountChooserRadioGroupView(
    AccountChooserViewDelegate& parent_dialog,
    const std::vector<AccountInfo>& accounts,
    std::optional<CoreAccountId> primary_account_id)
    : parent_dialog_(parent_dialog) {
  CHECK(accounts.size() > 1) << "Account chooser radio group view should only "
                                "be used for multi-account cases.";
  SetCascadingRadioGroupView(this, views::kCascadingRadioGroupView);
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  GetViewAccessibility().SetRole(ax::mojom::Role::kRadioGroup);
  GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
      IDS_ACCOUNT_CHOOSER_RADIO_GROUP_ACCESSIBILITY_LABEL));

  // Order the accounts by display name.
  for (const auto& account : accounts) {
    accounts_[account] = nullptr;
  }

  SetOrientation(views::LayoutOrientation::kVertical);

  // Build the account rows.
  AddChildView(std::make_unique<views::Separator>());
  for (auto& account : accounts_) {
    // Keep track of the account to radio button mapping.
    if (account.first.account_id == primary_account_id) {
      // Ensures the primary account is the first account in the list.
      account.second = AddChildViewAt(
          std::make_unique<AccountChooserRadioButtonRow>(this, account.first),
          1);
      AddChildViewAt(std::make_unique<views::Separator>(), 2);
      // Select the primary account.
      account.second->SelectRadioButton();
    } else {
      account.second = AddChildView(
          std::make_unique<AccountChooserRadioButtonRow>(this, account.first));
      AddChildView(std::make_unique<views::Separator>());
    }
  }
  if (!primary_account_id) {
    // Select the first account if no primary account is provided.
    accounts_.begin()->second->SelectRadioButton();
  }
}

AccountChooserRadioGroupView::~AccountChooserRadioGroupView() = default;

void AccountChooserRadioGroupView::SelectAccount(const AccountInfo& account) {
  auto it = accounts_.find(account);
  CHECK(it != accounts_.end());
  parent_dialog_->OnAccountSelected(account);
}

BEGIN_METADATA(AccountChooserRadioGroupView)
END_METADATA

}  // namespace save_to_drive
