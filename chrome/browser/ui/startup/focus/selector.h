// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_FOCUS_SELECTOR_H_
#define CHROME_BROWSER_UI_STARTUP_FOCUS_SELECTOR_H_

#include <string>
#include <vector>

#include "url/gurl.h"

namespace focus {

enum class SelectorType { kApp, kUrlExact, kUrlPrefix };

// Represents criteria for matching tabs or app windows to focus.
// Can specify exact URLs, URL prefixes, or web app IDs to match against.
struct Selector {
  Selector(SelectorType type, const std::string& app_id);
  Selector(SelectorType type, const GURL& url);
  ~Selector();

  Selector(const Selector& other);
  Selector& operator=(const Selector& other);
  Selector(Selector&& other) noexcept;
  Selector& operator=(Selector&& other) noexcept;

  bool IsValid() const;
  std::string ToString() const;

  SelectorType type;
  std::string app_id;
  GURL url;
};

// Parses a comma-separated string of selectors into a vector of Selector
// objects. Each selector can be one of three types:
//
// 1. App selector: "app:APP_ID"
//    - Matches web app windows with the specified app ID
//    - Example: "app:chrome-extension://abc123" or "app:my-pwa-id"
//
// 2. Exact URL selector: "https://example.com/path"
//    - Matches tabs with exactly this URL (after canonicalization)
//    - Example: "https://github.com/user/repo"
//
// 3. URL prefix selector: "https://example.com/path/*"
//    - Matches tabs whose URLs start with the prefix (trailing /* removed)
//    - Example: "https://github.com/*" matches any GitHub page
//
// Multiple selectors can be combined with commas:
//   "app:my-app,https://example.com,https://github.com/*"
//
// Returns an empty vector if the input is invalid or contains no valid
// selectors.
std::vector<Selector> ParseSelectors(const std::string& input);

}  // namespace focus

#endif  // CHROME_BROWSER_UI_STARTUP_FOCUS_SELECTOR_H_
