// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_VM_VM_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_VM_VM_UI_H_

#include <memory>

#include "chrome/browser/ui/webui/ash/vm/vm.mojom-forward.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash {

class VmUI;

// WebUIConfig for chrome://vm
class VmUIConfig : public content::DefaultWebUIConfig<VmUI> {
 public:
  VmUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme, chrome::kChromeUIVmHost) {}
};

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

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_VM_VM_UI_H_
