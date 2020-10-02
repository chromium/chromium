// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_CUSTOMIZATION_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_CUSTOMIZATION_HANDLER_H_

#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}

// WebUI message handler for the profile customization bubble.
class ProfileCustomizationHandler : public content::WebUIMessageHandler {
 public:
  ProfileCustomizationHandler();
  ~ProfileCustomizationHandler() override;

  ProfileCustomizationHandler(const ProfileCustomizationHandler&) = delete;
  ProfileCustomizationHandler& operator=(const ProfileCustomizationHandler&) =
      delete;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  void HandleDone(const base::ListValue* args);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_CUSTOMIZATION_HANDLER_H_
