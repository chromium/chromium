// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_DIALOG_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/plus_addresses/plus_address_creation_controller.h"

namespace content {
class WebContents;
}  // namespace content

namespace plus_addresses {

// Shows a modal dialog that, if confirmed, creates and fills a plus address.
void ShowPlusAddressCreationDialogView(
    content::WebContents* web_contents,
    base::WeakPtr<PlusAddressCreationController> controller,
    const std::string& primary_email_address);

}  // namespace plus_addresses

#endif  // CHROME_BROWSER_UI_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_DIALOG_VIEW_H_
