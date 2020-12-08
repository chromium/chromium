// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBAPKS_WEBAPKS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_WEBAPKS_WEBAPKS_UI_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"

// The WebUI handler for chrome://webapks.
class WebApksUI : public content::WebUIController {
 public:
  explicit WebApksUI(content::WebUI* web_ui);
  ~WebApksUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebApksUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_WEBAPKS_WEBAPKS_UI_H_
