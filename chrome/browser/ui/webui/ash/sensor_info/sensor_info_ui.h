// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SENSOR_INFO_SENSOR_INFO_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SENSOR_INFO_SENSOR_INFO_UI_H_

#include "ash/sensor_info/sensor_provider.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/ash/sensor_info/sensor.mojom.h"
#include "chrome/browser/ui/webui/ash/sensor_info/sensor_page_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class Profile;

namespace ash {

class SensorInfoUI;

// WebUIConfig for chrome://sensor-info.
class SensorInfoUIConfig : public content::DefaultWebUIConfig<SensorInfoUI> {
 public:
  SensorInfoUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUISensorInfoHost) {}
};

// The WebUI controller for chrome://sensor-info.
class SensorInfoUI : public ui::MojoWebUIController,
                     public sensor::mojom::PageHandlerFactory {
 public:
  explicit SensorInfoUI(content::WebUI* web_ui);
  SensorInfoUI(const SensorInfoUI&) = delete;
  SensorInfoUI& operator=(const SensorInfoUI&) = delete;
  ~SensorInfoUI() override;

  // Disconnects the previous connection, if there exists. Instantiates the
  // implementor of the mojom::PageHandlerFactory mojo interface passing the
  // pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<sensor::mojom::PageHandlerFactory> receiver);

 private:
  // sensor::mojom::PageHandlerFactory:
  // Creates SensorPageHandler with parameters. This function is only called
  // once when creating webui.
  void CreatePageHandler(
      mojo::PendingReceiver<sensor::mojom::PageHandler> receiver) override;

  raw_ptr<Profile> profile_;
  mojo::Receiver<sensor::mojom::PageHandlerFactory> page_factory_receiver_{
      this};
  ash::SensorProvider provider_;
  std::unique_ptr<SensorPageHandler> page_handler_;
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SENSOR_INFO_SENSOR_INFO_UI_H_
