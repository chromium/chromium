// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_UI_UTIL_H_
#define CHROME_BROWSER_UI_AUTOFILL_UI_UTIL_H_

#include "components/signin/public/identity_manager/account_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class BrowserContext;
}

namespace autofill {

// Retrieves user's primary account from BrowserContext, traversing a chain
// of dependencies up to signin::IdentityManager.
absl::optional<AccountInfo> GetPrimaryAccountInfoFromBrowserContext(
    content::BrowserContext* context);

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_UI_UTIL_H_
