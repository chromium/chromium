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
      {"dialogTitle", IDS_COMPOSE_TITLE},
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
      {"errorTooLong", IDS_COMPOSE_ERROR_TOO_LONG},
      {"errorTryAgain", IDS_COMPOSE_ERROR_TRY_AGAIN},
      {"errorTryAgainLater", IDS_COMPOSE_ERROR_TRY_AGAIN_LATER},
      {"errorRequestNotSuccessful", IDS_COMPOSE_ERROR_REQUEST_NOT_SUCCESSFUL},
      {"errorPermissionDenied", IDS_COMPOSE_ERROR_REQUEST_NOT_SUCCESSFUL},
      {"errorGeneric", IDS_COMPOSE_ERROR_GENERIC},
      {"editButton", IDS_EDIT},
      {"editCancelButton", IDS_CANCEL},
      {"editUpdateButton", IDS_COMPOSE_EDIT_UPDATE_BUTTON},
      {"fileBugText", IDS_COMPOSE_FILE_BUG},
  };
  source->AddLocalizedStrings(kStrings);
}

ComposeUI::~ComposeUI() = default;

void ComposeUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
        pending_receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(pending_receiver));
}

void ComposeUI::BindInterface(
    mojo::PendingReceiver<compose::mojom::ComposeDialogPageHandlerFactory>
        factory) {
  if (dialog_handler_factory_.is_bound()) {
    dialog_handler_factory_.reset();
  }
  dialog_handler_factory_.Bind(std::move(factory));
}

void ComposeUI::CreateComposeDialogPageHandler(
    mojo::PendingReceiver<compose::mojom::ComposeDialogClosePageHandler>
        close_handler,
    mojo::PendingReceiver<compose::mojom::ComposeDialogPageHandler> handler,
    mojo::PendingRemote<compose::mojom::ComposeDialog> dialog) {
  DCHECK(dialog.is_valid());

  content::WebContents* web_contents = triggering_web_contents_
                                           ? triggering_web_contents_.get()
                                           : web_ui()->GetWebContents();
  ChromeComposeClient::FromWebContents(web_contents)
      ->BindComposeDialog(std::move(close_handler), std::move(handler),
                          std::move(dialog));
}

WEB_UI_CONTROLLER_TYPE_IMPL(ComposeUI)
