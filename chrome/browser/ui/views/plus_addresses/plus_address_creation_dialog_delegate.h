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
class Label;
class MdTextButton;
class StyledLabel;
}  // namespace views

namespace plus_addresses {

class PlusAddressCreationController;

//  A delegate that creates and updates the PlusAddresses dialog.
class PlusAddressCreationDialogDelegate : public views::BubbleDialogDelegate,
                                          public PlusAddressCreationView {
 public:
  PlusAddressCreationDialogDelegate(
      base::WeakPtr<PlusAddressCreationController> controller,
      content::WebContents* web_contents,
      const std::string& primary_email_address);
  PlusAddressCreationDialogDelegate(const PlusAddressCreationDialogDelegate&) =
      delete;
  PlusAddressCreationDialogDelegate& operator=(
      const PlusAddressCreationDialogDelegate&) = delete;
  ~PlusAddressCreationDialogDelegate() override;

  // WidgetDelegate:
  bool ShouldShowCloseButton() const override;
  void OnWidgetInitialized() override;

  // PlusAddressCreationView:
  void ShowReserveResult(const PlusProfileOrError& maybe_plus_profile) override;
  void ShowConfirmResult(const PlusProfileOrError& maybe_plus_profile) override;
  void OpenSettingsLink(content::WebContents* web_contents) override;
  void OpenErrorReportLink(content::WebContents* web_contents) override;

  // Calls the respective controller method for `type`.
  void HandleButtonPress(PlusAddressViewButtonType type);

 private:
  // Set the modal dialog to show error messages.
  void ShowErrorStateUI();

  base::WeakPtr<PlusAddressCreationController> controller_;
  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<views::Label> plus_address_label_ = nullptr;
  raw_ptr<views::StyledLabel> error_report_label_ = nullptr;
  raw_ptr<views::MdTextButton> confirm_button_ = nullptr;
  raw_ptr<views::MdTextButton> cancel_button_ = nullptr;
};

}  // namespace plus_addresses

#endif  // CHROME_BROWSER_UI_VIEWS_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_DIALOG_DELEGATE_H_
