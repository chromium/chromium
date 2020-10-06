// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_ADDRESS_PROFILES_SAVE_ADDRESS_PROFILE_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_ADDRESS_PROFILES_SAVE_ADDRESS_PROFILE_BUBBLE_CONTROLLER_H_

#include "base/strings/string16.h"

namespace autofill {

// The controller functionality for SaveAddressProfileView.
class SaveAddressProfileBubbleController {
 public:
  SaveAddressProfileBubbleController();
  SaveAddressProfileBubbleController(
      const SaveAddressProfileBubbleController&) = delete;
  SaveAddressProfileBubbleController& operator=(
      const SaveAddressProfileBubbleController&) = delete;
  virtual ~SaveAddressProfileBubbleController();

  base::string16 GetWindowTitle() const;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_ADDRESS_PROFILES_SAVE_ADDRESS_PROFILE_BUBBLE_CONTROLLER_H_
