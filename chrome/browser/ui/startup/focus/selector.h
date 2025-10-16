// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_FOCUS_SELECTOR_H_
#define CHROME_BROWSER_UI_STARTUP_FOCUS_SELECTOR_H_

#include <string>
#include <vector>

#include "url/gurl.h"

namespace focus {

enum class SelectorType { kUrlExact, kUrlPrefix };

// Represents criteria for matching tabs or app windows to focus.
// URLs are matched against both tab URLs and app manifest IDs.
// If an app window's manifest ID matches, it will be preferred over tabs.
struct Selector {
  Selector(SelectorType type, const GURL& url);
  ~Selector();

  Selector(const Selector& other);
  Selector& operator=(const Selector& other);
  Selector(Selector&& other) noexcept;
  Selector& operator=(Selector&& other) noexcept;

  bool IsValid() const;
  std::string ToString() const;

  SelectorType type;
  GURL url;
};

// Parses a comma-separated string of selectors into a vector of Selector
// objects. Each selector can be one of two types:
//
// 1. Exact URL selector: "https://example.com/path"
//    - Matches tabs with exactly this URL (after canonicalization)
//    - Also matches app windows whose manifest ID equals this URL
//    - Example: "https://github.com/user/repo"
//
// 2. URL prefix selector: "https://example.com/path/*"
//    - Matches tabs whose URLs start with the prefix (trailing /* removed)
//    - Also matches app windows whose manifest ID starts with this prefix
//    - Example: "https://github.com/*" matches any GitHub page or GitHub app
//
// App windows are matched by their manifest ID (which is a URL). Since apps
// typically have unique manifest IDs, they will naturally match and be
// prioritized over regular tabs with the same URL.
//
// Multiple selectors can be combined with commas:
//   "https://app.example.com/,https://example.com,https://github.com/*"
//
// Returns an empty vector if the input is invalid or contains no valid
// selectors.
std::vector<Selector> ParseSelectors(const std::string& input);

}  // namespace focus

#endif  // CHROME_BROWSER_UI_STARTUP_FOCUS_SELECTOR_H_
