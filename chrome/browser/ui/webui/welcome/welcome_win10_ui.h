// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WELCOME_WELCOME_WIN10_UI_H_
#define CHROME_BROWSER_UI_WEBUI_WELCOME_WELCOME_WIN10_UI_H_

#include "content/public/browser/web_ui_controller.h"

class GURL;

// The WebUI for chrome://welcome-win10, the page which greets new Windows 10
// users and educates them about setting the default browser and pinning the
// browser to their taskbar.
class WelcomeWin10UI : public content::WebUIController {
 public:
  WelcomeWin10UI(content::WebUI* web_ui, const GURL& url);
  ~WelcomeWin10UI() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_WELCOME_WELCOME_WIN10_UI_H_
