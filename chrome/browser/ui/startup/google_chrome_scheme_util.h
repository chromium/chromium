// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_GOOGLE_CHROME_SCHEME_UTIL_H_
#define CHROME_BROWSER_UI_STARTUP_GOOGLE_CHROME_SCHEME_UTIL_H_

#include "base/files/file_path.h"

class GURL;

namespace startup {

// Strips the `google-chrome://` (or `chromium://`) prefix from `arg` if present
// and the `kGoogleChromeScheme` feature is enabled. Returns true if the prefix
// was stripped.
// This supports the direct launch URI scheme (e.g. google-chrome://url).
bool StripGoogleChromeScheme(base::FilePath::StringViewType& arg);

// Validates the URL whether it is allowed to be opened at launching. Dangerous
// schemes are excluded to prevent untrusted external applications from opening
// them.
bool ValidateUrl(const GURL& url);

}  // namespace startup

#endif  // CHROME_BROWSER_UI_STARTUP_GOOGLE_CHROME_SCHEME_UTIL_H_
