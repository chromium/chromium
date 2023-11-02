// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_SCREENTIME_SCREENTIME_POLICY_H_
#define CHROME_BROWSER_UI_COCOA_SCREENTIME_SCREENTIME_POLICY_H_

#include "url/gurl.h"

// Functions in this file encapsulate policy decisions about how to interact
// with and which data to supply to Screen Time.
namespace screentime {

// Return a url based on `url` to be passed to Screen Time when reporting (or
// clearing) page visits. All URLs passed to Screen Time are filtered through
// this function.
GURL URLForReporting(const GURL& url);

}  // namespace screentime

#endif  // CHROME_BROWSER_UI_COCOA_SCREENTIME_SCREENTIME_POLICY_H_
