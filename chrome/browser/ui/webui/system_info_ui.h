// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SYSTEM_INFO_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SYSTEM_INFO_UI_H_

#include "content/public/browser/web_ui_controller.h"

class SystemInfoUI : public content::WebUIController {
 public:
  explicit SystemInfoUI(content::WebUI* web_ui);

  SystemInfoUI(const SystemInfoUI&) = delete;
  SystemInfoUI& operator=(const SystemInfoUI&) = delete;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SYSTEM_INFO_UI_H_
