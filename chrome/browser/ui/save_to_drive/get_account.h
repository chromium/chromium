// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAVE_TO_DRIVE_GET_ACCOUNT_H_
#define CHROME_BROWSER_UI_SAVE_TO_DRIVE_GET_ACCOUNT_H_

#include <optional>

#include "base/functional/callback_forward.h"

namespace content {
class WebContents;
}  // namespace content

struct AccountInfo;

namespace save_to_drive {

class AccountChooser {
 public:
  AccountChooser() = default;
  AccountChooser(const AccountChooser&) = delete;
  AccountChooser& operator=(const AccountChooser&) = delete;
  virtual ~AccountChooser() = default;

  // Launches AccountChooser flow and calls `on_account_chosen_callback` once an
  // account has been chosen. If the account chooser is canceled, the callback
  // will be called with a `std::nullopt`.
  virtual void GetAccount(content::WebContents* web_contents,
                          base::OnceCallback<void(std::optional<AccountInfo>)>
                              on_account_chosen_callback);
};

}  // namespace save_to_drive

#endif  // CHROME_BROWSER_UI_SAVE_TO_DRIVE_GET_ACCOUNT_H_
