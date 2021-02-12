// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_SAVE_ADDRESS_PROFILE_ICON_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_SAVE_ADDRESS_PROFILE_ICON_CONTROLLER_H_

#include "content/public/browser/web_contents.h"

namespace autofill {

class AutofillBubbleBase;

// The controller for SaveAddressProfileIconView.
class SaveAddressProfileIconController {
 public:
  virtual ~SaveAddressProfileIconController() = default;

  // Returns a reference to the SaveAddressProfileIconController associated with
  // the given |web_contents|. If controller does not exist, this will return
  // nullptr.
  static SaveAddressProfileIconController* Get(
      content::WebContents* web_contents);

  virtual void OnPageActionIconClicked() = 0;

  virtual bool IsBubbleActive() const = 0;

  virtual AutofillBubbleBase* GetSaveBubbleView() const = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_SAVE_ADDRESS_PROFILE_ICON_CONTROLLER_H_
