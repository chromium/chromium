// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_GOOGLE_CHROME_SCHEME_UTIL_H_
#define CHROME_BROWSER_UI_STARTUP_GOOGLE_CHROME_SCHEME_UTIL_H_

#include <optional>

#include "base/files/file_path.h"

class GURL;

namespace startup {

// Strips the `google-chrome://` (or `chromium://`) prefix from `arg` if present
// and the `kGoogleChromeScheme` feature is enabled. Returns true if the prefix
// was stripped.
// This supports the direct launch URI scheme (e.g. google-chrome://url).
// It only accepts the scheme defined by
// `shell_integration::GetDirectLaunchUrlScheme()`.
bool StripGoogleChromeScheme(base::FilePath::StringViewType& arg);

// Helper to extract the inner URL from a GURL that uses the direct launch
// scheme. Handles platform-specific string conversions for
// `StripGoogleChromeScheme`.
std::optional<GURL> ExtractGoogleChromeSchemeInnerUrl(const GURL& url);

// Validates the URL whether it is allowed to be opened at launching. Dangerous
// schemes are excluded to prevent untrusted external applications from opening
// them.
bool ValidateUrl(const GURL& url);

}  // namespace startup

#endif  // CHROME_BROWSER_UI_STARTUP_GOOGLE_CHROME_SCHEME_UTIL_H_
