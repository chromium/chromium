// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/save_to_drive/get_account.h"

#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/web_contents.h"

namespace save_to_drive {

void LaunchAccountChooserAndGetAccount(
    content::WebContents* web_contents,
    base::OnceCallback<void(std::optional<AccountInfo>)>
        on_account_selected_callback) {
  // TODO(crbug.com/440292537): Implement.
}

}  // namespace save_to_drive
