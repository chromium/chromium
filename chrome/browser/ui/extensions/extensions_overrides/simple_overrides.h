// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_OVERRIDES_SIMPLE_OVERRIDES_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_OVERRIDES_SIMPLE_OVERRIDES_H_

#include <string>
#include <vector>

namespace extensions {
class Extension;
}

namespace simple_overrides {

// Returns true if the given `extension` is considered a "simple override"
// extension. This is the case if the extension has marginal additional
// capabilities, in addition to the override it provides.
bool IsSimpleOverrideExtension(const extensions::Extension& extension);

// Returns the list of manifest keys that are allowlisted for an extension to
// be considered a "simple override" extension.
std::vector<std::string> GetAllowlistedManifestKeysForTesting();

}  // namespace simple_overrides

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_OVERRIDES_SIMPLE_OVERRIDES_H_
