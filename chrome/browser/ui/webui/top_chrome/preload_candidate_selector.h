// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TOP_CHROME_PRELOAD_CANDIDATE_SELECTOR_H_
#define CHROME_BROWSER_UI_WEBUI_TOP_CHROME_PRELOAD_CANDIDATE_SELECTOR_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "chrome/browser/ui/webui/top_chrome/preload_context.h"
#include "url/gurl.h"

namespace webui {

// PreloadCandidateSelector selects the best URL to preload under the current
// condition of a given PreloadContext.
class PreloadCandidateSelector {
 public:
  virtual ~PreloadCandidateSelector() = default;

  // Initializes the selector with preloadable URLs.
  virtual void Init(const std::vector<GURL>& preloadable_urls) = 0;

  // Among preloadable URLs, selects the best URL to preload under the
  // current condition of PreloadContext.
  // If no URL should be preloaded, returns std::nullopt.
  virtual std::optional<GURL> GetURLToPreload(
      const PreloadContext& context) const = 0;
};

}  // namespace webui

#endif  // CHROME_BROWSER_UI_WEBUI_TOP_CHROME_PRELOAD_CANDIDATE_SELECTOR_H_
