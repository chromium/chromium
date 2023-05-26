// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_LOCATION_INTERNALS_LOCATION_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_LOCATION_INTERNALS_LOCATION_INTERNALS_UI_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

// The WebUI for chrome://location-internals
class LocationInternalsUI : public ui::MojoWebUIController {
 public:
  explicit LocationInternalsUI(content::WebUI* web_ui);

  LocationInternalsUI(const LocationInternalsUI&) = delete;
  LocationInternalsUI& operator=(const LocationInternalsUI&) = delete;

  ~LocationInternalsUI() override;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_LOCATION_INTERNALS_LOCATION_INTERNALS_UI_H_
