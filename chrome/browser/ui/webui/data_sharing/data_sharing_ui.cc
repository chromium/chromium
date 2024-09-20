// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/data_sharing/data_sharing_ui.h"

#include "chrome/browser/companion/core/utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/data_sharing/data_sharing_page_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/data_sharing_resources.h"
#include "chrome/grit/data_sharing_resources_map.h"
#include "components/data_sharing/public/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/color_change_listener/color_change_handler.h"
#include "ui/webui/webui_allowlist.h"

DataSharingUIConfig::DataSharingUIConfig()
    : DefaultTopChromeWebUIConfig(content::kChromeUIUntrustedScheme,
                                  chrome::kChromeUIUntrustedDataSharingHost) {}

DataSharingUIConfig::~DataSharingUIConfig() = default;

bool DataSharingUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(
      data_sharing::features::kDataSharingFeature);
}

bool DataSharingUIConfig::ShouldAutoResizeHost() {
  return true;
}

DataSharingUI::DataSharingUI(content::WebUI* web_ui)
    : UntrustedTopChromeWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIUntrustedDataSharingURL);

  webui::SetupWebUIDataSource(
      source, base::make_span(kDataSharingResources, kDataSharingResourcesSize),
      IDR_DATA_SHARING_DATA_SHARING_HTML);

  // Allow untrusted mojo resources to be loaded.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src "
      "chrome-untrusted://resources "
      "chrome-untrusted://webui-test "
      "'unsafe-inline' 'self';");

  // Allow images and avatars to be loaded.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ImgSrc,
      "img-src "
      "https://lh3.google.com "
      "https://lh3.googleusercontent.com "
      "https://www.gstatic.com "
      "'self';");

  // Allow stylesheets to be loaded.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::StyleSrc,
      "style-src "
      "chrome-untrusted://theme "
      "'unsafe-inline' "
      "'self';");

  // Allow external network connections to be made.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ConnectSrc,
      "connect-src "
      "https://play.google.com "
      "https://peoplestack-pa.clients6.google.com;");

  // Allow trsuted types to be created.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types "
      "goog#html "
      "lit-html "
      "static-types "
      "webui-test-script;");
}

DataSharingUI::~DataSharingUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(DataSharingUI)

void DataSharingUI::BindInterface(
    mojo::PendingReceiver<data_sharing::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void DataSharingUI::ApiInitComplete() {
  if (delegate_) {
    delegate_->ApiInitComplete();
  }
}

void DataSharingUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
        pending_receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(pending_receiver));
}

void DataSharingUI::CreatePageHandler(
    mojo::PendingRemote<data_sharing::mojom::Page> page,
    mojo::PendingReceiver<data_sharing::mojom::PageHandler> receiver) {
  page_handler_ = std::make_unique<DataSharingPageHandler>(
      this, std::move(receiver), std::move(page));
}
