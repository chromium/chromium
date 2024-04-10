// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAPPS_INSTALLABLE_INSTALLABLE_UTILS_H_
#define CHROME_BROWSER_WEBAPPS_INSTALLABLE_INSTALLABLE_UTILS_H_

#include <set>

namespace content {
class BrowserContext;
}

class GURL;

// Returns true if there is any installed web app within |browser_context|
// contained within |origin|. For example, a web app at https://example.com/a/b
// is contained within the origin https://example.com.
//
// |origin| is a GURL type for convenience; this method will DCHECK if
// |origin| != |origin.DeprecatedGetOriginAsURL()|. Prefer using
// IsWebAppInstalledForUrl if a more specific URL is available.
bool DoesOriginContainAnyInstalledWebApp(
    content::BrowserContext* browser_context,
    const GURL& origin);

// Returns the set of HTTPS origins that contain an installed web app within
// |browser_context|. For example, if a web app at https://example.com/a/b is
// installed, the returned set will contain the origin https://example.com.
// The return types are GURLs for convenience.
std::set<GURL> GetOriginsWithInstalledWebApps(
    content::BrowserContext* browser_context);

#endif  // CHROME_BROWSER_WEBAPPS_INSTALLABLE_INSTALLABLE_UTILS_H_
