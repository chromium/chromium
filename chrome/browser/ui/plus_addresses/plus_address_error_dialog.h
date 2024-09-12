// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PLUS_ADDRESSES_PLUS_ADDRESS_ERROR_DIALOG_H_
#define CHROME_BROWSER_UI_PLUS_ADDRESSES_PLUS_ADDRESS_ERROR_DIALOG_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "ui/base/interaction/element_identifier.h"

namespace content {
class WebContents;
}  // namespace content

namespace plus_addresses {

DECLARE_ELEMENT_IDENTIFIER_VALUE(kPlusAddressErrorDialogAcceptButton);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kPlusAddressErrorDialogCancelButton);

// Shows a tab-modal error dialog that explains that there already exists an
// `affiliated_plus_address` for an `affiliated_domain` and offers filling that.
// Runs `on_accepted` if the user chooses to do so.
void ShowInlineCreationAffiliationErrorDialog(
    content::WebContents* web_contents,
    std::u16string affiliated_domain,
    std::u16string affiliated_plus_address,
    base::OnceClosure on_accepted);

// Shows a tab-modal error dialog and runs `on_accepted` if the user accepts it.
using PlusAddressErrorDialogType =
    autofill::AutofillClient::PlusAddressErrorDialogType;
void ShowInlineCreationErrorDialog(content::WebContents* web_contents,
                                   PlusAddressErrorDialogType error_dialog_type,
                                   base::OnceClosure on_accepted);

}  // namespace plus_addresses

#endif  // CHROME_BROWSER_UI_PLUS_ADDRESSES_PLUS_ADDRESS_ERROR_DIALOG_H_
