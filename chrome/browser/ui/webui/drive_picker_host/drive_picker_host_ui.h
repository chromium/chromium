// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DRIVE_PICKER_HOST_DRIVE_PICKER_HOST_UI_H_
#define CHROME_BROWSER_UI_WEBUI_DRIVE_PICKER_HOST_DRIVE_PICKER_HOST_UI_H_

#include <string_view>

#include "chrome/browser/ui/views/drive_picker_host/drive_picker_result_handler.mojom.h"
#include "chrome/browser/ui/webui/drive_picker_host/drive_picker_host.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "chrome/common/webui_url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class DrivePickerHostUI;

class DrivePickerHostUIConfig
    : public DefaultTopChromeWebUIConfig<DrivePickerHostUI> {
 public:
  DrivePickerHostUIConfig();

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The WebUI controller for chrome://drive-picker-host.
// It implements DrivePickerHostHandler for communication from the Trusted JS.
class DrivePickerHostUI
    : public TopChromeWebUIController,
      public drive_picker_host::mojom::DrivePickerHostHandler {
 public:
  explicit DrivePickerHostUI(content::WebUI* web_ui);
  ~DrivePickerHostUI() override;

  DrivePickerHostUI(const DrivePickerHostUI&) = delete;
  DrivePickerHostUI& operator=(const DrivePickerHostUI&) = delete;

  static std::string_view GetWebUIName() { return "DrivePickerHost"; }

  // Triggers the Drive Picker host logic to display the picker UI and relay
  // results to `result_handler`. If consent has not yet been granted, a
  // consent dialog is shown first.
  virtual void TriggerDrivePickerHost(
      mojo::PendingRemote<drive_picker_host::mojom::DrivePickerResultHandler>
          result_handler);

  void BindInterface(
      mojo::PendingReceiver<drive_picker_host::mojom::DrivePickerHostHandler>
          receiver);

 private:
  // Remote to relay results back to the C++ callers.
  mojo::Remote<drive_picker_host::mojom::DrivePickerResultHandler>
      result_remote_;

  mojo::Receiver<drive_picker_host::mojom::DrivePickerHostHandler> receiver_{
      this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_DRIVE_PICKER_HOST_DRIVE_PICKER_HOST_UI_H_
