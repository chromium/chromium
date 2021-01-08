// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/domain_reliability_internals_ui.h"

#include <string>

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/dev_ui_browser_resources.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"

DomainReliabilityInternalsUI::DomainReliabilityInternalsUI(
    content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  content::WebUIDataSource* html_source = content::WebUIDataSource::Create(
      chrome::kChromeUIDomainReliabilityInternalsHost);
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources 'self' 'unsafe-eval';");
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types jstemplate;");
  html_source->AddResourcePath("domain_reliability_internals.css",
      IDR_DOMAIN_RELIABILITY_INTERNALS_CSS);
  html_source->AddResourcePath("domain_reliability_internals.js",
      IDR_DOMAIN_RELIABILITY_INTERNALS_JS);
  html_source->SetDefaultResource(IDR_DOMAIN_RELIABILITY_INTERNALS_HTML);
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, html_source);

  web_ui->AddMessageHandler(
      std::make_unique<DomainReliabilityInternalsHandler>());
}

DomainReliabilityInternalsUI::~DomainReliabilityInternalsUI() = default;

DomainReliabilityInternalsHandler::DomainReliabilityInternalsHandler() =
    default;
DomainReliabilityInternalsHandler::~DomainReliabilityInternalsHandler() =
    default;

void DomainReliabilityInternalsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "updateData",
      base::BindRepeating(&DomainReliabilityInternalsHandler::HandleUpdateData,
                          base::Unretained(this)));
}

void DomainReliabilityInternalsHandler::HandleUpdateData(
    const base::ListValue* args) {
  DCHECK_EQ(1u, args->GetSize());
  AllowJavascript();
  callback_id_ = args->GetList()[0].GetString();

  Profile* profile = Profile::FromWebUI(web_ui());
  network::mojom::NetworkContext* network_context =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetNetworkContext();
  network_context->GetDomainReliabilityJSON(
      base::BindOnce(&DomainReliabilityInternalsHandler::OnDataUpdated,
                     weak_factory_.GetWeakPtr()));
}

void DomainReliabilityInternalsHandler::OnDataUpdated(base::Value data) {
  ResolveJavascriptCallback(base::Value(std::move(callback_id_)), data);
  callback_id_.clear();
}
