// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DRIVE_PICKER_HOST_UNTRUSTED_DRIVE_PICKER_HOST_UNTRUSTED_UI_H_
#define CHROME_BROWSER_UI_WEBUI_DRIVE_PICKER_HOST_UNTRUSTED_DRIVE_PICKER_HOST_UNTRUSTED_UI_H_

#include <string_view>

#include "base/gtest_prod_util.h"
#include "chrome/browser/ui/webui/drive_picker_host/untrusted/drive_picker_host_untrusted.mojom.h"
#include "content/public/browser/webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/webui/untrusted_web_ui_controller.h"

class DrivePickerUntrustedHostUI;

class DrivePickerUntrustedHostUIConfig
    : public content::DefaultWebUIConfig<DrivePickerUntrustedHostUI> {
 public:
  DrivePickerUntrustedHostUIConfig();
  ~DrivePickerUntrustedHostUIConfig() override;

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The WebUI controller for chrome-untrusted://drive-picker-host.
// It implements DrivePickerBridge for communication from the Trusted side,
// and PageHandlerFactory for communication from the Untrusted JS.
class DrivePickerUntrustedHostUI
    : public ui::UntrustedWebUIController,
      public drive_picker_host_untrusted::mojom::DrivePickerBridge,
      public drive_picker_host_untrusted::mojom::PageHandlerFactory,
      public drive_picker_host_untrusted::mojom::PageHandler {
 public:
  explicit DrivePickerUntrustedHostUI(content::WebUI* web_ui);
  ~DrivePickerUntrustedHostUI() override;

  DrivePickerUntrustedHostUI(const DrivePickerUntrustedHostUI&) = delete;
  DrivePickerUntrustedHostUI& operator=(const DrivePickerUntrustedHostUI&) =
      delete;

  void BindInterface(
      mojo::PendingReceiver<
          drive_picker_host_untrusted::mojom::PageHandlerFactory> receiver);

  void BindInterface(
      mojo::PendingReceiver<
          drive_picker_host_untrusted::mojom::DrivePickerBridge> receiver);

  // drive_picker_host_untrusted::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<drive_picker_host_untrusted::mojom::Page> page,
      mojo::PendingReceiver<drive_picker_host_untrusted::mojom::PageHandler>
          handler) override;

  // drive_picker_host_untrusted::mojom::DrivePickerBridge:
  void ShowDrivePicker(
      mojo::PendingRemote<drive_picker_host::mojom::DrivePickerResultHandler>
          result_handler,
      drive_picker_host_untrusted::mojom::DrivePickerKeysPtr keys) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(DrivePickerUntrustedHostUITest,
                           ShowDrivePickerQueuesOnDisconnect);

  mojo::Receiver<drive_picker_host_untrusted::mojom::PageHandlerFactory>
      factory_receiver_{this};
  mojo::Receiver<drive_picker_host_untrusted::mojom::PageHandler>
      page_handler_receiver_{this};
  mojo::Receiver<drive_picker_host_untrusted::mojom::DrivePickerBridge>
      bridge_receiver_{this};

  mojo::Remote<drive_picker_host_untrusted::mojom::Page> page_;

  // Data for a single request that arrived before the page was bound.
  struct PendingRequest {
    PendingRequest(
        mojo::PendingRemote<drive_picker_host::mojom::DrivePickerResultHandler>
            handler,
        drive_picker_host_untrusted::mojom::DrivePickerKeysPtr keys);
    ~PendingRequest();
    mojo::PendingRemote<drive_picker_host::mojom::DrivePickerResultHandler>
        result_handler;
    drive_picker_host_untrusted::mojom::DrivePickerKeysPtr keys;
  };
  std::unique_ptr<PendingRequest> pending_request_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_DRIVE_PICKER_HOST_UNTRUSTED_DRIVE_PICKER_HOST_UNTRUSTED_UI_H_
