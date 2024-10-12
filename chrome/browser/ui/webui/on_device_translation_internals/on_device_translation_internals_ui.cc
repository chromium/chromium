// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/on_device_translation_internals/on_device_translation_internals_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/on_device_translation_internals/on_device_translation_internals_page_handler_impl.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/on_device_translation_internals_resources.h"
#include "chrome/grit/on_device_translation_internals_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"

OnDeviceTranslationInternalsUIConfig::OnDeviceTranslationInternalsUIConfig()
    : WebUIConfig(content::kChromeUIScheme,
                  chrome::kChromeUIOnDeviceTranslationInternalsHost) {}

OnDeviceTranslationInternalsUIConfig::~OnDeviceTranslationInternalsUIConfig() =
    default;

std::unique_ptr<content::WebUIController>
OnDeviceTranslationInternalsUIConfig::CreateWebUIController(
    content::WebUI* web_ui,
    const GURL& url) {
  return std::make_unique<OnDeviceTranslationInternalsUI>(web_ui);
}

OnDeviceTranslationInternalsUI::OnDeviceTranslationInternalsUI(
    content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIOnDeviceTranslationInternalsHost);
  webui::SetupWebUIDataSource(
      source, kOnDeviceTranslationInternalsResources,
      IDR_ON_DEVICE_TRANSLATION_INTERNALS_ON_DEVICE_TRANSLATION_INTERNALS_HTML);
}

OnDeviceTranslationInternalsUI::~OnDeviceTranslationInternalsUI() = default;

void OnDeviceTranslationInternalsUI::BindInterface(
    mojo::PendingReceiver<
        on_device_translation_internals::mojom::PageHandlerFactory> receiver) {
  on_device_translation_internals_page_factory_receiver_.reset();
  on_device_translation_internals_page_factory_receiver_.Bind(
      std::move(receiver));
}

void OnDeviceTranslationInternalsUI::CreatePageHandler(
    mojo::PendingRemote<on_device_translation_internals::mojom::Page> page,
    mojo::PendingReceiver<on_device_translation_internals::mojom::PageHandler>
        receiver) {
  on_device_translation_internals_page_handler_ =
      std::make_unique<OnDeviceTranslationInternalsPageHandlerImpl>(
          std::move(receiver), std::move(page));
}

WEB_UI_CONTROLLER_TYPE_IMPL(OnDeviceTranslationInternalsUI)
