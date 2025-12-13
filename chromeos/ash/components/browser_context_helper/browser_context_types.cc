// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"

#include "base/files/file_path.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "content/public/browser/browser_context.h"

namespace ash {

const char kSigninBrowserContextBaseName[] = "Default";
const char kLockScreenBrowserContextBaseName[] = "LockScreenProfile";
const char kShimlessRmaAppBrowserContextBaseName[] = "ShimlessRmaAppProfile";

bool IsSigninBrowserContext(const content::BrowserContext* browser_context) {
  return browser_context && browser_context->GetPath().BaseName().value() ==
                                kSigninBrowserContextBaseName;
}

bool IsLockScreenBrowserContext(
    const content::BrowserContext* browser_context) {
  return browser_context && browser_context->GetPath().BaseName().value() ==
                                kLockScreenBrowserContextBaseName;
}

bool IsShimlessRmaAppBrowserContext(
    const content::BrowserContext* browser_context) {
  return browser_context && browser_context->GetPath().BaseName().value() ==
                                kShimlessRmaAppBrowserContextBaseName;
}

bool IsUserBrowserContext(const content::BrowserContext* browser_context) {
  // Check `AnnotatedAccountId` as an optimization to avoid creating/destroying
  // `base::FilePath`, which is cpu intensive. See b:402192521 for more details.
  //
  // `IsUserBrowserContextBaseName` is still needed so that profile creation
  // hook can identify user profiles and call `AnnotatedAccountId::Set` on them.
  // TODO(b:402192521): Get rid of `IsUserBrowserContextBaseName` check.
  return browser_context &&
         (AnnotatedAccountId::Get(browser_context) ||
          IsUserBrowserContextBaseName(browser_context->GetPath().BaseName()));
}

bool IsUserBrowserContextBaseName(const base::FilePath& base_name) {
  const auto& value = base_name.value();
  return value != kSigninBrowserContextBaseName &&
         value != kLockScreenBrowserContextBaseName &&
         value != kShimlessRmaAppBrowserContextBaseName;
}

}  // namespace ash
