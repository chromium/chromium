// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_DELETE_ADDRESS_PROFILE_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_AUTOFILL_DELETE_ADDRESS_PROFILE_DIALOG_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/delete_address_profile_dialog_controller.h"

namespace views {
class Widget;
}  // namespace views

namespace content {
class WebContents;
}  // namespace content

namespace autofill::dialogs {

// Shows a modal dialog prompting to user whether they want to delete their
// profile address.
views::Widget* ShowDeleteAddressProfileDialogView(
    content::WebContents* web_contents,
    base::WeakPtr<DeleteAddressProfileDialogController> controller);

}  // namespace autofill::dialogs

#endif  // CHROME_BROWSER_UI_AUTOFILL_DELETE_ADDRESS_PROFILE_DIALOG_VIEW_H_
