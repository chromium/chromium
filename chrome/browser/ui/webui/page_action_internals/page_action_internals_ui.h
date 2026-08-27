// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PAGE_ACTION_INTERNALS_PAGE_ACTION_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_PAGE_ACTION_INTERNALS_PAGE_ACTION_INTERNALS_UI_H_

#include <memory>

#include "chrome/browser/ui/webui/page_action_internals/page_action_internals.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/internal_webui_config.h"
#include "content/public/browser/web_ui_controller.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class PageActionInternalsHandler;
class PageActionInternalsUI;

class PageActionInternalsUIConfig final
    : public content::DefaultInternalWebUIConfig<PageActionInternalsUI> {
 public:
  PageActionInternalsUIConfig();
};

class PageActionInternalsUI final
    : public ui::MojoWebUIController,
      public page_action_internals::mojom::PageHandlerFactory {
 public:
  explicit PageActionInternalsUI(content::WebUI* web_ui);

  PageActionInternalsUI(const PageActionInternalsUI&) = delete;
  PageActionInternalsUI& operator=(const PageActionInternalsUI&) = delete;

  ~PageActionInternalsUI() override;

  void BindInterface(
      mojo::PendingReceiver<page_action_internals::mojom::PageHandlerFactory>
          receiver);

 private:
  // page_action_internals::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingReceiver<page_action_internals::mojom::PageHandler> receiver)
      override;

  std::unique_ptr<PageActionInternalsHandler> page_handler_;
  mojo::Receiver<page_action_internals::mojom::PageHandlerFactory>
      page_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_PAGE_ACTION_INTERNALS_PAGE_ACTION_INTERNALS_UI_H_
