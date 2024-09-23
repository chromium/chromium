// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BLUETOOTH_INTERNALS_BLUETOOTH_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_BLUETOOTH_INTERNALS_BLUETOOTH_INTERNALS_UI_H_

#include "chrome/browser/ui/webui/bluetooth_internals/bluetooth_internals.mojom-forward.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class BluetoothInternalsHandler;
class BluetoothInternalsUI;

class BluetoothInternalsUIConfig
    : public content::DefaultWebUIConfig<BluetoothInternalsUI> {
 public:
  BluetoothInternalsUIConfig();
  ~BluetoothInternalsUIConfig() override;
};

// The WebUI for chrome://bluetooth-internals
class BluetoothInternalsUI : public ui::MojoWebUIController {
 public:
  explicit BluetoothInternalsUI(content::WebUI* web_ui);

  BluetoothInternalsUI(const BluetoothInternalsUI&) = delete;
  BluetoothInternalsUI& operator=(const BluetoothInternalsUI&) = delete;

  ~BluetoothInternalsUI() override;

  // Instantiates the implementor of the mojom::BluetoothInternalsHandler mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      content::RenderFrameHost* host,
      mojo::PendingReceiver<mojom::BluetoothInternalsHandler> receiver);

 private:
  std::unique_ptr<BluetoothInternalsHandler> page_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_BLUETOOTH_INTERNALS_BLUETOOTH_INTERNALS_UI_H_
