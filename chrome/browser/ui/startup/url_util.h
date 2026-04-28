// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_URL_UTIL_H_
#define CHROME_BROWSER_UI_STARTUP_URL_UTIL_H_

class GURL;

namespace startup {

// Validates the URL whether it is allowed to be opened at launching.
// Dangerous schemes are excluded to prevent untrusted external applications
// from opening them.
bool ValidateUrl(const GURL& url);

}  // namespace startup

#endif  // CHROME_BROWSER_UI_STARTUP_URL_UTIL_H_
