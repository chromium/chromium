// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAVE_TO_DRIVE_GET_ACCOUNT_H_
#define CHROME_BROWSER_UI_SAVE_TO_DRIVE_GET_ACCOUNT_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "components/signin/public/identity_manager/account_info.h"

namespace content {
class WebContents;
}  // namespace content

namespace save_to_drive {

void LaunchAccountChooserAndGetAccount(
    content::WebContents* web_contents,
    base::OnceCallback<void(std::optional<AccountInfo>)>
        on_account_selected_callback);

}  // namespace save_to_drive

#endif  // CHROME_BROWSER_UI_SAVE_TO_DRIVE_GET_ACCOUNT_H_
