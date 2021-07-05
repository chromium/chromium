// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_DEMO_MODE_APP_UI_DEMO_MODE_APP_UI_H_
#define CHROMEOS_COMPONENTS_DEMO_MODE_APP_UI_DEMO_MODE_APP_UI_H_

#include "ui/webui/mojo_web_ui_controller.h"

namespace chromeos {
// The WebUI for chrome://demo-mode-app
class DemoModeAppUI : public ui::MojoWebUIController {
 public:
  explicit DemoModeAppUI(content::WebUI* web_ui);
  ~DemoModeAppUI() override;

  DemoModeAppUI(const DemoModeAppUI&) = delete;
  DemoModeAppUI& operator=(const DemoModeAppUI&) = delete;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_DEMO_MODE_APP_UI_DEMO_MODE_APP_UI_H_
