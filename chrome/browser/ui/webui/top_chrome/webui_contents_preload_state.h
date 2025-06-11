// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TOP_CHROME_WEBUI_CONTENTS_PRELOAD_STATE_H_
#define CHROME_BROWSER_UI_WEBUI_TOP_CHROME_WEBUI_CONTENTS_PRELOAD_STATE_H_

#include <optional>

#include "base/time/time.h"
#include "content/public/browser/web_contents_user_data.h"

// This WebContentsUserData is attached to WebContents created by
// WebUIContentsPreloadManager. These WebContents are not tab contents, but
// WebContents hosted by WebUIContentsWrapper in bubbles, side panels, etc.
class WebUIContentsPreloadState
    : public content::WebContentsUserData<WebUIContentsPreloadState> {
 public:
  // Whether the WebUI is (or was) preloaded.
  bool preloaded = false;

  // Whether the WebUI is ready to be shown. This is set to true when the WebUI
  // calls TopChromeWebUIController::Embedder::ShowUI().
  bool ready_to_show = false;

  // The timeticks when Request() is called. If nullopt, the WebUI is not yet
  // requested. A WebUI is considered foreground if and only if this is set.
  std::optional<base::TimeTicks> request_time;

 private:
  WEB_CONTENTS_USER_DATA_KEY_DECL();
  friend class content::WebContentsUserData<WebUIContentsPreloadState>;

  explicit WebUIContentsPreloadState(content::WebContents* web_contents);
};

#endif  // CHROME_BROWSER_UI_WEBUI_TOP_CHROME_WEBUI_CONTENTS_PRELOAD_STATE_H_
