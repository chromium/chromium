// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_EDIT_ADDRESS_PROFILE_VIEW_H_
#define CHROME_BROWSER_UI_AUTOFILL_EDIT_ADDRESS_PROFILE_VIEW_H_

#include "chrome/browser/ui/autofill/edit_address_profile_dialog_controller.h"

namespace content {
class WebContents;
}  // namespace content

namespace autofill {
class AutofillBubbleBase;

// Shows a modal dialog for editing an Autofill profile.
AutofillBubbleBase* ShowEditAddressProfileDialogView(
    content::WebContents* web_contents,
    EditAddressProfileDialogController* controller);

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_EDIT_ADDRESS_PROFILE_VIEW_H_
