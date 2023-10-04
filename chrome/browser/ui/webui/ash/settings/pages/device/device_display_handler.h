// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_DEVICE_DEVICE_DISPLAY_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_DEVICE_DEVICE_DISPLAY_HANDLER_H_

#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chromeos/crosapi/mojom/cros_display_config.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::settings {

// Chrome OS "Displays" settings page UI handler.
class DisplayHandler : public ::settings::SettingsPageUIHandler {
 public:
  DisplayHandler();
  DisplayHandler(const DisplayHandler&) = delete;
  DisplayHandler& operator=(const DisplayHandler&) = delete;
  ~DisplayHandler() override;

  // SettingsPageUIHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override {}
  void OnJavascriptDisallowed() override {}

 private:
  void HandleHighlightDisplay(const base::Value::List& args);
  void HandleDragDisplayDelta(const base::Value::List& args);

  mojo::Remote<crosapi::mojom::CrosDisplayConfigController>
      cros_display_config_;
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_DEVICE_DEVICE_DISPLAY_HANDLER_H_
