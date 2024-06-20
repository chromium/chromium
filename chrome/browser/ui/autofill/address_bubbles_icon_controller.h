// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_ADDRESS_BUBBLES_ICON_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_ADDRESS_BUBBLES_ICON_CONTROLLER_H_

#include "content/public/browser/web_contents.h"

namespace autofill {

class AutofillBubbleBase;

// The controller for AddressBubblesIconView.
class AddressBubblesIconController {
 public:
  virtual ~AddressBubblesIconController() = default;

  // Returns a reference to the AddressBubblesIconController associated with
  // the given |web_contents|. If controller does not exist, this will return
  // nullptr.
  static AddressBubblesIconController* Get(content::WebContents* web_contents);

  virtual void OnIconClicked() = 0;

  virtual bool IsBubbleActive() const = 0;

  virtual std::u16string GetPageActionIconTootip() const = 0;

  virtual AutofillBubbleBase* GetBubbleView() const = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_ADDRESS_BUBBLES_ICON_CONTROLLER_H_
