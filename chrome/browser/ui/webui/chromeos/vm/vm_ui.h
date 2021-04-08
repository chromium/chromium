// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_VM_VM_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_VM_VM_UI_H_

#include <memory>

#include "chrome/browser/ui/webui/chromeos/vm/vm.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace chromeos {

// The WebUI for chrome://vm
class VmUI : public ui::MojoWebUIController {
 public:
  explicit VmUI(content::WebUI* web_ui);
  VmUI(const VmUI&) = delete;
  VmUI& operator=(const VmUI&) = delete;
  ~VmUI() override;

  void BindInterface(
      mojo::PendingReceiver<vm::mojom::VmDiagnosticsProvider> receiver);

 private:
  std::unique_ptr<vm::mojom::VmDiagnosticsProvider> ui_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_VM_VM_UI_H_
