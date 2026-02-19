// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/legion_internals/legion_internals_ui.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "chrome/browser/private_ai/private_ai_service.h"
#include "chrome/browser/private_ai/private_ai_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/legion_internals/legion_internals.mojom.h"
#include "chrome/browser/ui/webui/legion_internals/legion_internals_page_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/legion_internals_resources.h"
#include "chrome/grit/legion_internals_resources_map.h"
#include "components/private_ai/features.h"
#include "components/private_ai/proto/legion.pb.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/internal_webui_config.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/webui_util.h"

LegionInternalsUIConfig::LegionInternalsUIConfig()
    : DefaultInternalWebUIConfig(chrome::kChromeUILegionInternalsHost) {}

LegionInternalsUIConfig::~LegionInternalsUIConfig() = default;

bool LegionInternalsUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  if (!base::FeatureList::IsEnabled(private_ai::kLegion)) {
    return false;
  }

  Profile* profile = Profile::FromBrowserContext(browser_context);
  auto* private_ai_service =
      private_ai::PrivateAiServiceFactory::GetForProfile(profile);
  return private_ai_service && private_ai_service->GetTokenManager();
}

LegionInternalsUI::LegionInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);

  // Set up the chrome://legion-internals source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUILegionInternalsHost);

  webui::SetupWebUIDataSource(source, base::span(kLegionInternalsResources),
                              IDR_LEGION_INTERNALS_LEGION_INTERNALS_HTML);

  source->AddString("default_url", private_ai::kLegionUrl.Get());
  source->AddString("default_api_key", private_ai::kLegionApiKey.Get());
  source->AddString("default_feature_name",
                    private_ai::proto::FeatureName_Name(
                        private_ai::proto::FeatureName::
                            FEATURE_NAME_DEMO_GEMINI_GENERATE_CONTENT));
}

LegionInternalsUI::~LegionInternalsUI() = default;

void LegionInternalsUI::BindInterface(
    mojo::PendingReceiver<legion_internals::mojom::LegionInternalsPageHandler>
        receiver) {
  Profile* profile = Profile::FromWebUI(web_ui());
  auto* private_ai_service =
      private_ai::PrivateAiServiceFactory::GetForProfile(profile);

  // IsWebUIEnabled should have prevented this from being created if the token
  // service or manager is not available.
  CHECK(private_ai_service);
  auto* token_manager = private_ai_service->GetTokenManager();
  CHECK(token_manager);

  auto* network_context =
      profile->GetDefaultStoragePartition()->GetNetworkContext();
  page_handler_ = std::make_unique<LegionInternalsPageHandler>(
      token_manager, network_context, private_ai_service->GetClient(),
      std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(LegionInternalsUI)
