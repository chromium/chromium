// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_EDIT_ADDRESS_PROFILE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_EDIT_ADDRESS_PROFILE_VIEW_H_

#include "chrome/browser/ui/autofill/address_editor_controller.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContents;
}

namespace autofill {
class EditAddressProfileDialogController;

// This is the dialog used to edit an address profile it. It's part of the
// flow triggered upon submitting a form with an address profile that is not
// already saved. This dialog is opened when the user decides to edit the
// address before saving it.
class EditAddressProfileView : public AutofillBubbleBase,
                               public views::DialogDelegateView {
 public:
  EditAddressProfileView(content::WebContents* web_contents,
                         EditAddressProfileDialogController* controller);

  EditAddressProfileView(const EditAddressProfileView&) = delete;
  EditAddressProfileView& operator=(const EditAddressProfileView&) = delete;
  ~EditAddressProfileView() override;

  // AutofillBubbleBase:
  void Hide() override;

  // views::WidgetDelegate:
  std::u16string GetWindowTitle() const override;
  void WindowClosing() override;

 private:
  EditAddressProfileDialogController* controller_;
  std::unique_ptr<AddressEditorController> address_editor_controller_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_EDIT_ADDRESS_PROFILE_VIEW_H_
