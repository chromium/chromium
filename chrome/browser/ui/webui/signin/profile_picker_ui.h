// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_PICKER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_PICKER_UI_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_ui_controller.h"
#include "ui/gfx/geometry/size.h"

class ProfilePickerHandler;

// The WebUI controller for chrome://profile-picker/.
class ProfilePickerUI : public content::WebUIController {
 public:
  explicit ProfilePickerUI(content::WebUI* web_ui);
  ~ProfilePickerUI() override;

  ProfilePickerUI(const ProfilePickerUI&) = delete;
  ProfilePickerUI& operator=(const ProfilePickerUI&) = delete;

  // Get the minimum size for the picker UI.
  static gfx::Size GetMinimumSize();

  // Allows tests to trigger page events.
  ProfilePickerHandler* GetProfilePickerHandlerForTesting();

 private:
  // Stored for tests.
  raw_ptr<ProfilePickerHandler> profile_picker_handler_ = nullptr;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_PICKER_UI_H_
