// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/dlp_internals/dlp_internals_ui.h"

#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/dlp_internals_resources.h"
#include "chrome/grit/dlp_internals_resources_map.h"
#include "content/public/browser/web_ui_data_source.h"

namespace policy {

DlpInternalsUI::DlpInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIDlpInternalsHost);

  source->AddBoolean("isOtr", profile->IsOffTheRecord());
  DlpRulesManager* rules_manager =
      DlpRulesManagerFactory::GetForPrimaryProfile();
  source->AddBoolean("doRulesManagerExist", rules_manager != nullptr);
  source->AddBoolean(
      "isReportingEnabled",
      rules_manager ? rules_manager->IsReportingEnabled() : false);

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kDlpInternalsResources, kDlpInternalsResourcesSize),
      IDR_DLP_INTERNALS_INDEX_HTML);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::RequireTrustedTypesFor,
      "require-trusted-types-for 'script';");
}

WEB_UI_CONTROLLER_TYPE_IMPL(DlpInternalsUI)

DlpInternalsUI::~DlpInternalsUI() = default;

void DlpInternalsUI::BindInterface(
    mojo::PendingReceiver<dlp_internals::mojom::PageHandler> receiver) {
  page_handler_ = std::make_unique<DlpInternalsPageHandler>(
      std::move(receiver), Profile::FromWebUI(web_ui()));
}

}  // namespace policy
