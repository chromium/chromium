// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CONNECTORS_INTERNALS_CONNECTORS_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CONNECTORS_INTERNALS_CONNECTORS_INTERNALS_UI_H_

#include <memory>

#include "chrome/browser/ui/webui/connectors_internals/connectors_internals.mojom-forward.h"
#include "chrome/browser/ui/webui/connectors_internals/connectors_internals_page_handler.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace enterprise_connectors {

// UI controller for chrome://connectors-internals.
class ConnectorsInternalsUI : public ui::MojoWebUIController {
 public:
  explicit ConnectorsInternalsUI(content::WebUI* web_ui);

  ConnectorsInternalsUI(const ConnectorsInternalsUI&) = delete;
  ConnectorsInternalsUI& operator=(const ConnectorsInternalsUI&) = delete;

  ~ConnectorsInternalsUI() override;

  // Instantiates the implementor of the mojom::PageHandler mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<connectors_internals::mojom::PageHandler> receiver);

 private:
  std::unique_ptr<ConnectorsInternalsPageHandler> page_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_UI_WEBUI_CONNECTORS_INTERNALS_CONNECTORS_INTERNALS_UI_H_
