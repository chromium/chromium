// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NOTEBOOKS_INTERNALS_WEBUI_NOTEBOOKS_INTERNALS_UI_H_
#define COMPONENTS_NOTEBOOKS_INTERNALS_WEBUI_NOTEBOOKS_INTERNALS_UI_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/notebooks/internals/webui/notebooks_internals.mojom.h"
#include "components/notebooks/public/notebooks_eligibility_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace content {
class WebUI;
}

namespace notebooks {

class NotebooksInternalsPageHandler;

// The WebUIController for chrome://notebooks-internals.
// TODO(b/478016257): Remove this WebUI once Notebooks is launched.
class NotebooksInternalsUI
    : public ui::MojoWebUIController,
      public notebooks_internals::mojom::PageHandlerFactory {
 public:
  NotebooksInternalsUI(
      content::WebUI* web_ui,
      NotebooksEligibilityService* notebooks_eligibility_service);
  ~NotebooksInternalsUI() override;

  NotebooksInternalsUI(const NotebooksInternalsUI&) = delete;
  NotebooksInternalsUI& operator=(const NotebooksInternalsUI&) = delete;

  void BindInterface(
      mojo::PendingReceiver<notebooks_internals::mojom::PageHandlerFactory>
          receiver);

 private:
  // notebooks_internals::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<notebooks_internals::mojom::Page> page,
      mojo::PendingReceiver<notebooks_internals::mojom::PageHandler> receiver)
      override;

  raw_ptr<NotebooksEligibilityService> notebooks_eligibility_service_;
  std::unique_ptr<NotebooksInternalsPageHandler> page_handler_;
  mojo::Receiver<notebooks_internals::mojom::PageHandlerFactory>
      page_handler_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace notebooks

#endif  // COMPONENTS_NOTEBOOKS_INTERNALS_WEBUI_NOTEBOOKS_INTERNALS_UI_H_
