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

  // Navigates to the link shown in the dialog's description.
  void OpenSettingsLink(content::WebContents* web_contents);

  // PlusAddressCreationView:
  void ShowReserveResult(const PlusProfileOrError& maybe_plus_profile) override;
  void ShowConfirmResult(const PlusProfileOrError& maybe_plus_profile) override;
  bool GetConfirmButtonEnabledForTesting() const override;
  void ClickButtonForTesting(PlusAddressViewButtonType type) override;
  std::u16string GetPlusAddressLabelTextForTesting() const override;
  bool ShowsLoadingIndicatorForTesting() const override;
  void WaitUntilResultShownForTesting() override;

  // Calls the respective controller method for `type`.
  void HandleButtonPress(PlusAddressViewButtonType type);

 private:
  // Blocks iff `WaitUntilResultShownForTesting()` has been called beforehand.
  void MaybeBlockUntilResultShows();

  base::WeakPtr<PlusAddressCreationController> controller_;
  raw_ptr<views::Label> plus_address_label_ = nullptr;
  raw_ptr<views::MdTextButton> confirm_button_ = nullptr;
  raw_ptr<views::MdTextButton> cancel_button_ = nullptr;
  // Stores a RunLoop::QuitClosure(). Only set in tests.
  absl::optional<base::OnceClosure> blocking_until_result_shown_ =
      absl::nullopt;
};

}  // namespace plus_addresses

#endif  // CHROME_BROWSER_UI_VIEWS_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_DIALOG_DELEGATE_H_
