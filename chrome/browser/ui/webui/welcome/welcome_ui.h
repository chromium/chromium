// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WELCOME_WELCOME_UI_H_
#define CHROME_BROWSER_UI_WEBUI_WELCOME_WELCOME_UI_H_

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui_controller.h"
#include "url/gurl.h"

// The WebUI for chrome://welcome, the page which greets new Desktop users and
// promotes sign-in. By default, this page uses the "Welcome to Chrome" language
// and layout; the "Take Chrome Everywhere" variant may be accessed by appending
// the query string "?variant=everywhere".
class WelcomeUI : public content::WebUIController {
 public:
  WelcomeUI(content::WebUI* web_ui, const GURL& url);
  ~WelcomeUI() override;

 private:
  void StorePageSeen(Profile* profile);

  DISALLOW_COPY_AND_ASSIGN(WelcomeUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_WELCOME_WELCOME_UI_H_
