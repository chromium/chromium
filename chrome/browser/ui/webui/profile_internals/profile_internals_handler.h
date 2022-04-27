// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PROFILE_INTERNALS_PROFILE_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PROFILE_INTERNALS_PROFILE_INTERNALS_HANDLER_H_

#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

// Handles actions on Profile Internals debug page.
class ProfileInternalsHandler : public content::WebUIMessageHandler {
 public:
  ProfileInternalsHandler();

  ProfileInternalsHandler(const ProfileInternalsHandler&) = delete;
  ProfileInternalsHandler& operator=(const ProfileInternalsHandler&) = delete;

  ~ProfileInternalsHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  void HandleProfilesChanged(const base::Value::List& args);
};

#endif  // CHROME_BROWSER_UI_WEBUI_PROFILE_INTERNALS_PROFILE_INTERNALS_HANDLER_H_
