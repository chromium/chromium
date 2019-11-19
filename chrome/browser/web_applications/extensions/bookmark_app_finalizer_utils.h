// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_FINALIZER_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_FINALIZER_UTILS_H_

#include "base/callback_forward.h"

class Profile;

namespace extensions {

class Extension;

bool CanBookmarkAppCreateOsShortcuts();
void BookmarkAppCreateOsShortcuts(
    Profile* profile,
    const Extension* extension,
    bool add_to_desktop,
    base::OnceCallback<void(bool created_shortcuts)> callback);

}  // namespace extensions

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_FINALIZER_UTILS_H_
