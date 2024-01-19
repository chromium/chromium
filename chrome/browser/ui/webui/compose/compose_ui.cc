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
#include "components/compose/core/browser/config.h"
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
      {"freMsbbTitle", IDS_COMPOSE_FRE_MSBB_TITLE},
      {"freMsbbMain", IDS_COMPOSE_FRE_MSBB_MAIN},
      {"freMsbbSettingsButton", IDS_COMPOSE_FRE_MSBB_SETTINGS_BUTTON},
      {"firstRunTitle", IDS_COMPOSE_FRE_TITLE},
      {"firstRunMainTop", IDS_COMPOSE_FRE_MAIN_TOP},
      {"firstRunMainMid", IDS_COMPOSE_FRE_MAIN_MID},
      {"firstRunMainBottom", IDS_COMPOSE_EXPERIMENTAL_DISCLAIMER_FOOTER},
      {"firstRunOkButton", IDS_COMPOSE_FRE_OK_BUTTON},
      {"dialogTitle", IDS_COMPOSE_DIALOG_TITLE},
      {"inputPlaceholderTitle", IDS_COMPOSE_INPUT_PLACEHOLDER_TITLE},
      {"inputPlaceholderLine1", IDS_COMPOSE_INPUT_PLACEHOLDER_LINE_1},
      {"inputPlaceholderLine2", IDS_COMPOSE_INPUT_PLACEHOLDER_LINE_2},
      {"inputPlaceholderLine3", IDS_COMPOSE_INPUT_PLACEHOLDER_LINE_3},
      {"inputFooter", IDS_COMPOSE_INPUT_FOOTER},
      {"submitButton", IDS_COMPOSE_SUBMIT_BUTTON},
      {"onDeviceUsedFooter", IDS_COMPOSE_FOOTER_FISHFOOD_ON_DEVICE_USED},
      {"resultFooter", IDS_COMPOSE_EXPERIMENTAL_DISCLAIMER_FOOTER},
      {"dogfoodFooter", IDS_COMPOSE_FOOTER_FISHFOOD},
      {"insertButton", IDS_COMPOSE_INSERT_BUTTON},
      {"replaceButton", IDS_COMPOSE_REPLACE_BUTTON},
      {"lengthMenuTitle", IDS_COMPOSE_MENU_LENGTH_TITLE},
      {"shorterOption", IDS_COMPOSE_MENU_SHORTER_OPTION},
      {"longerOption", IDS_COMPOSE_MENU_LONGER_OPTION},
      {"toneMenuTitle", IDS_COMPOSE_MENU_TONE_TITLE},
      {"casualToneOption", IDS_COMPOSE_MENU_CASUAL_OPTION},
      {"formalToneOption", IDS_COMPOSE_MENU_FORMAL_OPTION},
      {"errorTooShort", IDS_COMPOSE_ERROR_TOO_SHORT},
      {"errorTooLong", IDS_COMPOSE_ERROR_TOO_LONG},
      {"errorTryAgain", IDS_COMPOSE_ERROR_TRY_AGAIN},
      {"errorTryAgainLater", IDS_COMPOSE_ERROR_TRY_AGAIN_LATER},
      {"errorFiltered", IDS_COMPOSE_ERROR_FILTERED},
      {"errorUnsupportedLanguage", IDS_COMPOSE_ERROR_UNSUPPORTED_LANGUAGE},
      {"errorPermissionDenied", IDS_COMPOSE_ERROR_PERMISSION_DENIED},
      {"errorRequestThrottled", IDS_COMPOSE_ERROR_REQUEST_THROTTLED},
      {"errorOffline", IDS_COMPOSE_ERROR_OFFLINE},
      {"editButton", IDS_COMPOSE_EDIT},
      {"editCancelButton", IDS_CANCEL},
      {"editUpdateButton", IDS_COMPOSE_EDIT_UPDATE_BUTTON},
      {"undo", IDS_COMPOSE_UNDO},
      {"resubmit", IDS_COMPOSE_RESUBMIT},
      {"thumbsDown", IDS_COMPOSE_THUMBS_DOWN},
      {"thumbsUp", IDS_COMPOSE_THUMBS_UP},
      {"savedText", IDS_COMPOSE_SUGGESTION_SAVED_TEXT},
      {"savedLabel", IDS_COMPOSE_SUGGESTION_SAVED_LABEL},
  };
  source->AddLocalizedStrings(kStrings);
  source->AddBoolean("enableAnimations",
                     base::FeatureList::IsEnabled(
                         compose::features::kEnableComposeWebUIAnimations));
  source->AddBoolean(
      "enableOnDeviceDogfoodFooter",
      base::FeatureList::IsEnabled(
          compose::features::kEnableComposeOnDeviceDogfoodFooter));
  source->AddBoolean(
      "enableSavedStateNotification",
      base::FeatureList::IsEnabled(
          compose::features::kEnableComposeSavedStateNotification));

  const compose::Config& config = compose::GetComposeConfig();
  source->AddInteger("savedStateTimeoutInMilliseconds",
                     config.saved_state_timeout_milliseconds);
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
