// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_SCREENTIME_WEBPAGE_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_SCREENTIME_WEBPAGE_CONTROLLER_H_

#include "base/functional/callback.h"
#include "url/gurl.h"

@class NSView;

namespace screentime {

// The interface for the per-page controller. This interface exists to allow for
// abstracting away the concrete STWebpageController class, which is only
// available on some platforms and ties into a systemwide API that makes unit
// testing difficult. As little logic as possible should happen in
// implementations of WebpageController.
class WebpageController {
 public:
  using BlockedChangedCallback = base::RepeatingCallback<void(bool)>;

  WebpageController() = default;
  virtual ~WebpageController() = default;

  virtual NSView* GetView() = 0;

  // Called when the WebContents that this WebpageController is attached to
  // changes its committed URL to |url|, to update ScreenTime's notion of the
  // "page URL" (in Chrome parlance, the top-level frame URL).
  virtual void PageURLChangedTo(const GURL& url) = 0;
};

}  // namespace screentime

#endif  // CHROME_BROWSER_UI_COCOA_SCREENTIME_WEBPAGE_CONTROLLER_H_
