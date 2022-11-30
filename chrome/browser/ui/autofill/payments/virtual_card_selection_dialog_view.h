// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VIRTUAL_CARD_SELECTION_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VIRTUAL_CARD_SELECTION_DIALOG_VIEW_H_

namespace content {
class WebContents;
}

namespace autofill {

class VirtualCardSelectionDialogController;

// The view of the dialog that offers the all the available credit cards that
// can be used as virtual card. Shown when the option of using a virtual card is
// clicked in the Autofill popup bubble.
class VirtualCardSelectionDialogView {
 public:
  static VirtualCardSelectionDialogView* CreateAndShow(
      VirtualCardSelectionDialogController* controller,
      content::WebContents* web_content);

  virtual void Hide() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VIRTUAL_CARD_SELECTION_DIALOG_VIEW_H_
