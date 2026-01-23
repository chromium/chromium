// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_GUEST_OS_BOREALIS_MOTD_BOREALIS_MOTD_UI_H_
#define CHROMEOS_ASH_EXPERIENCES_GUEST_OS_BOREALIS_MOTD_BOREALIS_MOTD_UI_H_

#include <memory>

#include "chromeos/ash/experiences/guest_os/borealis/motd/borealis_motd.mojom.h"
#include "content/public/browser/webui_config.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace content {
class WebUI;
}

namespace borealis {

class BorealisMOTDPageHandler;

// Forward declaration so that config definition can come before controller.
class BorealisMOTDUI;

class BorealisMOTDUIConfig
    : public content::DefaultWebUIConfig<BorealisMOTDUI> {
 public:
  BorealisMOTDUIConfig();
};

// The WebUI for chrome://borealis-motd
// This class is an interface. It should be implemented in //chrome
// overriding the CreatePageHandler function to properly create the
// BorealisMOTDPageHandler with its chrome delegate.
class BorealisMOTDUI : public ui::MojoWebDialogUI,
                       public ash::borealis_motd::mojom::PageHandlerFactory {
 public:
  explicit BorealisMOTDUI(content::WebUI* web_ui);
  BorealisMOTDUI(const BorealisMOTDUI&) = delete;
  BorealisMOTDUI& operator=(const BorealisMOTDUI&) = delete;
  ~BorealisMOTDUI() override;

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<ash::borealis_motd::mojom::PageHandlerFactory>
          pending_receiver);

  // ash::borealis_motd::mojom::PageHandlerFactory implementation.
  void CreatePageHandler(
      mojo::PendingRemote<ash::borealis_motd::mojom::Page> pending_page,
      mojo::PendingReceiver<ash::borealis_motd::mojom::PageHandler>
          pending_page_handler) override;

 protected:
  // A callback function that is called when the page handler is closed.
  // |action| is the action that the user took on the page.
  void OnPageClosed();

  std::unique_ptr<BorealisMOTDPageHandler> page_handler_;
  mojo::Receiver<ash::borealis_motd::mojom::PageHandlerFactory>
      page_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace borealis

#endif  // CHROMEOS_ASH_EXPERIENCES_GUEST_OS_BOREALIS_MOTD_BOREALIS_MOTD_UI_H_
