// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DRIVE_PICKER_HOST_UNTRUSTED_DRIVE_PICKER_HOST_UNTRUSTED_UI_H_
#define CHROME_BROWSER_UI_WEBUI_DRIVE_PICKER_HOST_UNTRUSTED_DRIVE_PICKER_HOST_UNTRUSTED_UI_H_

#include <string_view>

#include "chrome/browser/ui/webui/drive_picker_host/untrusted/drive_picker_host_untrusted.mojom.h"
#include "content/public/browser/webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/untrusted_web_ui_controller.h"

class DrivePickerUntrustedHostUI;

// WebUI config for the Drive Picker untrusted host page.
class DrivePickerUntrustedHostUIConfig
    : public content::DefaultWebUIConfig<DrivePickerUntrustedHostUI> {
 public:
  DrivePickerUntrustedHostUIConfig();
  ~DrivePickerUntrustedHostUIConfig() override;

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The WebUI controller for chrome-untrusted://drive-picker-host.
class DrivePickerUntrustedHostUI : public ui::UntrustedWebUIController,
                                   public drive_picker_host_untrusted::mojom::
                                       DrivePickerUntrustedHostHandler {
 public:
  explicit DrivePickerUntrustedHostUI(content::WebUI* web_ui);
  DrivePickerUntrustedHostUI(const DrivePickerUntrustedHostUI&) = delete;
  DrivePickerUntrustedHostUI& operator=(const DrivePickerUntrustedHostUI&) =
      delete;
  ~DrivePickerUntrustedHostUI() override;

  static std::string_view GetWebUIName() { return "DrivePickerUntrustedHost"; }

  void BindInterface(
      mojo::PendingReceiver<
          drive_picker_host_untrusted::mojom::DrivePickerUntrustedHostHandler>
          receiver);

 private:
  mojo::Receiver<
      drive_picker_host_untrusted::mojom::DrivePickerUntrustedHostHandler>
      handler_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_DRIVE_PICKER_HOST_UNTRUSTED_DRIVE_PICKER_HOST_UNTRUSTED_UI_H_
