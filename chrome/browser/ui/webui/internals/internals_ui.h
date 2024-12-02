// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INTERNALS_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_INTERNALS_INTERNALS_UI_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/internals/user_education/user_education_internals.mojom.h"
#include "components/user_education/webui/help_bubble_handler.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/webui_config.h"
#include "ui/webui/color_change_listener/color_change_handler.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/help_bubble/help_bubble.mojom.h"

namespace content {
class WebUI;
}  // namespace content

class InternalsUI;

class InternalsUIConfig : public content::DefaultWebUIConfig<InternalsUI> {
 public:
  InternalsUIConfig();
};

// Client could put debug WebUI as sub-URL under chrome://internals/.
// e.g. chrome://internals/your-feature.
class InternalsUI : public ui::MojoWebUIController,
                    public help_bubble::mojom::HelpBubbleHandlerFactory {
 public:
  explicit InternalsUI(content::WebUI* web_ui);
  ~InternalsUI() override;

  void BindInterface(
      mojo::PendingReceiver<
          mojom::user_education_internals::UserEducationInternalsPageHandler>
          receiver);

  // The HelpBubbleHandlerFactory provides support for help bubbles in this
  // WebUI. Also see CreateHelpBubbleHandler() below.
  void BindInterface(
      mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandlerFactory>
          pending_receiver);

  // help_bubble::mojom::HelpBubbleHandlerFactory:
  void CreateHelpBubbleHandler(
      mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> pending_client,
      mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler>
          pending_handler) override;

  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          pending_receiver);

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();

  raw_ptr<Profile> profile_;
  raw_ptr<content::WebUIDataSource> source_;

  std::unique_ptr<
      mojom::user_education_internals::UserEducationInternalsPageHandler>
      user_education_handler_;

  std::unique_ptr<user_education::HelpBubbleHandler> help_bubble_handler_;
  mojo::Receiver<help_bubble::mojom::HelpBubbleHandlerFactory>
      help_bubble_handler_factory_receiver_;

  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_INTERNALS_INTERNALS_UI_H_
