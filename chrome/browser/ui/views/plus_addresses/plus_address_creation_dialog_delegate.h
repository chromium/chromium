// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_DIALOG_DELEGATE_H_

#include "base/memory/weak_ptr.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace views {
class Label;
}  // namespace views

namespace plus_addresses {

class PlusAddressCreationController;

//  A delegate that creates and updates the PlusAddresses dialog.
class PlusAddressCreationDialogDelegate : public views::BubbleDialogDelegate {
 public:
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

  // Updates the modal to show `plus_address` and enables the OK button for use.
  void OnModalReadyForUse(const std::string& plus_address);

  // Displays an error message indicating that something went wrong.
  void OnRequestError();

 private:
  raw_ptr<views::Label> plus_address_label_ = nullptr;
};

}  // namespace plus_addresses

#endif  // CHROME_BROWSER_UI_VIEWS_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_DIALOG_DELEGATE_H_
