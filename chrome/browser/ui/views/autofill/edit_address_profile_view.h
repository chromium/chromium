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
class SaveAddressProfileBubbleController;

class EditAddressProfileView : public AutofillBubbleBase,
                               public views::DialogDelegateView {
 public:
  EditAddressProfileView(content::WebContents* web_contents,
                         SaveAddressProfileBubbleController* controller);

  EditAddressProfileView(const EditAddressProfileView&) = delete;
  EditAddressProfileView& operator=(const EditAddressProfileView&) = delete;
  ~EditAddressProfileView() override;

  // AutofillBubbleBase:
  void Hide() override;

  // views::WidgetDelegate:
  std::u16string GetWindowTitle() const override;
  void WindowClosing() override;

 private:
  SaveAddressProfileBubbleController* controller_;
  std::unique_ptr<AddressEditorController> address_editor_controller_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_EDIT_ADDRESS_PROFILE_VIEW_H_
