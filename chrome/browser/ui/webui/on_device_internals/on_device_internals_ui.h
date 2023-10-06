// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ON_DEVICE_INTERNALS_ON_DEVICE_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ON_DEVICE_INTERNALS_ON_DEVICE_INTERNALS_UI_H_

#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "ui/webui/mojo_web_ui_controller.h"

// A dev UI for testing the OnDeviceModelService.
class OnDeviceInternalsUI : public ui::MojoWebUIController {
 public:
  explicit OnDeviceInternalsUI(content::WebUI* web_ui);
  ~OnDeviceInternalsUI() override;

  OnDeviceInternalsUI(const OnDeviceInternalsUI&) = delete;
  OnDeviceInternalsUI& operator=(const OnDeviceInternalsUI&) = delete;

  void BindInterface(
      mojo::PendingReceiver<on_device_model::mojom::OnDeviceModelService>
          receiver);

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_ON_DEVICE_INTERNALS_ON_DEVICE_INTERNALS_UI_H_
