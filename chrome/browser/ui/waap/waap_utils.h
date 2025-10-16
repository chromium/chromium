// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WAAP_WAAP_UTILS_H_
#define CHROME_BROWSER_UI_WAAP_WAAP_UTILS_H_

#include "url/gurl.h"

// Returns true if the given URL is the initial WebUI scheme.
// This is only relevant on non-Android platforms.
bool IsForInitialWebUI(const GURL& url);

#endif  // CHROME_BROWSER_UI_WAAP_WAAP_UTILS_H_
