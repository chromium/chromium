// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_VIEW_H_

namespace content {
class WebContents;
}

namespace autofill {

class CardUnmaskAuthenticationSelectionDialogController;

// Interface that exposes the view to
// CardUnmaskAuthenticationSelectionDialogControllerImpl.
class CardUnmaskAuthenticationSelectionDialogView {
 public:
  // Creates a dialog and displays it as a modal on top of the web contents of
  // CardUnmaskAuthenticationSelectionDialogController.
  static CardUnmaskAuthenticationSelectionDialogView* CreateAndShow(
      CardUnmaskAuthenticationSelectionDialogController* controller,
      content::WebContents* web_contents);

  // Method to safely close this dialog when the controller is destroyed by
  // unlinking the controller and closing the widget that owns this dialog view.
  virtual void OnControllerDestroying() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_VIEW_H_
