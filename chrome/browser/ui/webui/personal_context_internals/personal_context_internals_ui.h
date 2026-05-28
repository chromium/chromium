// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PERSONAL_CONTEXT_INTERNALS_PERSONAL_CONTEXT_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_PERSONAL_CONTEXT_INTERNALS_PERSONAL_CONTEXT_INTERNALS_UI_H_

#include "chrome/browser/ui/webui/personal_context_internals/personal_context_internals.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/internal_webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class PersonalContextInternalsPageHandler;
class PersonalContextInternalsUI;

class PersonalContextInternalsUIConfig
    : public content::DefaultInternalWebUIConfig<PersonalContextInternalsUI> {
 public:
  PersonalContextInternalsUIConfig()
      : DefaultInternalWebUIConfig(
            chrome::kChromeUIPersonalContextInternalsHost) {}
};

class PersonalContextInternalsUI
    : public ui::MojoWebUIController,
      public browser::personal_context_internals::mojom::PageHandlerFactory {
 public:
  explicit PersonalContextInternalsUI(content::WebUI* web_ui);
  ~PersonalContextInternalsUI() override;

  PersonalContextInternalsUI(const PersonalContextInternalsUI&) = delete;
  PersonalContextInternalsUI& operator=(const PersonalContextInternalsUI&) =
      delete;

  void BindInterface(
      mojo::PendingReceiver<
          browser::personal_context_internals::mojom::PageHandlerFactory>
          receiver);

 private:
  // browser::personal_context_internals::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingReceiver<
          browser::personal_context_internals::mojom::PageHandler> handler)
      override;

  std::unique_ptr<PersonalContextInternalsPageHandler> page_handler_;
  mojo::Receiver<browser::personal_context_internals::mojom::PageHandlerFactory>
      page_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_PERSONAL_CONTEXT_INTERNALS_PERSONAL_CONTEXT_INTERNALS_UI_H_
