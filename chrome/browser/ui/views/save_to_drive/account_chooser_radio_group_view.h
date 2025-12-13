// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SAVE_TO_DRIVE_ACCOUNT_CHOOSER_RADIO_GROUP_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SAVE_TO_DRIVE_ACCOUNT_CHOOSER_RADIO_GROUP_VIEW_H_

#include <map>

#include "chrome/browser/ui/views/save_to_drive/account_chooser_view_delegate.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view.h"

namespace save_to_drive {

// Delegate for the radio button row. This is used to notify the radio group
// view of events.
class AccountChooserRadioButtonDelegate {
 public:
  virtual ~AccountChooserRadioButtonDelegate() = default;
  virtual void SelectAccount(const AccountInfo& account) = 0;
};

// View of a single account row within the multi-account account chooser radio
// group.
class AccountChooserRadioButtonRow : public views::FlexLayoutView {
  METADATA_HEADER(AccountChooserRadioButtonRow, views::FlexLayoutView)
 public:
  AccountChooserRadioButtonRow(AccountChooserRadioButtonDelegate* delegate,
                               const AccountInfo& account);
  AccountChooserRadioButtonRow(const AccountChooserRadioButtonRow&) = delete;
  AccountChooserRadioButtonRow& operator=(const AccountChooserRadioButtonRow&) =
      delete;
  ~AccountChooserRadioButtonRow() override;

  // View:
  bool HandleAccessibleAction(const ui::AXActionData& action_data) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;

  // Registers the account selection.
  void OnAccountSelected();
  // Selects the radio button.
  void SelectRadioButton();

 private:
  raw_ptr<AccountChooserRadioButtonDelegate> delegate_;
  const AccountInfo account_;
  raw_ptr<views::RadioButton> radio_button_;
  base::CallbackListSubscription account_selected_subscription_;
};

// View of the multi-account account chooser radio group.
class AccountChooserRadioGroupView : public views::BoxLayoutView,
                                     public AccountChooserRadioButtonDelegate {
  METADATA_HEADER(AccountChooserRadioGroupView, views::BoxLayoutView)
 public:
  // Precondition: accounts.size() > 1
  AccountChooserRadioGroupView(AccountChooserViewDelegate& parent_dialog,
                               const std::vector<AccountInfo>& accounts,
                               std::optional<CoreAccountId> primary_account_id);
  AccountChooserRadioGroupView(const AccountChooserRadioGroupView&) = delete;
  AccountChooserRadioGroupView& operator=(const AccountChooserRadioGroupView&) =
      delete;
  ~AccountChooserRadioGroupView() override;

  void SelectAccount(const AccountInfo& account) override;

 private:
  struct AccountInfoCmp {
    bool operator()(const AccountInfo& lhs, const AccountInfo& rhs) const {
      if (lhs.full_name == rhs.full_name) {
        return lhs.email < rhs.email;
      }
      return lhs.full_name < rhs.full_name;
    }
  };

  // Sorted by account display name.
  std::map<AccountInfo, AccountChooserRadioButtonRow*, AccountInfoCmp>
      accounts_;
  raw_ref<AccountChooserViewDelegate> parent_dialog_;  // can never be null
};
}  // namespace save_to_drive
#endif  // CHROME_BROWSER_UI_VIEWS_SAVE_TO_DRIVE_ACCOUNT_CHOOSER_RADIO_GROUP_VIEW_H_
