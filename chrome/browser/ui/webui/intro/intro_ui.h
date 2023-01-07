// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INTRO_INTRO_UI_H_
#define CHROME_BROWSER_UI_WEBUI_INTRO_INTRO_UI_H_

#include "content/public/browser/web_ui_controller.h"

// The WebUI controller for `chrome://intro`.
class IntroUI : public content::WebUIController {
 public:
  explicit IntroUI(content::WebUI* web_ui);

  IntroUI(const IntroUI&) = delete;
  IntroUI& operator=(const IntroUI&) = delete;

  ~IntroUI() override;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_INTRO_INTRO_UI_H_
