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

// Checks whether the host of |url| is allowed by |command_line_switch|.
//
// If the value of |command_line_switch| is:
// (1) '*': returns true for any packaged or platform apps;
// (2) a list of host names separated by ',': returns true if |host| is in the
//     list. (NOTE: In this case, |url| doesn't have to belong to an extension.)
bool IsHostAllowedByCommandLine(const GURL& url,
                                const extensions::ExtensionSet* extension_set,
                                const char* command_line_switch);
#endif  // CHROME_COMMON_PEPPER_PERMISSION_UTIL_H_
