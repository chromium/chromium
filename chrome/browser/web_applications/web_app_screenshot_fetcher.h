// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SCREENSHOT_FETCHER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SCREENSHOT_FETCHER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace web_app {

// Interface that allows the user to fetch web app screenshots. This interface
// is provided to screenshot users to allow the download to happen
// asynchronously and not block, for example, the install dialog from being
// shown immediately.
class WebAppScreenshotFetcher {
 public:
  virtual ~WebAppScreenshotFetcher() = default;

  // Fetch a specific screenshot. This will CHECK-fail if the index is out of
  // bounds of [0, `GetScreenshotCount()`).
  virtual void GetScreenshot(
      int index,
      base::OnceCallback<void(SkBitmap bitmap,
                              std::optional<std::u16string> label)>
          callback) = 0;

  virtual const std::vector<gfx::Size>& GetScreenshotSizes() = 0;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SCREENSHOT_FETCHER_H_
