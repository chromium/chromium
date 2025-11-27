// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/user_education_internals/user_education_internals_ui.h"

#include <vector>

#include "build/build_config.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/webui/user_education_internals/user_education_internals_page_handler_impl.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/user_education_internals_resources.h"
#include "chrome/grit/user_education_internals_resources_map.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/webui/webui_util.h"

UserEducationInternalsUIConfig::UserEducationInternalsUIConfig()
    : DefaultInternalWebUIConfig(chrome::kChromeUIUserEducationInternalsHost) {}

UserEducationInternalsUI::UserEducationInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true),
      help_bubble_handler_factory_receiver_(this) {
  auto* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIUserEducationInternalsHost);

  webui::SetupWebUIDataSource(source, kUserEducationInternalsResources,
                              IDR_USER_EDUCATION_INTERNALS_INDEX_HTML);
}

UserEducationInternalsUI::~UserEducationInternalsUI() = default;

void UserEducationInternalsUI::BindInterface(
    mojo::PendingReceiver<
        mojom::user_education_internals::UserEducationInternalsPageHandler>
        receiver) {
  user_education_handler_ =
      std::make_unique<UserEducationInternalsPageHandlerImpl>(
          web_ui(), Profile::FromWebUI(web_ui()), std::move(receiver));
}

void UserEducationInternalsUI::BindInterface(
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandlerFactory>
        pending_receiver) {
  if (help_bubble_handler_factory_receiver_.is_bound()) {
    help_bubble_handler_factory_receiver_.reset();
  }
  help_bubble_handler_factory_receiver_.Bind(std::move(pending_receiver));
}

void UserEducationInternalsUI::CreateHelpBubbleHandler(
    mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> pending_client,
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler>
        pending_handler) {
  help_bubble_handler_ = std::make_unique<user_education::HelpBubbleHandler>(
      std::move(pending_handler), std::move(pending_client), this,
      std::vector<ui::ElementIdentifier>{kWebUIIPHDemoElementIdentifier});
}

WEB_UI_CONTROLLER_TYPE_IMPL(UserEducationInternalsUI)
