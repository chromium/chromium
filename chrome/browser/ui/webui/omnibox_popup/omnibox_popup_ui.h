// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_OMNIBOX_POPUP_UI_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_OMNIBOX_POPUP_UI_H_

#include "build/build_config.h"
#include "ui/webui/mojo_web_ui_controller.h"

// The Web UI controller for the chrome://omnibox-popup.top-chrome.
class OmniboxPopupUI : public ui::MojoWebUIController {
 public:
  explicit OmniboxPopupUI(content::WebUI* web_ui);
  OmniboxPopupUI(const OmniboxPopupUI&) = delete;
  OmniboxPopupUI& operator=(const OmniboxPopupUI&) = delete;
  ~OmniboxPopupUI() override;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_OMNIBOX_POPUP_UI_H_
