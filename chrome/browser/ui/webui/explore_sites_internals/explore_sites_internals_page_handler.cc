// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/webui/explore_sites_internals/explore_sites_internals_page_handler.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/android/explore_sites/explore_sites_feature.h"
#include "chrome/browser/android/explore_sites/url_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/explore_sites_internals/explore_sites_internals.mojom.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace explore_sites {
using chrome::android::explore_sites::ExploreSitesVariation;
using chrome::android::explore_sites::GetExploreSitesVariation;

namespace {

std::string GetChromeFlagsSetupString() {
  switch (GetExploreSitesVariation()) {
    case ExploreSitesVariation::ENABLED:
      return "Enabled";
    case ExploreSitesVariation::EXPERIMENT:
      return "Experiment";
    case ExploreSitesVariation::PERSONALIZED:
      return "Personalized";
    case ExploreSitesVariation::MOST_LIKELY:
      return "Most Likely";
    case ExploreSitesVariation::DISABLED:
      return "Disabled";
  }
}
}  // namespace

ExploreSitesInternalsPageHandler::ExploreSitesInternalsPageHandler(
    mojo::PendingReceiver<explore_sites_internals::mojom::PageHandler> receiver,
    ExploreSitesService* explore_sites_service,
    Profile* profile)
    : receiver_(this, std::move(receiver)),
      explore_sites_service_(explore_sites_service),
      profile_(profile) {}

ExploreSitesInternalsPageHandler::~ExploreSitesInternalsPageHandler() {}

void ExploreSitesInternalsPageHandler::GetProperties(
    GetPropertiesCallback callback) {
  base::flat_map<std::string, std::string> properties;
  properties["chrome-flags-setup"] = GetChromeFlagsSetupString();
  properties["server-endpoint"] = GetCatalogURL().spec();
  properties["country-code"] = explore_sites_service_->GetCountryCode();
  std::move(callback).Run(properties);
}

void ExploreSitesInternalsPageHandler::ClearCachedExploreSitesCatalog(
    ClearCachedExploreSitesCatalogCallback callback) {
  if (ExploreSitesVariation::DISABLED == GetExploreSitesVariation()) {
    std::move(callback).Run(false);
    return;
  }

  explore_sites_service_->ClearCachedCatalogsForDebugging();
  std::move(callback).Run(true);
}

void ExploreSitesInternalsPageHandler::ForceNetworkRequest(
    ForceNetworkRequestCallback callback) {
  explore_sites_service_->UpdateCatalogFromNetwork(
      true /* is_immediate_fetch */,
      profile_->GetPrefs()->GetString(language::prefs::kAcceptLanguages),
      std::move(callback));
}

void ExploreSitesInternalsPageHandler::OverrideCountryCode(
    const std::string& country_code,
    OverrideCountryCodeCallback callback) {
  if (ExploreSitesVariation::DISABLED == GetExploreSitesVariation()) {
    std::move(callback).Run(false);
    return;
  }

  explore_sites_service_->OverrideCountryCodeForDebugging(country_code);
  std::move(callback).Run(true);
}

}  // namespace explore_sites
