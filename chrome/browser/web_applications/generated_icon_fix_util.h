// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_GENERATED_ICON_FIX_UTIL_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_GENERATED_ICON_FIX_UTIL_H_

namespace base {
class Value;
}

namespace web_app {

class GeneratedIconFix;

// Must have all fields present and a known source.
bool IsGeneratedIconFixValid(const GeneratedIconFix& generated_icon_fix);

base::Value GeneratedIconFixToDebugValue(
    const GeneratedIconFix* generated_icon_fix);

bool operator==(const GeneratedIconFix& a, const GeneratedIconFix& b);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_GENERATED_ICON_FIX_UTIL_H_
