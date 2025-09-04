// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INCOGNITO_ALLOWED_URL_H_
#define CHROME_BROWSER_UI_INCOGNITO_ALLOWED_URL_H_

class GURL;

// Returns true if the url is allowed to open in incognito window.
bool IsURLAllowedInIncognito(const GURL& url);

#endif  // CHROME_BROWSER_UI_INCOGNITO_ALLOWED_URL_H_
