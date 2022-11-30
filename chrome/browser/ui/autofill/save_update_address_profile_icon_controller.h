// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_ICON_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_ICON_CONTROLLER_H_

#include "content/public/browser/web_contents.h"

namespace autofill {

class AutofillBubbleBase;

// The controller for SaveUpdateAddressProfileIconView.
class SaveUpdateAddressProfileIconController {
 public:
  virtual ~SaveUpdateAddressProfileIconController() = default;

  // Returns a reference to the SaveUpdateAddressProfileIconController
  // associated with the given |web_contents|. If controller does not exist,
  // this will return nullptr.
  static SaveUpdateAddressProfileIconController* Get(
      content::WebContents* web_contents);

  virtual void OnPageActionIconClicked() = 0;

  virtual bool IsBubbleActive() const = 0;

  virtual std::u16string GetPageActionIconTootip() const = 0;

  virtual AutofillBubbleBase* GetBubbleView() const = 0;

  // Whether the icon belongs to a save or an update address bubble.
  virtual bool IsSaveBubble() const = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_ICON_CONTROLLER_H_
