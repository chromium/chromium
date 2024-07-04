// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_DIALOG_DELEGATE_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/plus_addresses/plus_address_creation_view.h"
#include "components/plus_addresses/plus_address_types.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace content {
class WebContents;
}  // namespace content

namespace views {
class ImageButton;
class Label;
class MdTextButton;
class StyledLabel;
class TableLayoutView;
}  // namespace views

namespace plus_addresses {

class PlusAddressCreationController;

// A delegate that creates and updates the PlusAddresses dialog.
class PlusAddressCreationDialogDelegate : public views::BubbleDialogDelegate,
                                          public PlusAddressCreationView {
 public:
  PlusAddressCreationDialogDelegate(
      base::WeakPtr<PlusAddressCreationController> controller,
      content::WebContents* web_contents,
      const std::string& primary_email_address,
      bool offer_refresh,
      bool show_notice);
  PlusAddressCreationDialogDelegate(const PlusAddressCreationDialogDelegate&) =
      delete;
  PlusAddressCreationDialogDelegate& operator=(
      const PlusAddressCreationDialogDelegate&) = delete;
  ~PlusAddressCreationDialogDelegate() override;

  // WidgetDelegate:
  void OnWidgetInitialized() override;

  // PlusAddressCreationView:
  void ShowReserveResult(const PlusProfileOrError& maybe_plus_profile) override;
  void ShowConfirmResult(const PlusProfileOrError& maybe_plus_profile) override;
  void HideRefreshButton() override;

  // Calls the respective controller method for `type`.
  void HandleButtonPress(PlusAddressViewButtonType type);

 private:
  // Updates the modal dialog to show error messages.
  void ShowErrorStateUI();

  // Updates `plus_address_label_` to indicate that a new address is loading,
  // disables `confirm_button_` and informs the controller.
  void OnRefreshClicked();

  base::WeakPtr<PlusAddressCreationController> controller_;
  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<views::TableLayoutView> plus_address_label_container_ = nullptr;
  raw_ptr<views::Label> plus_address_label_ = nullptr;
  raw_ptr<views::ImageButton> refresh_button_ = nullptr;
  raw_ptr<views::StyledLabel> error_report_label_ = nullptr;
  raw_ptr<views::MdTextButton> confirm_button_ = nullptr;
};

}  // namespace plus_addresses

#endif  // CHROME_BROWSER_UI_VIEWS_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_DIALOG_DELEGATE_H_
