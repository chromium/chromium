// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/regional_capabilities_internals/regional_capabilities_internals_ui.h"

#include "base/check_deref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/regional_capabilities/regional_capabilities_service_factory.h"
#include "components/grit/regional_capabilities_internals_resources.h"
#include "components/grit/regional_capabilities_internals_resources_map.h"
#include "components/regional_capabilities/access/country_access_reason.h"
#include "components/regional_capabilities/regional_capabilities_internals_data_holder.h"
#include "components/regional_capabilities/regional_capabilities_service.h"
#include "components/webui/regional_capabilities_internals/constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

using regional_capabilities::CountryAccessKey;
using regional_capabilities::CountryAccessReason;
using regional_capabilities::RegionalCapabilitiesServiceFactory;

RegionalCapabilitiesInternalsUI::RegionalCapabilitiesInternalsUI(
    content::WebUI* web_ui,
    const GURL& url)
    : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile,
      regional_capabilities::kChromeUIRegionalCapabilitiesInternalsHost);

  regional_capabilities::InternalsDataHolder internals_data_holder =
      RegionalCapabilitiesServiceFactory::GetForProfile(profile)
          ->GetInternalsData();
  for (const auto& [key, value] :
       internals_data_holder.GetRestricted(CountryAccessKey(
           CountryAccessReason::
               kRegionalCapabilitiesInternalsDisplayInDebugUi))) {
    source->AddString(key, value);
  }

  webui::SetupWebUIDataSource(source, kRegionalCapabilitiesInternalsResources,
                              IDR_REGIONAL_CAPABILITIES_INTERNALS_INDEX_HTML);
}
