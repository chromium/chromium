// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_CROSTINI_UPGRADER_CROSTINI_UPGRADER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_CROSTINI_UPGRADER_CROSTINI_UPGRADER_UI_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/crostini_upgrader/crostini_upgrader.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace ash {

class CrostiniUpgraderPageHandler;
class CrostiniUpgraderUI;

// WebUIConfig for chrome://crostini-upgrader
class CrostiniUpgraderUIConfig
    : public content::DefaultWebUIConfig<CrostiniUpgraderUI> {
 public:
  CrostiniUpgraderUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUICrostiniUpgraderHost) {}
};

// The WebUI for chrome://crostini-upgrader
class CrostiniUpgraderUI : public ui::MojoWebDialogUI,
                           public crostini_upgrader::mojom::PageHandlerFactory {
 public:
  explicit CrostiniUpgraderUI(content::WebUI* web_ui);

  CrostiniUpgraderUI(const CrostiniUpgraderUI&) = delete;
  CrostiniUpgraderUI& operator=(const CrostiniUpgraderUI&) = delete;

  ~CrostiniUpgraderUI() override;

  // Send a close request to the web page. Return true if the page is already
  // closed.
  bool RequestClosePage();

  void set_launch_callback(base::OnceCallback<void(bool)>(launch_callback)) {
    launch_callback_ = std::move(launch_callback);
  }

  // Instantiates implementor of the
  // crostini_upgrader::mojom::PageHandlerFactory mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<crostini_upgrader::mojom::PageHandlerFactory>
          pending_receiver);

  base::WeakPtr<CrostiniUpgraderUI> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // crostini_upgrader::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<crostini_upgrader::mojom::Page> pending_page,
      mojo::PendingReceiver<crostini_upgrader::mojom::PageHandler>
          pending_page_handler) override;

  void OnPageClosed();

  std::unique_ptr<CrostiniUpgraderPageHandler> page_handler_;
  mojo::Receiver<crostini_upgrader::mojom::PageHandlerFactory>
      page_factory_receiver_{this};

  // Not owned. Passed to |page_handler_|
  base::OnceCallback<void(bool)> launch_callback_;

  bool page_closed_ = false;

  base::WeakPtrFactory<CrostiniUpgraderUI> weak_ptr_factory_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_CROSTINI_UPGRADER_CROSTINI_UPGRADER_UI_H_
