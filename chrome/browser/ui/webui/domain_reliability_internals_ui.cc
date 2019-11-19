// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/domain_reliability_internals_ui.h"

#include <string>

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/network_context.mojom.h"

DomainReliabilityInternalsUI::DomainReliabilityInternalsUI(
    content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  content::WebUIDataSource* html_source = content::WebUIDataSource::Create(
      chrome::kChromeUIDomainReliabilityInternalsHost);
  html_source->OverrideContentSecurityPolicyScriptSrc(
      "script-src chrome://resources 'self' 'unsafe-eval';");
  html_source->AddResourcePath("domain_reliability_internals.css",
      IDR_DOMAIN_RELIABILITY_INTERNALS_CSS);
  html_source->AddResourcePath("domain_reliability_internals.js",
      IDR_DOMAIN_RELIABILITY_INTERNALS_JS);
  html_source->SetDefaultResource(IDR_DOMAIN_RELIABILITY_INTERNALS_HTML);

  web_ui->RegisterMessageCallback(
      "updateData",
      base::BindRepeating(&DomainReliabilityInternalsUI::UpdateData,
                          base::Unretained(this)));

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, html_source);
}

DomainReliabilityInternalsUI::~DomainReliabilityInternalsUI() {}

void DomainReliabilityInternalsUI::UpdateData(const base::ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  network::mojom::NetworkContext* network_context =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetNetworkContext();
  network_context->GetDomainReliabilityJSON(
      base::BindOnce(&DomainReliabilityInternalsUI::OnDataUpdated,
                     weak_factory_.GetWeakPtr()));
}

void DomainReliabilityInternalsUI::OnDataUpdated(base::Value data) const {
  web_ui()->CallJavascriptFunctionUnsafe(
      "DomainReliabilityInternals.onDataUpdated", data);
}
