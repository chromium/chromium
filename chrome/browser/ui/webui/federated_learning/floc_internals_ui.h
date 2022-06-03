// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FEDERATED_LEARNING_FLOC_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_FEDERATED_LEARNING_FLOC_INTERNALS_UI_H_

#include <memory>

#include "chrome/browser/ui/webui/federated_learning/floc_internals.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class FlocInternalsPageHandler;

// WebUI which handles serving the chrome://floc-internals page.
class FlocInternalsUI : public ui::MojoWebUIController {
 public:
  explicit FlocInternalsUI(content::WebUI* web_ui);
  FlocInternalsUI(const FlocInternalsUI&) = delete;
  FlocInternalsUI& operator=(const FlocInternalsUI&) = delete;
  ~FlocInternalsUI() override;

  // Instantiates the implementor of the mojom::PageHandler mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<federated_learning::mojom::PageHandler> receiver);

 private:
  std::unique_ptr<FlocInternalsPageHandler> page_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_FEDERATED_LEARNING_FLOC_INTERNALS_UI_H_
