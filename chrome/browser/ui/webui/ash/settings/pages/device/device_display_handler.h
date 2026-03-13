// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_DEVICE_DEVICE_DISPLAY_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_DEVICE_DEVICE_DISPLAY_HANDLER_H_

#include "base/memory/raw_ref.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace ash {
class CrosDisplayConfig;
}  // namespace ash

namespace base {
class ListValue;
}  // namespace base

namespace ash::settings {

// Chrome OS "Displays" settings page UI handler.
class DisplayHandler : public content::WebUIMessageHandler {
 public:
  DisplayHandler();
  DisplayHandler(const DisplayHandler&) = delete;
  DisplayHandler& operator=(const DisplayHandler&) = delete;
  ~DisplayHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override {}
  void OnJavascriptDisallowed() override {}

 private:
  void HandleHighlightDisplay(const base::ListValue& args);
  void HandleDragDisplayDelta(const base::ListValue& args);

  const raw_ref<CrosDisplayConfig> cros_display_config_;
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_DEVICE_DEVICE_DISPLAY_HANDLER_H_
