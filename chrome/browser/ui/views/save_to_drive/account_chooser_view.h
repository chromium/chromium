// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SAVE_TO_DRIVE_ACCOUNT_CHOOSER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SAVE_TO_DRIVE_ACCOUNT_CHOOSER_VIEW_H_

#include "chrome/browser/ui/views/save_to_drive/account_chooser_view_delegate.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view.h"

namespace save_to_drive {

class AccountChooserView : public views::FlexLayoutView {
  METADATA_HEADER(AccountChooserView, views::FlexLayoutView)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kTopViewId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kAddAccountButtonId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCancelButtonId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kSaveButtonId);

  AccountChooserView(AccountChooserViewDelegate* parent_dialog,
                              const std::vector<AccountInfo>& accounts,
                              std::optional<CoreAccountId> primary_account_id);
  ~AccountChooserView() override;
  // Updates the view with the new accounts and primary account id.
  void UpdateView(const std::vector<AccountInfo>& accounts,
                  std::optional<CoreAccountId> primary_account_id);

 private:
  // Creates the view containing the account rows based on the number of
  // accounts.
  std::unique_ptr<views::View> CreateBodyMultiAccount(
      const std::vector<AccountInfo>& accounts,
      std::optional<CoreAccountId> primary_account_id);
  // Creates the view containing the account rows based on the number of
  // accounts.
  std::unique_ptr<views::View> CreateBodySingleAccount(
      const AccountInfo& account);
  // Creates the view containing the account rows based on the number of
  // accounts.
  std::unique_ptr<views::View> CreateBodyView(
      const std::vector<AccountInfo>& accounts,
      std::optional<CoreAccountId> primary_account_id);
  std::unique_ptr<views::View> CreateDriveLogoView();
  // Creates the view containing the "Use a different account", "Cancel", and
  // "Save" buttons.
  std::unique_ptr<views::View> CreateFooterView();
  // Creates the view containing the title, subtitle, and Drive logo.
  std::unique_ptr<views::View> CreateHeaderView(
      const std::vector<AccountInfo>& accounts);
  std::unique_ptr<views::Label> CreateTitleLabel(
      const std::vector<AccountInfo>& accounts);
  std::unique_ptr<views::StyledLabel> CreateSubtitleLabel();
  std::unique_ptr<views::View> CreateTitleView(
      const std::vector<AccountInfo>& accounts);
  // Gets the title of the account chooser dialog based on the number of
  // accounts.
  std::u16string GetTitle(const std::vector<AccountInfo>& accounts);
  bool IsMultiAccount(const std::vector<AccountInfo>& accounts);
  bool IsSingleAccount(const std::vector<AccountInfo>& accounts);
  void SetLabelProperties(views::Label* label);

  // Updates the body view with the new accounts and primary account id.
  void UpdateBodyView(const std::vector<AccountInfo>& accounts,
                      std::optional<CoreAccountId> primary_account_id);
  // Updates the header view with the new accounts.
  void UpdateHeaderView(const std::vector<AccountInfo>& accounts);

  raw_ptr<AccountChooserViewDelegate> parent_dialog_ = nullptr;

  // View containing the logo of the identity provider and the title.
  raw_ptr<views::View> header_view_ = nullptr;

  raw_ptr<views::View> body_view_ = nullptr;

  raw_ptr<views::View> footer_view_ = nullptr;
};
}  // namespace save_to_drive

#endif  // CHROME_BROWSER_UI_VIEWS_SAVE_TO_DRIVE_ACCOUNT_CHOOSER_VIEW_H_
