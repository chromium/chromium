// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/save_update_address_profile_icon_controller.h"

#include "chrome/browser/ui/autofill/address_bubbles_controller.h"

namespace autofill {

// static
SaveUpdateAddressProfileIconController*
SaveUpdateAddressProfileIconController::Get(
    content::WebContents* web_contents) {
  if (!web_contents)
    return nullptr;

  return AddressBubblesController::FromWebContents(web_contents);
}

}  // namespace autofill
