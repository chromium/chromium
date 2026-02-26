// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/private_ai_internals/private_ai_internals_ui.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "chrome/browser/private_ai/private_ai_service.h"
#include "chrome/browser/private_ai/private_ai_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/private_ai_internals/private_ai_internals.mojom.h"
#include "chrome/browser/ui/webui/private_ai_internals/private_ai_internals_page_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/private_ai_internals_resources.h"
#include "chrome/grit/private_ai_internals_resources_map.h"
#include "components/private_ai/features.h"
#include "components/private_ai/proto/private_ai.pb.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/internal_webui_config.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/webui_util.h"

PrivateAiInternalsUIConfig::PrivateAiInternalsUIConfig()
    : DefaultInternalWebUIConfig(chrome::kChromeUIPrivateAiInternalsHost) {}

PrivateAiInternalsUIConfig::~PrivateAiInternalsUIConfig() = default;

bool PrivateAiInternalsUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  if (!base::FeatureList::IsEnabled(private_ai::kPrivateAi)) {
    return false;
  }

  Profile* profile = Profile::FromBrowserContext(browser_context);
  auto* private_ai_service =
      private_ai::PrivateAiServiceFactory::GetForProfile(profile);
  return private_ai_service && private_ai_service->GetTokenManager();
}

PrivateAiInternalsUI::PrivateAiInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);

  // Set up the chrome://private-ai-internals source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIPrivateAiInternalsHost);

  webui::SetupWebUIDataSource(
      source, base::span(kPrivateAiInternalsResources),
      IDR_PRIVATE_AI_INTERNALS_PRIVATE_AI_INTERNALS_HTML);

  source->AddString("default_url", private_ai::kPrivateAiUrl.Get());
  source->AddString("default_api_key", private_ai::kPrivateAiApiKey.Get());
  source->AddString("default_proxy_url",
                    private_ai::kPrivateAiProxyServerUrl.Get());
  source->AddString("default_feature_name",
                    private_ai::proto::FeatureName_Name(
                        private_ai::proto::FeatureName::
                            FEATURE_NAME_DEMO_GEMINI_GENERATE_CONTENT));
  source->AddBoolean(
      "default_use_token_attestation",
      base::FeatureList::IsEnabled(private_ai::kPrivateAiUseTokenAttestation));
}

PrivateAiInternalsUI::~PrivateAiInternalsUI() = default;

void PrivateAiInternalsUI::BindInterface(
    mojo::PendingReceiver<
        private_ai_internals::mojom::PrivateAiInternalsPageHandler> receiver) {
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
  page_handler_ = std::make_unique<PrivateAiInternalsPageHandler>(
      token_manager, network_context, private_ai_service->GetClient(),
      std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(PrivateAiInternalsUI)
