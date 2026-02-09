// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DEVTOOLS_GREENDEV_FLOATY_UI_H_
#define CHROME_BROWSER_UI_WEBUI_DEVTOOLS_GREENDEV_FLOATY_UI_H_

#include "content/public/browser/web_ui_controller.h"

// The WebUI controller for the GreenDev Floaty.
class GreenDevFloatyUI : public content::WebUIController {
 public:
  explicit GreenDevFloatyUI(content::WebUI* web_ui);
  ~GreenDevFloatyUI() override;

  GreenDevFloatyUI(const GreenDevFloatyUI&) = delete;
  GreenDevFloatyUI& operator=(const GreenDevFloatyUI&) = delete;
};

#endif  // CHROME_BROWSER_UI_WEBUI_DEVTOOLS_GREENDEV_FLOATY_UI_H_
