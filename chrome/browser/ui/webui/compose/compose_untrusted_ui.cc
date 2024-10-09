// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/compose/compose_untrusted_ui.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/compose/chrome_compose_client.h"
#include "chrome/browser/compose/compose_enabling.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/grit/compose_resources.h"
#include "chrome/grit/compose_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "components/compose/core/browser/compose_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/resources/grit/webui_resources.h"
#include "ui/webui/color_change_listener/color_change_handler.h"

ComposeUIUntrustedConfig::ComposeUIUntrustedConfig()
    : DefaultTopChromeWebUIConfig(content::kChromeUIUntrustedScheme,
                                  chrome::kChromeUIUntrustedComposeHost) {}

bool ComposeUIUntrustedConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return ComposeEnabling::IsEnabledForProfile(
      Profile::FromBrowserContext(browser_context));
}

bool ComposeUIUntrustedConfig::ShouldAutoResizeHost() {
  return true;
}

ComposeUntrustedUI::ComposeUntrustedUI(content::WebUI* web_ui)
    : UntrustedTopChromeWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIUntrustedComposeUrl);
webui::SetupWebUIDataSource(
      source, base::make_span(kComposeResources, kComposeResourcesSize),
      IDR_COMPOSE_COMPOSE_HTML);

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
      {"inputModeChipPolish", IDS_COMPOSE_INPUT_MODE_POLISH},
      {"inputModeChipElaborate", IDS_COMPOSE_INPUT_MODE_ELABORATE},
      {"inputModeChipFormalize", IDS_COMPOSE_INPUT_MODE_FORMALIZE},
      {"inputFooter", IDS_COMPOSE_INPUT_FOOTER},
      {"submitButton", IDS_COMPOSE_SUBMIT_BUTTON},
      {"onDeviceUsedFooter", IDS_COMPOSE_FOOTER_FISHFOOD_ON_DEVICE_USED},
      {"resultFooter", IDS_COMPOSE_EXPERIMENTAL_DISCLAIMER_FOOTER},
      {"refinementsResultFooter",
       IDS_COMPOSE_REFINEMENTS_EXPERIMENTAL_DISCLAIMER_FOOTER},
      {"dogfoodFooter", IDS_COMPOSE_FOOTER_FISHFOOD},
      {"insertButton", IDS_COMPOSE_INSERT_BUTTON},
      {"replaceButton", IDS_COMPOSE_REPLACE_BUTTON},
      {"lengthMenuTitle", IDS_COMPOSE_MENU_LENGTH_TITLE},
      {"toneMenuTitle", IDS_COMPOSE_MENU_TONE_TITLE},
      {"modifierMenuTitle", IDS_COMPOSE_MODIFIERS_MENU_TITLE},
      {"modifierMenuLabel", IDS_COMPOSE_MODIFIERS_MENU_LABEL},
      {"retryOption", IDS_COMPOSE_MENU_RETRY_OPTION},
      {"shorterOption", IDS_COMPOSE_MENU_SHORTER_OPTION},
      {"longerOption", IDS_COMPOSE_MENU_LONGER_OPTION},
      {"casualToneOption", IDS_COMPOSE_MENU_CASUAL_OPTION},
      {"formalToneOption", IDS_COMPOSE_MENU_FORMAL_OPTION},
      {"undo", IDS_COMPOSE_UNDO_LABEL},
      {"undoButtonText", IDS_COMPOSE_UNDO_BUTTON_TEXT},
      {"redo", IDS_COMPOSE_REDO_LABEL},
      {"redoButtonText", IDS_COMPOSE_REDO_BUTTON_TEXT},
      {"errorTooShort", IDS_COMPOSE_ERROR_TOO_SHORT},
      {"errorTooLong", IDS_COMPOSE_ERROR_TOO_LONG},
      {"errorTryAgain", IDS_COMPOSE_ERROR_TRY_AGAIN},
      {"errorTryAgainLater", IDS_COMPOSE_ERROR_TRY_AGAIN_LATER},
      {"errorFiltered", IDS_COMPOSE_ERROR_FILTERED},
      {"errorFilteredGoBackButton", IDS_COMPOSE_ERROR_FILTERED_BACK_BUTTON},
      {"errorUnsupportedLanguage", IDS_COMPOSE_ERROR_UNSUPPORTED_LANGUAGE},
      {"errorPermissionDenied", IDS_COMPOSE_ERROR_PERMISSION_DENIED},
      {"errorRequestThrottled", IDS_COMPOSE_ERROR_REQUEST_THROTTLED},
      {"errorOffline", IDS_COMPOSE_ERROR_OFFLINE},
      {"editButton", IDS_COMPOSE_EDIT},
      {"editCancelButton", IDS_CANCEL},
      {"editUpdateButton", IDS_COMPOSE_EDIT_UPDATE_BUTTON},
      {"resubmit", IDS_COMPOSE_RESUBMIT},
      {"thumbsDown", IDS_COMPOSE_THUMBS_DOWN},
      {"thumbsUp", IDS_COMPOSE_THUMBS_UP},
      {"resultText", IDS_COMPOSE_RESULT_TEXT_LABEL},
      {"resultLoadingA11yMessage", IDS_COMPOSE_RESULT_LOADING_A11Y_MESSAGE},
      {"resultUpdatedA11yMessage", IDS_COMPOSE_RESULT_UPDATED_A11Y_MESSAGE},
      {"undoResultA11yMessage", IDS_COMPOSE_UNDO_RESULT_A11Y_MESSAGE},
      {"redoResultA11yMessage", IDS_COMPOSE_REDO_RESULT_A11Y_MESSAGE},
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
    "enableUpfrontInputModes",
    base::FeatureList::IsEnabled(compose::features::kComposeUpfrontInputModes));

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::StyleSrc,
      "style-src 'self' chrome-untrusted://resources chrome-untrusted://theme "
      "'unsafe-inline';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FontSrc,
      "font-src 'self' chrome-untrusted://resources;");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ImgSrc,
      "img-src 'self' chrome-untrusted://resources;");

  // If the ThemeSource isn't added here, since Compose is chrome-untrusted,
  // it will be unable to load stylesheets until a new tab is opened.
  raw_ptr<Profile> profile = Profile::FromWebUI(web_ui);
  content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(
                                           profile, /*serve_untrusted=*/true));
}

ComposeUntrustedUI::~ComposeUntrustedUI() = default;

void ComposeUntrustedUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
        pending_receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(pending_receiver));
}

void ComposeUntrustedUI::BindInterface(
    mojo::PendingReceiver<
        compose::mojom::ComposeSessionUntrustedPageHandlerFactory> factory) {
  if (session_handler_factory_.is_bound()) {
    session_handler_factory_.reset();
  }
  session_handler_factory_.Bind(std::move(factory));
}

void ComposeUntrustedUI::CreateComposeSessionUntrustedPageHandler(
    mojo::PendingReceiver<compose::mojom::ComposeClientUntrustedPageHandler>
        close_handler,
    mojo::PendingReceiver<compose::mojom::ComposeSessionUntrustedPageHandler>
        handler,
    mojo::PendingRemote<compose::mojom::ComposeUntrustedDialog> dialog) {
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

WEB_UI_CONTROLLER_TYPE_IMPL(ComposeUntrustedUI)
