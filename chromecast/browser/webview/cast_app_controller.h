// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_WEBVIEW_CAST_APP_CONTROLLER_H_
#define CHROMECAST_BROWSER_WEBVIEW_CAST_APP_CONTROLLER_H_

#include "chromecast/browser/webview/web_content_controller.h"

namespace chromecast {

class CastAppController : public WebContentController {
 public:
  CastAppController(Client* client, content::WebContents* contents);

  CastAppController(const CastAppController&) = delete;
  CastAppController& operator=(const CastAppController&) = delete;

  ~CastAppController() override;

  void Destroy() override;

 protected:
  content::WebContents* GetWebContents() override;

 private:
  // content::WebContentsObserver
  void WebContentsDestroyed() override;

  content::WebContents* contents_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_WEBVIEW_CAST_APP_CONTROLLER_H_
