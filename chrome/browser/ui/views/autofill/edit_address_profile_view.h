// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_EDIT_ADDRESS_PROFILE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_EDIT_ADDRESS_PROFILE_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContents;
}

namespace autofill {
class EditAddressProfileDialogController;
class AddressEditorView;

// This is the dialog used to edit an address profile it. It's part of the
// flow triggered upon submitting a form with an address profile that is not
// already saved. This dialog is opened when the user decides to edit the
// address before saving it.
class EditAddressProfileView : public AutofillBubbleBase,
                               public views::DialogDelegateView {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kTopViewId);

  explicit EditAddressProfileView(
      EditAddressProfileDialogController* controller);

  EditAddressProfileView(const EditAddressProfileView&) = delete;
  EditAddressProfileView& operator=(const EditAddressProfileView&) = delete;
  ~EditAddressProfileView() override;

  void ShowForWebContents(content::WebContents* web_contents);

  // AutofillBubbleBase:
  void Hide() override;

  // views::DialogDelegateView
  View* GetInitiallyFocusedView() override;
  void WindowClosing() override;
  void ChildPreferredSizeChanged(views::View* child) override;

  AddressEditorView* GetAddressEditorViewForTesting();

 private:
  void OnUserDecision(AutofillClient::AddressPromptUserDecision decision);
  void UpdateActionButtonState(bool is_valid);
  bool OnAcceptButtonClicked();

  raw_ptr<EditAddressProfileDialogController> controller_;
  raw_ptr<AddressEditorView> address_editor_view_ = nullptr;
  base::CallbackListSubscription on_is_valid_change_subscription_;
  AutofillClient::AddressPromptUserDecision decision_ =
      AutofillClient::AddressPromptUserDecision::kIgnored;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_EDIT_ADDRESS_PROFILE_VIEW_H_
