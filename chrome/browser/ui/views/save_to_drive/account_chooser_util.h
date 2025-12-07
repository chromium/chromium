// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SAVE_TO_DRIVE_ACCOUNT_CHOOSER_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_SAVE_TO_DRIVE_ACCOUNT_CHOOSER_UTIL_H_

#include "components/signin/public/identity_manager/account_info.h"
#include "ui/views/view.h"

namespace save_to_drive {
// The size of a user's avatar.
inline constexpr int32_t kAvatarSize = 40;
// The width of the modal dialog.
inline constexpr int32_t kDialogWidth = 448;
// The maximum height of the space used to display available accounts, in terms
// of the display height of each account. Making this fractional means that if
// there are more than this number, the last visible account will be only
// partially visible, helping to indicate that the pane can be scrolled to see
// additional values.
inline constexpr float kMaxAccountsToShow = 7.5f;

// Creates a view for a single account, including the avatar, display name, and
// email.
std::unique_ptr<views::View> CreateAccountRow(const AccountInfo& account);

}  // namespace save_to_drive

#endif  // CHROME_BROWSER_UI_VIEWS_SAVE_TO_DRIVE_ACCOUNT_CHOOSER_UTIL_H_
