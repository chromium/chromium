// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_CUSTOMIZATION_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_CUSTOMIZATION_UI_H_

#include "content/public/browser/web_ui_controller.h"

#include "base/callback.h"

namespace content {
class WebUI;
}

class ProfileCustomizationUI : public content::WebUIController {
 public:
  explicit ProfileCustomizationUI(content::WebUI* web_ui);
  ~ProfileCustomizationUI() override;

  ProfileCustomizationUI(const ProfileCustomizationUI&) = delete;
  ProfileCustomizationUI& operator=(const ProfileCustomizationUI&) = delete;

  // Initializes the ProfileCustomizationUI.
  void Initialize(base::OnceClosure done_closure);

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_CUSTOMIZATION_UI_H_
