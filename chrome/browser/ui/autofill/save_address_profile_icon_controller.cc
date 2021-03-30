// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/save_address_profile_icon_controller.h"

#include "chrome/browser/ui/autofill/save_address_profile_bubble_controller_impl.h"

namespace autofill {

// static
SaveAddressProfileIconController* SaveAddressProfileIconController::Get(
    content::WebContents* web_contents) {
  if (!web_contents)
    return nullptr;

  return SaveAddressProfileBubbleControllerImpl::FromWebContents(web_contents);
}

}  // namespace autofill
