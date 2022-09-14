// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_PERFORMANCE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_PERFORMANCE_HANDLER_H_

#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace settings {

class PerformanceHandler : public SettingsPageUIHandler {
 public:
  PerformanceHandler();

  PerformanceHandler(const PerformanceHandler&) = delete;
  PerformanceHandler& operator=(const PerformanceHandler&) = delete;

  ~PerformanceHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override {}
  void OnJavascriptDisallowed() override {}

 private:
  void HandleOpenHighEfficiencyFeedbackDialog(const base::Value::List& args);
  void HandleOpenFeedbackDialog(const std::string category_tag);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_PERFORMANCE_HANDLER_H_
