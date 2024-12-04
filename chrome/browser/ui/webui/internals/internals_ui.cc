// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/internals/internals_ui.h"

#include <vector>

#include "build/build_config.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/dev_ui_browser_resources.h"
#include "chrome/grit/internals_resources.h"
#include "chrome/grit/internals_resources_map.h"
#include "components/user_education/common/user_education_features.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/webui/internals/user_education/user_education_internals_page_handler_impl.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "ui/base/interaction/element_identifier.h"

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/ui/webui/internals/sessions/session_service_internals_handler.h"
#endif

InternalsUIConfig::InternalsUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         chrome::kChromeUIInternalsHost) {}

namespace {

bool ShouldHandleWebUIRequestCallback(const std::string& path) {
#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  if (SessionServiceInternalsHandler::ShouldHandleWebUIRequestCallback(path))
    return true;
#endif
  return false;
}

void HandleWebUIRequestCallback(
    Profile* profile,
    const std::string& path,
    content::WebUIDataSource::GotDataCallback callback) {
#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  if (SessionServiceInternalsHandler::ShouldHandleWebUIRequestCallback(path)) {
    return SessionServiceInternalsHandler::HandleWebUIRequestCallback(
        profile, path, std::move(callback));
  }
#endif
  NOTREACHED();
}

}  // namespace

InternalsUI::InternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true),
      help_bubble_handler_factory_receiver_(this) {
  profile_ = Profile::FromWebUI(web_ui);
  source_ = content::WebUIDataSource::CreateAndAdd(
      profile_, chrome::kChromeUIInternalsHost);

  webui::SetupWebUIDataSource(source_, kInternalsResources,
                              IDR_INTERNALS_INTERNALS_HTML);
  source_->AddResourcePath("user-education",
                           IDR_USER_EDUCATION_INTERNALS_INDEX_HTML);

  // chrome://internals/session-service
  source_->SetRequestFilter(
      base::BindRepeating(&ShouldHandleWebUIRequestCallback),
      base::BindRepeating(&HandleWebUIRequestCallback, profile_));

  source_->AddBoolean("isWhatsNewV2", user_education::features::IsWhatsNewV2());
}

InternalsUI::~InternalsUI() = default;

void InternalsUI::BindInterface(
    mojo::PendingReceiver<
        mojom::user_education_internals::UserEducationInternalsPageHandler>
        receiver) {
  user_education_handler_ =
      std::make_unique<UserEducationInternalsPageHandlerImpl>(
          web_ui(), profile_, std::move(receiver));
}

void InternalsUI::BindInterface(
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandlerFactory>
        pending_receiver) {
  if (help_bubble_handler_factory_receiver_.is_bound())
    help_bubble_handler_factory_receiver_.reset();
  help_bubble_handler_factory_receiver_.Bind(std::move(pending_receiver));
}

void InternalsUI::CreateHelpBubbleHandler(
    mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> pending_client,
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler>
        pending_handler) {
  help_bubble_handler_ = std::make_unique<user_education::HelpBubbleHandler>(
      std::move(pending_handler), std::move(pending_client), this,
      std::vector<ui::ElementIdentifier>{kWebUIIPHDemoElementIdentifier});
}

void InternalsUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
        pending_receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(pending_receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(InternalsUI)
