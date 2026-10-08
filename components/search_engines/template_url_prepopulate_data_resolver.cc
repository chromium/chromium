// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url_prepopulate_data_resolver.h"

#include <optional>
#include <string_view>

#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "components/prefs/pref_service.h"
#include "components/regional_capabilities/regional_capabilities_country_id.h"
#include "components/regional_capabilities/regional_capabilities_service.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/regional_capabilities/regional_capabilities_utils.h"
#include "components/search_engines/keyword_table.h"
#include "components/search_engines/keyword_web_data_service.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "url/gurl.h"

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
      regional_capabilities_->GetRegionalVariants(), prepopulated_id);
}

std::unique_ptr<TemplateURLData> Resolver::GetEngineFromFullList(
    std::u16string_view keyword) const {
  return TemplateURLPrepopulateData::GetPrepopulatedEngineFromFullList(
      profile_prefs_.get(),
      regional_capabilities_->GetRegionalPrepopulatedEngines(),
      regional_capabilities_->GetRegionalVariants(), keyword);
}

std::unique_ptr<TemplateURLData> Resolver::GetFallbackSearch() const {
  return TemplateURLPrepopulateData::GetPrepopulatedFallbackSearch(
      profile_prefs_.get(),
      regional_capabilities_->GetRegionalPrepopulatedEngines());
}

std::optional<BuiltinKeywordsMetadata>
Resolver::ComputeDatabaseUpdateRequirements(
    const WDKeywordsResult::Metadata& keywords_metadata) const {
  KeywordTable::PrepopulatedEngineMigrationSet current_migration_state;
  if (base::FeatureList::IsEnabled(switches::kPrepopulatedEnginesMigration)) {
    current_migration_state.Put(
        KeywordTable::PrepopulatedEngineMigration::kMigration);
  }
  if (switches::ArePrepopulatedEnginesShadowVariantsEnabled()) {
    current_migration_state.Put(
        KeywordTable::PrepopulatedEngineMigration::kShadowVariants);
  }

  BuiltinKeywordsMetadata current_metadata{
      .country_id = regional_capabilities_->GetCountryId(),
      .data_version =
          TemplateURLPrepopulateData::GetDataVersion(&profile_prefs_.get()),
      .prepopulated_engines_migration_state = current_migration_state,
  };

  // Rollback check: if DB has bits set that current doesn't have.
  if (!base::Difference(keywords_metadata.prepopulated_engines_migration_state,
                        current_metadata.prepopulated_engines_migration_state)
           .empty()) {
    // The keywords DB indicates that it was updated with some post-migration
    // data, but the feature state checks indicate that the feature is not
    // enabled. There is no way to fully return to the previous state, so the
    // local keywords data might be inconsistent, where the user could be using
    // a not-yet-listed prepopulated engine.
    //
    // The migration features are not rolled out to users yet, so this is only
    // expected for profiles that got exposed to them through test configs,
    // flags or non-official builds. Report it, then let the database catch up
    // with the current feature state through the regular update below, so
    // that each profile reports this at most once instead of on every startup.
    // TODO(crbug.com/446637115): Once the migration features are rolled out to
    //   users, block again on rollbacks: stop treating the migration state
    //   rollback as a reason to update the database (see the last check
    //   below), so that the post-migration data is preserved. Keep reporting
    //   it, without crashing.
    SCOPED_CRASH_KEY_NUMBER(
        "KwdbRefresh", "db_migration_state",
        keywords_metadata.prepopulated_engines_migration_state.ToEnumBitmask());
    SCOPED_CRASH_KEY_NUMBER(
        "KwdbRefresh", "current_migration_state",
        current_metadata.prepopulated_engines_migration_state.ToEnumBitmask());
    base::debug::DumpWithoutCrashing();
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

  if (keywords_metadata.prepopulated_engines_migration_state !=
      current_metadata.prepopulated_engines_migration_state) {
    // The migration state changed, either because a new migration feature got
    // enabled for this client, or because of a rollback (see above). Update the
    // database so that it reflects the current state.
    return current_metadata;
  }

  return std::nullopt;
}

bool Resolver::IsMatch(MigrationMatch match) {
  switch (match) {
    case MigrationMatch::kExactMatch:
    case MigrationMatch::kHostMatch:
      return true;
    case MigrationMatch::kIdsDontMatch:
    case MigrationMatch::kInvalidCheckedUrl:
    case MigrationMatch::kUrlMismatch:
      return false;
  }
}

Resolver::MigrationMatch Resolver::CompareEngineUnderMigration(
    const TemplateURLData& checked_data,
    const PrepopulatedEngine* deprecated_engine) const {
  CHECK_NE(deprecated_engine->migrate_to_id, 0);

  if (checked_data.prepopulate_id != deprecated_engine->id) {
    return MigrationMatch::kIdsDontMatch;
  }

  // Don't only check the IDs, also check the URLs. The prepopulated
  // engines data defines multiple entries sharing the same `preopulate_id`,
  // but only adds one version to regional engines sets. Checking the URL
  // ensures that the engine being migrated corresponds to the expected
  // regional version.
  if (checked_data.url() == deprecated_engine->search_url) {
    return MigrationMatch::kExactMatch;
  }

  GURL incoming_gurl(checked_data.url());
  if (!incoming_gurl.is_valid()) {
    return MigrationMatch::kInvalidCheckedUrl;
  }

  GURL built_in_gurl(deprecated_engine->search_url);
  // Assumption checked in `TemplateURLPrepopulateDataTest.ValidSearchURLs`
  // May still fail if we end up migrating Google to another prepopulate ID.
  CHECK(built_in_gurl.is_valid());

  if (incoming_gurl.host() == built_in_gurl.host()) {
    return MigrationMatch::kHostMatch;
  }

  return MigrationMatch::kUrlMismatch;
}

std::unique_ptr<TemplateURLData> Resolver::TryGetMigratedEngine(
    const TemplateURLData& pre_migration_engine) const {
  if (!base::FeatureList::IsEnabled(switches::kPrepopulatedEnginesMigration)) {
    return {};
  }

  // Should only be requested for prepopulated engines.
  CHECK_NE(pre_migration_engine.prepopulate_id, 0);

  const auto& migrating_engines =
      regional_capabilities::GetMigratingPrepopulatedEngines();
  for (const auto& [new_engine_id, deprecated_engine] : migrating_engines) {
    MigrationMatch match =
        CompareEngineUnderMigration(pre_migration_engine, deprecated_engine);
    if (match != MigrationMatch::kIdsDontMatch) {
      base::UmaHistogramEnumeration(
          "Omnibox.TemplateUrl.DseReconciler.MigrationMatch", match);
    }
    if (IsMatch(match)) {
      auto new_engine = GetEngineFromFullList(new_engine_id);

      // By design there should be an entry for this ID, see
      // `regional_capabilities::ComputeMigratedEnginesMapping`.
      CHECK(new_engine);

      return new_engine;
    }
  }

  return {};
}

}  // namespace TemplateURLPrepopulateData
