// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_WEBVIEW_CAST_APP_CONTROLLER_H_
#define CHROMECAST_BROWSER_WEBVIEW_CAST_APP_CONTROLLER_H_

#include "chromecast/browser/webview/web_content_controller.h"

#include "base/macros.h"

namespace chromecast {

class CastAppController : public WebContentController {
 public:
  CastAppController(Client* client, content::WebContents* contents);
  ~CastAppController() override;

  void Destroy() override;

 protected:
  content::WebContents* GetWebContents() override;

 private:
  content::WebContents* contents_;

  DISALLOW_COPY_AND_ASSIGN(CastAppController);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_WEBVIEW_CAST_APP_CONTROLLER_H_
