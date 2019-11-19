// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BLUETOOTH_INTERNALS_BLUETOOTH_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_BLUETOOTH_INTERNALS_BLUETOOTH_INTERNALS_UI_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/bluetooth_internals/bluetooth_internals.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class BluetoothInternalsHandler;

// The WebUI for chrome://bluetooth-internals
class BluetoothInternalsUI : public ui::MojoWebUIController {
 public:
  explicit BluetoothInternalsUI(content::WebUI* web_ui);
  ~BluetoothInternalsUI() override;

 private:
  void BindBluetoothInternalsHandler(
      mojo::PendingReceiver<mojom::BluetoothInternalsHandler> receiver);

  std::unique_ptr<BluetoothInternalsHandler> page_handler_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothInternalsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_BLUETOOTH_INTERNALS_BLUETOOTH_INTERNALS_UI_H_
