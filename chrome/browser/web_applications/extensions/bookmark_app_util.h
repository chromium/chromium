// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_UTIL_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_UTIL_H_

namespace content {
class BrowserContext;
}

class GURL;
class Profile;

namespace extensions {

class Extension;
class ExtensionPrefs;

// Sets an extension pref to indicate whether the hosted app is locally
// installed or not. When apps are not locally installed they will appear in the
// app launcher, but will act like normal web pages when launched. For example
// they will never open in standalone windows. They will also have different
// commands available to them reflecting the fact that they aren't fully
// installed.
void SetBookmarkAppIsLocallyInstalled(content::BrowserContext* context,
                                      const Extension* extension,
                                      bool is_locally_installed);

// Gets whether the bookmark app is locally installed. Defaults to true if the
// extension pref that stores this isn't set.
// Note this can be called for hosted apps which should use the default.
bool BookmarkAppIsLocallyInstalled(content::BrowserContext* context,
                                   const Extension* extension);
bool BookmarkAppIsLocallyInstalled(const ExtensionPrefs* prefs,
                                   const Extension* extension);

// Generates a scope based on |launch_url| and checks if the |url| falls under
// it. https://www.w3.org/TR/appmanifest/#navigation-scope
bool IsInNavigationScopeForLaunchUrl(const GURL& launch_url, const GURL& url);

// Finds the first Shortcut App (a non-PWA Bookmark App) with |url| in its
// scope, returns nullptr if there are none.
const Extension* GetInstalledShortcutForUrl(Profile* profile, const GURL& url);

// Count a number of all bookmark apps which are installed by user
// (non default-installed apps).
int CountUserInstalledBookmarkApps(content::BrowserContext* browser_context);

}  // namespace extensions

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_UTIL_H_
