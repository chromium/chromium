// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_WEBVIEW_WEB_CONTENTS_PROVIDER_H_
#define CHROMECAST_BROWSER_WEBVIEW_WEB_CONTENTS_PROVIDER_H_

#include "content/public/browser/web_contents.h"

namespace chromecast {

// Class that provides WebContents when given a window ID.
class WebContentsProvider {
 public:
  virtual content::WebContents* GetWebContents(int window_id) = 0;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_WEBVIEW_WEB_CONTENTS_PROVIDER_H_
