// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_MULTIDEVICE_INTERNALS_MULTIDEVICE_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_MULTIDEVICE_INTERNALS_MULTIDEVICE_INTERNALS_UI_H_

#include "ui/webui/mojo_web_ui_controller.h"

namespace chromeos {

// The WebUI controller for chrome://multidevice-internals.
class MultideviceInternalsUI : public ui::MojoWebUIController {
 public:
  explicit MultideviceInternalsUI(content::WebUI* web_ui);
  MultideviceInternalsUI(const MultideviceInternalsUI&) = delete;
  MultideviceInternalsUI& operator=(const MultideviceInternalsUI&) = delete;
  ~MultideviceInternalsUI() override;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  //  namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_MULTIDEVICE_INTERNALS_MULTIDEVICE_INTERNALS_UI_H_
