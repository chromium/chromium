// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ACTOR_INTERNALS_ACTOR_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ACTOR_INTERNALS_ACTOR_INTERNALS_UI_H_

#include "chrome/browser/ui/webui/actor_internals/actor_internals.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/internal_webui_config.h"
#include "content/public/browser/web_ui_controller.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class ActorInternalsUI;
class ActorInternalsUIHandler;

class ActorInternalsUIConfig
    : public content::DefaultInternalWebUIConfig<ActorInternalsUI> {
 public:
  ActorInternalsUIConfig()
      : DefaultInternalWebUIConfig(chrome::kChromeUIActorInternalsHost) {}
};

// The UI for chrome://actor-internals/
class ActorInternalsUI : public ui::MojoWebUIController,
                         public actor_internals::mojom::PageHandlerFactory {
 public:
  explicit ActorInternalsUI(content::WebUI* contents);

  ActorInternalsUI(const ActorInternalsUI&) = delete;
  ActorInternalsUI& operator=(const ActorInternalsUI&) = delete;

  ~ActorInternalsUI() override;

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<actor_internals::mojom::PageHandlerFactory>
          receiver);

 private:
  // actor_internals::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<actor_internals::mojom::Page> page,
      mojo::PendingReceiver<actor_internals::mojom::PageHandler> receiver)
      override;

  std::unique_ptr<ActorInternalsUIHandler> page_handler_;
  mojo::Receiver<actor_internals::mojom::PageHandlerFactory>
      page_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_ACTOR_INTERNALS_ACTOR_INTERNALS_UI_H_
