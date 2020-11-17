// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_DIAGNOSTICS_UI_H_
#define CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_DIAGNOSTICS_UI_H_

#include "base/macros.h"
#include "chromeos/components/diagnostics_ui/mojom/system_data_provider.mojom-forward.h"
#include "chromeos/components/diagnostics_ui/mojom/system_routine_controller.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace content {
class WebUI;
}  // namespace content

namespace chromeos {
namespace diagnostics {

class DiagnosticsManager;

}  // namespace diagnostics

// The WebUI for chrome://diagnostics.
class DiagnosticsUI : public ui::MojoWebUIController {
 public:
  explicit DiagnosticsUI(content::WebUI* web_ui);
  ~DiagnosticsUI() override;

  DiagnosticsUI(const DiagnosticsUI&) = delete;
  DiagnosticsUI& operator=(const DiagnosticsUI&) = delete;

  void BindInterface(
      mojo::PendingReceiver<diagnostics::mojom::SystemDataProvider> receiver);

  void BindInterface(
      mojo::PendingReceiver<diagnostics::mojom::SystemRoutineController>
          receiver);

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();

  std::unique_ptr<diagnostics::DiagnosticsManager> diagnostics_manager_;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_DIAGNOSTICS_UI_H_
