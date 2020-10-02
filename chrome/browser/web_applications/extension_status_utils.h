// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSION_STATUS_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSION_STATUS_UTILS_H_

#include <string>

namespace content {
class BrowserContext;
}

namespace extensions {

bool IsExtensionBlockedByPolicy(content::BrowserContext* context,
                                const std::string& extension_id);

// Returns whether the extension with |extension_id| is installed regardless of
// disabled/blocked/terminated status.
bool IsExtensionInstalled(content::BrowserContext* context,
                          const std::string& extension_id);

// Returns whether the user has uninstalled an externally installed extension
// with |extension_id|.
bool IsExternalExtensionUninstalled(content::BrowserContext* context,
                                    const std::string& extension_id);

}  // namespace extensions

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSION_STATUS_UTILS_H_
