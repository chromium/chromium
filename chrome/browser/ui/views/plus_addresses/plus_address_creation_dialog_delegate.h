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
class MdTextButton;
class ProgressBar;
class View;
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
      bool show_notice);
  PlusAddressCreationDialogDelegate(const PlusAddressCreationDialogDelegate&) =
      delete;
  PlusAddressCreationDialogDelegate& operator=(
      const PlusAddressCreationDialogDelegate&) = delete;
  ~PlusAddressCreationDialogDelegate() override;

  // WidgetDelegate:
  void OnWidgetInitialized() override;

  // PlusAddressCreationView:
  void ShowReserveResult(const PlusProfileOrError& maybe_plus_profile,
                         bool offer_refresh) override;
  void ShowConfirmResult(const PlusProfileOrError& maybe_plus_profile) override;

  // Calls the respective controller method for `type`.
  void HandleButtonPress(PlusAddressViewButtonType type);

 private:
  // Helper methods for view setup.

  // Creates the logo displayed at the top of the dialog.
  std::unique_ptr<views::View> CreateLogo();

  // Creates a hidden refresh button.
  std::unique_ptr<views::ImageButton> CreateRefreshButton();

  // Creates a view containing the two buttons for the dialog and saves a
  // pointer to the confirm button to `confirm_button_`.
  std::unique_ptr<views::View> CreateButtons();

  // Shows and hides the progress bar at the top of the dialog.
  void SetProgressBarVisibility(bool is_visible);

  // Updates the modal dialog to show error messages.
  // TODO(crbug.com/363720961): Remove once updated error states are launched -
  // it will then have been superseded by the methods below.
  void ShowErrorStateUI();

  // Updates the icon in the suggested address box to an error icon and shows an
  // error message below the suggested plus address container.
  void ShowCreateErrorMessage(bool is_timeout);

  void HideCreateErrorMessage();

  // Updates `plus_address_label_` to indicate that a new address is loading,
  // disables `confirm_button_` and informs the controller.
  void OnRefreshClicked();

  base::WeakPtr<PlusAddressCreationController> controller_;
  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<views::ProgressBar> progress_bar_ = nullptr;

  // The container for the suggested plus address and the refresh button.
  class PlusAddressContainerView;
  raw_ptr<PlusAddressContainerView> plus_address_container_ = nullptr;

  // A single line error message in red.
  raw_ptr<views::Label> create_error_message_label_ = nullptr;

  // Label with link for error reporting.
  // TODO(crbug.com/363720961): Remove once updated error states are launched.
  raw_ptr<views::View> error_report_label_ = nullptr;

  // The button for confirming the modal dialog.
  raw_ptr<views::MdTextButton> confirm_button_ = nullptr;
};

}  // namespace plus_addresses

#endif  // CHROME_BROWSER_UI_VIEWS_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_DIALOG_DELEGATE_H_
