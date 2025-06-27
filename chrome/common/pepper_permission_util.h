// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PEPPER_PERMISSION_UTIL_H_
#define CHROME_COMMON_PEPPER_PERMISSION_UTIL_H_

#include <set>
#include <string>

class GURL;

namespace extensions {
class ExtensionSet;
}

// Returns true if the extension (or an imported module if any) is allowed.
// Module imports are at most one level deep (ie, a module that exports cannot
// import another extension).  The extension is identified by the host of |url|
// (if it is a chrome-extension URL).  |extension_set| is the list of installed
// and enabled extensions for a given profile.  |allowlist| is a set of
// (possibly hashed) extension IDs to check against.
bool IsExtensionOrSharedModuleAllowed(
    const GURL& url,
    const extensions::ExtensionSet* extension_set,
    const std::set<std::string>& allowlist);

#endif  // CHROME_COMMON_PEPPER_PERMISSION_UTIL_H_
