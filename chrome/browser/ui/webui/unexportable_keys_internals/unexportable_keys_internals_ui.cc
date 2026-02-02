// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/unexportable_keys_internals/unexportable_keys_internals_ui.h"

#include <memory>
#include <utility>

#include "base/check_deref.h"
#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/bound_session_credentials/unexportable_key_provider_config.h"
#include "chrome/browser/signin/bound_session_credentials/unexportable_key_service_factory.h"
#include "chrome/browser/ui/webui/unexportable_keys_internals/unexportable_keys_internals_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/unexportable_keys_internals_resources.h"
#include "chrome/grit/unexportable_keys_internals_resources_map.h"
#include "components/unexportable_keys/features.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/webui_util.h"

bool UnexportableKeysInternalsUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(
      unexportable_keys::kUnexportableKeyDeletion);
}

UnexportableKeysInternalsUI::UnexportableKeysInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui),
      chrome::kChromeUIUnexportableKeysInternalsHost);
  webui::SetupWebUIDataSource(
      source, kUnexportableKeysInternalsResources,
      IDR_UNEXPORTABLE_KEYS_INTERNALS_UNEXPORTABLE_KEYS_INTERNALS_HTML);
}

void UnexportableKeysInternalsUI::BindInterface(
    mojo::PendingReceiver<
        unexportable_keys_internals::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void UnexportableKeysInternalsUI::CreateUnexportableKeysInternalsHandler(
    mojo::PendingRemote<unexportable_keys_internals::mojom::Page> page,
    mojo::PendingReceiver<unexportable_keys_internals::mojom::PageHandler>
        receiver) {
  page_handler_ = std::make_unique<UnexportableKeysInternalsHandler>(
      std::move(receiver), std::move(page),
      UnexportableKeyServiceFactory::CreateForGarbageCollection(
          unexportable_keys::GetDefaultConfig()));
}

WEB_UI_CONTROLLER_TYPE_IMPL(UnexportableKeysInternalsUI)

UnexportableKeysInternalsUI::~UnexportableKeysInternalsUI() = default;
