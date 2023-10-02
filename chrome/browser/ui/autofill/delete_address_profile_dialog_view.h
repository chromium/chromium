// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_DELETE_ADDRESS_PROFILE_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_AUTOFILL_DELETE_ADDRESS_PROFILE_DIALOG_VIEW_H_

#include "base/memory/weak_ptr.h"

namespace content {
class WebContents;
}  // namespace content

namespace autofill {
class DeleteAddressProfileDialogController;

// Shows a modal dialog prompting to user whether they want to delete their
// profile address.
void ShowDeleteAddressProfileDialogView(
    content::WebContents* web_contents,
    base::WeakPtr<DeleteAddressProfileDialogController> controller);

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_DELETE_ADDRESS_PROFILE_DIALOG_VIEW_H_
