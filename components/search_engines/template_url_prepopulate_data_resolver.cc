// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url_prepopulate_data_resolver.h"

#include <optional>

#include "base/logging.h"
#include "base/notreached.h"
#include "components/prefs/pref_service.h"
#include "components/regional_capabilities/regional_capabilities_country_id.h"
#include "components/regional_capabilities/regional_capabilities_service.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/regional_capabilities/regional_capabilities_utils.h"
#include "components/search_engines/keyword_web_data_service.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_prepopulate_data.h"

namespace TemplateURLPrepopulateData {

Resolver::Resolver(
    PrefService& prefs,
    regional_capabilities::RegionalCapabilitiesService& regional_capabilities)
    : profile_prefs_(prefs), regional_capabilities_(regional_capabilities) {}

std::vector<std::unique_ptr<TemplateURLData>> Resolver::GetPrepopulatedEngines()
    const {
  return TemplateURLPrepopulateData::GetPrepopulatedEngines(
      profile_prefs_.get(),
      regional_capabilities_->GetRegionalPrepopulatedEngines());
}

std::unique_ptr<TemplateURLData> Resolver::GetPrepopulatedEngine(
    int prepopulated_id) const {
  return TemplateURLPrepopulateData::GetPrepopulatedEngine(
      profile_prefs_.get(),
      regional_capabilities_->GetRegionalPrepopulatedEngines(),
      prepopulated_id);
}

std::unique_ptr<TemplateURLData> Resolver::GetEngineFromFullList(
    int prepopulated_id) const {
  return TemplateURLPrepopulateData::GetPrepopulatedEngineFromFullList(
      profile_prefs_.get(),
      regional_capabilities_->GetRegionalPrepopulatedEngines(),
      prepopulated_id);
}

std::unique_ptr<TemplateURLData> Resolver::GetFallbackSearch() const {
  return TemplateURLPrepopulateData::GetPrepopulatedFallbackSearch(
      profile_prefs_.get(),
      regional_capabilities_->GetRegionalPrepopulatedEngines());
}

std::optional<BuiltinKeywordsMetadata>
Resolver::ComputeDatabaseUpdateRequirements(
    const WDKeywordsResult::Metadata& keywords_metadata) const {
  BuiltinKeywordsMetadata current_metadata{
      .country_id = regional_capabilities_->GetCountryId(),
      .data_version =
          TemplateURLPrepopulateData::GetDataVersion(&profile_prefs_.get()),
      // Always keep track of the fact that some migration may have taken place.
      .prepopulated_engines_migration_enabled = base::FeatureList::IsEnabled(
          switches::kPrepopulatedEnginesMigration)};

  if (keywords_metadata.prepopulated_engines_migration_enabled &&
      !current_metadata.prepopulated_engines_migration_enabled) {
    // The keywords DB indicates that it was updated with some post-migration
    // data, but the feature state checks indicates that the feature is not
    // enabled.
    // Per feature rollout planning, this should not happen, as there is no way
    // to fully return to the previous state. The local keywords data is then
    // going to be in an inconsistent state, where the user could be using a
    // not-yet-listed prepopulated engine.
    // This is a bad™ state, but not to the point where it is expected to cause
    // crashes downstream, it it can be a non-fatal CHECK.
    // TODO(crbug.com/446637115): Clean up once the rollout is done.
    DUMP_WILL_BE_NOTREACHED();
  }

  if (regional_capabilities::HasSearchEngineCountryListOverride()) {
    // The search engine list is being explicitly overridden, so also force
    // recomputing it for the keywords database.
    return current_metadata;
  }

  if (keywords_metadata.builtin_keyword_data_version >
      current_metadata.data_version) {
    // The version in the database is more recent than the version in the Chrome
    // binary. Downgrades are not supported, so don't update it.
    return std::nullopt;
  }

  if (keywords_metadata.builtin_keyword_data_version <
      current_metadata.data_version) {
    // The built-in data from `prepopulated_engines.json` has been updated.
    return current_metadata;
  }

  if (!keywords_metadata.builtin_keyword_country.has_value() ||
      keywords_metadata.builtin_keyword_country.value() !=
          current_metadata.country_id) {
    // The country associated with the profile has changed.
    return current_metadata;
  }

  if (!keywords_metadata.prepopulated_engines_migration_enabled &&
      current_metadata.prepopulated_engines_migration_enabled) {
    // Ensure that when we enable the migration feature for this client, the
    // database gets updated. Continue and deliberately not do a migration if
    // the divergence is the other way.
    return current_metadata;
  }

  return std::nullopt;
}

}  // namespace TemplateURLPrepopulateData
