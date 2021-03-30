// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_CROSTINI_UPGRADER_CROSTINI_UPGRADER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_CROSTINI_UPGRADER_CROSTINI_UPGRADER_UI_H_

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/ui/webui/chromeos/crostini_upgrader/crostini_upgrader.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace chromeos {

class CrostiniUpgraderPageHandler;

// The WebUI for chrome://crostini-upgrader
class CrostiniUpgraderUI
    : public ui::MojoWebDialogUI,
      public chromeos::crostini_upgrader::mojom::PageHandlerFactory {
 public:
  explicit CrostiniUpgraderUI(content::WebUI* web_ui);
  ~CrostiniUpgraderUI() override;

  // Send a close request to the web page. Return true if the page is already
  // closed.
  bool RequestClosePage();

  void set_launch_callback(base::OnceCallback<void(bool)>(launch_callback)) {
    launch_callback_ = std::move(launch_callback);
  }

  // Instantiates implementor of the
  // chromeos::crostini_upgrader::mojom::PageHandlerFactory mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(mojo::PendingReceiver<
                     chromeos::crostini_upgrader::mojom::PageHandlerFactory>
                         pending_receiver);

 private:
  // chromeos::crostini_upgrader::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<chromeos::crostini_upgrader::mojom::Page>
          pending_page,
      mojo::PendingReceiver<chromeos::crostini_upgrader::mojom::PageHandler>
          pending_page_handler) override;

  void OnPageClosed();

  std::unique_ptr<CrostiniUpgraderPageHandler> page_handler_;
  mojo::Receiver<chromeos::crostini_upgrader::mojom::PageHandlerFactory>
      page_factory_receiver_{this};

  // Not owned. Passed to |page_handler_|
  base::OnceCallback<void(bool)> launch_callback_;

  bool page_closed_ = false;

  WEB_UI_CONTROLLER_TYPE_DECL();

  DISALLOW_COPY_AND_ASSIGN(CrostiniUpgraderUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_CROSTINI_UPGRADER_CROSTINI_UPGRADER_UI_H_
