// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_CONNECTIVITY_DIAGNOSTICS_CONNECTIVITY_DIAGNOSTICS_UI_H_
#define CHROMEOS_COMPONENTS_CONNECTIVITY_DIAGNOSTICS_CONNECTIVITY_DIAGNOSTICS_UI_H_

#include "ui/webui/mojo_web_ui_controller.h"

namespace chromeos {

class ConnectivityDiagnosticsUI : public ui::MojoWebUIController {
 public:
  explicit ConnectivityDiagnosticsUI(content::WebUI* web_ui);
  ~ConnectivityDiagnosticsUI() override;
  ConnectivityDiagnosticsUI(const ConnectivityDiagnosticsUI&) = delete;
  ConnectivityDiagnosticsUI& operator=(const ConnectivityDiagnosticsUI&) =
      delete;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_CONNECTIVITY_DIAGNOSTICS_CONNECTIVITY_DIAGNOSTICS_UI_H_
