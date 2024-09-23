// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/connectors_internals/connectors_internals_ui.h"

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/connectors_internals/connectors_internals.mojom.h"
#include "chrome/browser/ui/webui/connectors_internals/connectors_internals_page_handler.h"
#include "chrome/browser/ui/webui/connectors_internals/device_trust_utils.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/connectors_internals_resources.h"
#include "chrome/grit/connectors_internals_resources_map.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace enterprise_connectors {

ConnectorsInternalsUI::ConnectorsInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIConnectorsInternalsHost);

  source->AddBoolean("isOtr", profile->IsOffTheRecord());
  source->AddBoolean("canDeleteDeviceTrustKey",
                     utils::CanDeleteDeviceTrustKey());

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kConnectorsInternalsResources,
                      kConnectorsInternalsResourcesSize),
      IDR_CONNECTORS_INTERNALS_INDEX_HTML);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::RequireTrustedTypesFor,
      "require-trusted-types-for 'script';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types static-types;");
}

WEB_UI_CONTROLLER_TYPE_IMPL(ConnectorsInternalsUI)

ConnectorsInternalsUI::~ConnectorsInternalsUI() = default;

void ConnectorsInternalsUI::BindInterface(
    mojo::PendingReceiver<connectors_internals::mojom::PageHandler> receiver) {
  page_handler_ = std::make_unique<ConnectorsInternalsPageHandler>(
      std::move(receiver), Profile::FromWebUI(web_ui()));
}

}  // namespace enterprise_connectors
