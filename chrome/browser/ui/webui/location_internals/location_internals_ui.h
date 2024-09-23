// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_LOCATION_INTERNALS_LOCATION_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_LOCATION_INTERNALS_LOCATION_INTERNALS_UI_H_

#include "chrome/browser/ui/webui/location_internals/location_internals.mojom-forward.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class LocationInternalsHandler;
class LocationInternalsUI;

// WebUIConfig for chrome://location-internals
class LocationInternalsUIConfig
    : public content::DefaultWebUIConfig<LocationInternalsUI> {
 public:
  LocationInternalsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUILocationInternalsHost) {}
};

// The WebUI for chrome://location-internals
class LocationInternalsUI : public ui::MojoWebUIController {
 public:
  explicit LocationInternalsUI(content::WebUI* web_ui);

  LocationInternalsUI(const LocationInternalsUI&) = delete;
  LocationInternalsUI& operator=(const LocationInternalsUI&) = delete;

  ~LocationInternalsUI() override;

  // Instantiates the implementor of the mojom::LocationInternalsHandler mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<mojom::LocationInternalsHandler> receiver);

 private:
  std::unique_ptr<LocationInternalsHandler> handler_;
  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_LOCATION_INTERNALS_LOCATION_INTERNALS_UI_H_
