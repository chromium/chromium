// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/compose/compose_ui.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/compose/chrome_compose_client.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/compose_resources.h"
#include "chrome/grit/compose_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "components/compose/core/browser/compose_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/webui/color_change_listener/color_change_handler.h"

ComposeUI::ComposeUI(content::WebUI* web_ui)
    : ui::MojoBubbleWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIComposeHost);
  webui::SetupWebUIDataSource(
      source, base::make_span(kComposeResources, kComposeResourcesSize),
      IDR_COMPOSE_COMPOSE_HTML);
  webui::SetupChromeRefresh2023(source);

  // Localized strings.
  static constexpr webui::LocalizedString kStrings[] = {
      {"close", IDS_CLOSE},
      {"consentTitle", IDS_COMPOSE_CONSENT_TITLE},
      {"consentMainTop", IDS_COMPOSE_CONSENT_MAIN_TOP},
      {"consentMainBottom", IDS_COMPOSE_CONSENT_MAIN_BOTTOM},
      {"consentNoButton", IDS_COMPOSE_CONSENT_NO_BUTTON},
      {"consentYesButton", IDS_COMPOSE_CONSENT_YES_BUTTON},
      {"consentLearnMore", IDS_COMPOSE_CONSENT_LEARN_LINK},
      {"dialogTitle", IDS_COMPOSE_DIALOG_TITLE},
      {"disclaimerLetsGoButton", IDS_COMPOSE_DISCLAIMER_BUTTON},
      {"inputPlaceholder", IDS_COMPOSE_INPUT_PLACEHOLDER},
      {"inputFooter", IDS_COMPOSE_FOOTER_FISHFOOD},
      {"submitButton", IDS_COMPOSE_SUBMIT_BUTTON},
      {"resultFooter", IDS_COMPOSE_FOOTER_FISHFOOD},
      {"insertButton", IDS_COMPOSE_INSERT_BUTTON},
      {"lengthMenuTitle", IDS_COMPOSE_MENU_1_TITLE},
      {"shorterOption", IDS_COMPOSE_MENU_1_OPTION_1},
      {"longerOption", IDS_COMPOSE_MENU_1_OPTION_2},
      {"toneMenuTitle", IDS_COMPOSE_MENU_2_TITLE},
      {"casualToneOption", IDS_COMPOSE_MENU_2_OPTION_1},
      {"formalToneOption", IDS_COMPOSE_MENU_2_OPTION_2},
      {"errorTooShort", IDS_COMPOSE_ERROR_TOO_SHORT},
      {"errorTooLong", IDS_COMPOSE_ERROR_TOO_LONG},
      {"errorTryAgain", IDS_COMPOSE_ERROR_TRY_AGAIN},
      {"errorTryAgainLater", IDS_COMPOSE_ERROR_TRY_AGAIN_LATER},
      {"errorRequestNotSuccessful", IDS_COMPOSE_ERROR_REQUEST_NOT_SUCCESSFUL},
      {"errorPermissionDenied", IDS_COMPOSE_ERROR_REQUEST_NOT_SUCCESSFUL},
      {"errorGeneric", IDS_COMPOSE_ERROR_GENERIC},
      {"editButton", IDS_COMPOSE_EDIT},
      {"editCancelButton", IDS_CANCEL},
      {"editUpdateButton", IDS_COMPOSE_EDIT_UPDATE_BUTTON},
      {"undo", IDS_COMPOSE_UNDO},
      {"resubmit", IDS_COMPOSE_RESUBMIT},
      {"thumbsDown", IDS_THUMBS_DOWN},
      {"thumbsUp", IDS_THUMBS_UP},
  };
  source->AddLocalizedStrings(kStrings);
  source->AddBoolean("enableAnimations",
                     base::FeatureList::IsEnabled(
                         compose::features::kEnableComposeWebUIAnimations));
}

ComposeUI::~ComposeUI() = default;

void ComposeUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
        pending_receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(pending_receiver));
}

void ComposeUI::BindInterface(
    mojo::PendingReceiver<compose::mojom::ComposeSessionPageHandlerFactory>
        factory) {
  if (session_handler_factory_.is_bound()) {
    session_handler_factory_.reset();
  }
  session_handler_factory_.Bind(std::move(factory));
}

void ComposeUI::CreateComposeSessionPageHandler(
    mojo::PendingReceiver<compose::mojom::ComposeClientPageHandler>
        close_handler,
    mojo::PendingReceiver<compose::mojom::ComposeSessionPageHandler> handler,
    mojo::PendingRemote<compose::mojom::ComposeDialog> dialog) {
  DCHECK(dialog.is_valid());

  content::WebContents* web_contents = triggering_web_contents_
                                           ? triggering_web_contents_.get()
                                           : web_ui()->GetWebContents();
  ChromeComposeClient* client =
      ChromeComposeClient::FromWebContents(web_contents);
  if (client) {
    client->BindComposeDialog(std::move(close_handler), std::move(handler),
                              std::move(dialog));
  }
}

WEB_UI_CONTROLLER_TYPE_IMPL(ComposeUI)
