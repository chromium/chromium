// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_WEB_CONTENTS_DISPLAY_OBSERVER_H_
#define CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_WEB_CONTENTS_DISPLAY_OBSERVER_H_

#include <memory>

#include "base/functional/callback.h"

namespace content {
class WebContents;
}

namespace display {
class Display;
}

namespace media_router {

// Keeps track of the display that a WebContents is on.
class WebContentsDisplayObserver {
 public:
  // |web_contents|: WebContents to observe.
  // |callback|: Gets called whenever |web_contents| moves to another display.
  static std::unique_ptr<WebContentsDisplayObserver> Create(
      content::WebContents* web_contents,
      base::RepeatingClosure callback);

  virtual ~WebContentsDisplayObserver() = default;

  virtual const display::Display& GetCurrentDisplay() const = 0;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_WEB_CONTENTS_DISPLAY_OBSERVER_H_
