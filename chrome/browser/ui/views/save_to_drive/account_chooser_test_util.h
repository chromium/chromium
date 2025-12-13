// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SAVE_TO_DRIVE_ACCOUNT_CHOOSER_TEST_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_SAVE_TO_DRIVE_ACCOUNT_CHOOSER_TEST_UTIL_H_

#include <cstdint>
#include <string>

#include "components/signin/public/identity_manager/account_info.h"
#include "ui/views/view.h"

namespace save_to_drive::testing {

AccountInfo GetTestAccount(const std::string& name,
                           const std::string& domain,
                           int32_t gaia_id);
std::vector<AccountInfo> GetTestAccounts(const std::vector<std::string>& names,
                                         const std::string& domain);
// Verifies that the account row contains an avatar, name, and email. Returns
// false if the row does not contain the expected elements.
bool VerifyAccountChooserRow(views::View* row_view, const AccountInfo& account);

}  // namespace save_to_drive::testing

#endif  // CHROME_BROWSER_UI_VIEWS_SAVE_TO_DRIVE_ACCOUNT_CHOOSER_TEST_UTIL_H_
