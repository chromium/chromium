// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"

#include "base/files/file_path.h"
#include "content/public/browser/browser_context.h"

namespace ash {

const char kSigninBrowserContextBaseName[] = "Default";
const char kLockScreenAppBrowserContextBaseName[] = "LockScreenAppsProfile";
const char kLockScreenBrowserContextBaseName[] = "LockScreenProfile";
const char kShimlessRmaAppBrowserContextBaseName[] = "ShimlessRmaAppProfile";

bool IsSigninBrowserContext(content::BrowserContext* browser_context) {
  return browser_context && browser_context->GetPath().BaseName().value() ==
                                kSigninBrowserContextBaseName;
}

bool IsLockScreenAppBrowserContext(content::BrowserContext* browser_context) {
  return browser_context && browser_context->GetPath().BaseName().value() ==
                                kLockScreenAppBrowserContextBaseName;
}

bool IsLockScreenBrowserContext(content::BrowserContext* browser_context) {
  return browser_context && browser_context->GetPath().BaseName().value() ==
                                kLockScreenBrowserContextBaseName;
}

bool IsShimlessRmaAppBrowserContext(content::BrowserContext* browser_context) {
  return browser_context && browser_context->GetPath().BaseName().value() ==
                                kShimlessRmaAppBrowserContextBaseName;
}

bool IsUserBrowserContext(content::BrowserContext* browser_context) {
  return browser_context &&
         IsUserBrowserContextBaseName(browser_context->GetPath().BaseName());
}

bool IsUserBrowserContextBaseName(const base::FilePath& base_name) {
  const auto& value = base_name.value();
  return value != kSigninBrowserContextBaseName &&
         value != kLockScreenAppBrowserContextBaseName &&
         value != kLockScreenBrowserContextBaseName &&
         value != kShimlessRmaAppBrowserContextBaseName;
}

}  // namespace ash
