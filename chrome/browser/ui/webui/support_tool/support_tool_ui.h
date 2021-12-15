// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SUPPORT_TOOL_SUPPORT_TOOL_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SUPPORT_TOOL_SUPPORT_TOOL_UI_H_

#include "content/public/browser/web_ui_controller.h"

// The C++ back-end for the chrome://support-tool webui page.
class SupportToolUI : public content::WebUIController {
 public:
  explicit SupportToolUI(content::WebUI* web_ui);

  SupportToolUI(const SupportToolUI&) = delete;
  SupportToolUI& operator=(const SupportToolUI&) = delete;

  ~SupportToolUI() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SUPPORT_TOOL_SUPPORT_TOOL_UI_H_
