// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CAPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CAPTIONS_HANDLER_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace settings {

// UI handler for Chrome caption settings subpage on operating systems other
// than Chrome OS and Linux.
class CaptionsHandler : public SettingsPageUIHandler {
 public:
  CaptionsHandler();
  ~CaptionsHandler() override;

  // SettingsPageUIHandler overrides:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  void HandleOpenSystemCaptionsDialog(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(CaptionsHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CAPTIONS_HANDLER_H_
