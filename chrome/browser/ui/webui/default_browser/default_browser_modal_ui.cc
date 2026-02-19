// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/default_browser/default_browser_modal_ui.h"

#include <string>
#include <utility>

#include "base/containers/span.h"
#include "chrome/browser/ui/webui/default_browser/default_browser_modal_handler.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/default_browser_modal_resources.h"
#include "chrome/grit/default_browser_modal_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "net/base/url_util.h"
#include "ui/webui/webui_util.h"

DefaultBrowserModalUI::DefaultBrowserModalUI(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui, /*enable_chrome_send=*/false) {
  bool use_settings_illustration = false;
  std::string value;
  if (net::GetValueForKeyInQuery(web_ui->GetWebContents()->GetVisibleURL(),
                                 "illustration", &value)) {
    use_settings_illustration = value == "true";
  }

  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIDefaultBrowserModalHost);

  webui::SetupWebUIDataSource(
      source, base::span(kDefaultBrowserModalResources),
      IDR_DEFAULT_BROWSER_MODAL_DEFAULT_BROWSER_MODAL_HTML);

  static constexpr webui::LocalizedString kStrings[] = {
      {"confirmButton", IDS_DEFAULT_BROWSER_MODAL_CONFIRM_BUTTON},
      {"cancelButton", IDS_DEFAULT_BROWSER_MODAL_CANCEL_BUTTON},
  };
  source->AddLocalizedStrings(kStrings);

  source->AddLocalizedString(
      "title",
      use_settings_illustration
          ? IDS_DEFAULT_BROWSER_MODAL_TITLE_WITH_SETTINGS_ILLUSTRATION
          : IDS_DEFAULT_BROWSER_MODAL_TITLE_WITHOUT_SETTINGS_ILLUSTRATION);
  source->AddLocalizedString(
      "bodyText",
      use_settings_illustration
          ? IDS_DEFAULT_BROWSER_MODAL_BODY_WITH_SETTINGS_ILLUSTRATION
          : IDS_DEFAULT_BROWSER_MODAL_BODY_WITHOUT_SETTINGS_ILLUSTRATION);

  source->AddResourcePath("chrome_logo.svg", IDR_PRODUCT_LOGO_SVG);

  source->AddBoolean("useSettingsIllustration", use_settings_illustration);
}

WEB_UI_CONTROLLER_TYPE_IMPL(DefaultBrowserModalUI)

DefaultBrowserModalUI::~DefaultBrowserModalUI() = default;

void DefaultBrowserModalUI::CreatePageHandler(
    mojo::PendingRemote<default_browser_modal::mojom::Page> page,
    mojo::PendingReceiver<default_browser_modal::mojom::PageHandler> receiver) {
  page_handler_ = std::make_unique<DefaultBrowserModalHandler>(
      web_ui(), std::move(receiver));
}

void DefaultBrowserModalUI::BindInterface(
    mojo::PendingReceiver<default_browser_modal::mojom::PageHandlerFactory>
        receiver) {
  factory_receiver_.reset();
  factory_receiver_.Bind(std::move(receiver));
}
