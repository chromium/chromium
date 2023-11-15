// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_DIALOG_DELEGATE_H_

#include "base/memory/weak_ptr.h"
#include "components/plus_addresses/plus_address_types.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/md_text_button.h"

namespace views {
class Label;
}  // namespace views

namespace plus_addresses {

class PlusAddressCreationController;

//  A delegate that creates and updates the PlusAddresses dialog.
class PlusAddressCreationDialogDelegate : public views::BubbleDialogDelegate {
 public:
  enum ButtonType { kCancel = 0, kClose = 1, kConfirm = 2 };

  PlusAddressCreationDialogDelegate(
      base::WeakPtr<PlusAddressCreationController> controller,
      const std::string& primary_email_address);
  PlusAddressCreationDialogDelegate(const PlusAddressCreationDialogDelegate&) =
      delete;
  PlusAddressCreationDialogDelegate& operator=(
      const PlusAddressCreationDialogDelegate&) = delete;
  ~PlusAddressCreationDialogDelegate() override;

  // WidgetDelegate:
  bool ShouldShowCloseButton() const override;
  void OnWidgetInitialized() override;

  // TODO(crbug.com/1467623): Pull these into a shared delegate interface for
  // both Desktop & Android views.
  // Updates the dialog to either show an error message or show the
  // plus address in the dialog and enable the OK button.
  void ShowReserveResult(const PlusProfileOrError& maybe_plus_profile);
  // Either shows an error message on the dialog or closes the dialog.
  void ShowConfirmResult(const PlusProfileOrError& maybe_plus_profile);

  // Calls the respective controller method for `type`.
  void HandleButtonPress(ButtonType type);

 private:
  base::WeakPtr<PlusAddressCreationController> controller_;
  raw_ptr<views::Label> plus_address_label_ = nullptr;
  raw_ptr<views::MdTextButton> confirm_button_ = nullptr;
};

}  // namespace plus_addresses

#endif  // CHROME_BROWSER_UI_VIEWS_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_DIALOG_DELEGATE_H_
