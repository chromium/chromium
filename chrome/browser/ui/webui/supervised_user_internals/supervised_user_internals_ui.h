// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SUPERVISED_USER_INTERNALS_SUPERVISED_USER_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SUPERVISED_USER_INTERNALS_SUPERVISED_USER_INTERNALS_UI_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"

// The implementation for the chrome://supervised-user-internals page.
class SupervisedUserInternalsUI : public content::WebUIController {
 public:
  explicit SupervisedUserInternalsUI(content::WebUI* web_ui);
  ~SupervisedUserInternalsUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SupervisedUserInternalsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SUPERVISED_USER_INTERNALS_SUPERVISED_USER_INTERNALS_UI_H_
