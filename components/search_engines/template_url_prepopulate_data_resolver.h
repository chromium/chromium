// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_PREPOPULATE_DATA_RESOLVER_H_
#define COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_PREPOPULATE_DATA_RESOLVER_H_

#include <optional>
#include <vector>

#include "base/memory/raw_ref.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/regional_capabilities/regional_capabilities_country_id.h"
#include "components/search_engines/keyword_web_data_service.h"

struct TemplateURLData;
class PrefService;

namespace regional_capabilities {
class RegionalCapabilitiesService;
}

namespace TemplateURLPrepopulateData {

// Provides context on TemplateURLPrepopulateData's data set.
struct BuiltinKeywordsMetadata {
  // Country for which we are selecting the built-in prepopulate data.
  regional_capabilities::CountryIdHolder country_id;

  // Version of the built-in prepopulated keywords data.
  int data_version;
};

// Resolves prepopulated engines using on various information from the browser
// context, like country, state of some prefs, etc.
// Use this service instead directly calling functions from
// `TemplateURLPrepopulateData`.
class Resolver : public KeyedService {
 public:
  Resolver(PrefService& prefs,
           regional_capabilities::RegionalCapabilitiesService&
               regional_capabilities);

  // Returns the prepopulated URLs for the profile country.
  std::vector<std::unique_ptr<TemplateURLData>> GetPrepopulatedEngines() const;

  // Returns the prepopulated search engine with the given `prepopulated_id`
  // from the profile country's known prepopulated search engines, or `nullptr`
  // if it's not known there.
  std::unique_ptr<TemplateURLData> GetPrepopulatedEngine(
      int prepopulated_id) const;

  // Returns the prepopulated search engine with the given `prepopulated_id`
  // from the full list of known prepopulated search engines, or `nullptr` if
  // it's not known there.
  std::unique_ptr<TemplateURLData> GetEngineFromFullList(
      int prepopulated_id) const;

  // Returns the fallback default search provider, currently hardcoded to be
  // Google, or whichever one is the first of the list if Google is not in the
  // list of prepopulated search engines.
  // May return `nullptr` if for some reason there are no prepopulated search
  // engines available.
  std::unique_ptr<TemplateURLData> GetFallbackSearch() const;

  // Computes whether updates relative to prepopulated search engines need to be
  // made in the local search engines database.
  //
  // Returns `std::nullopt` when no updates are needed, or a `Metadata`
  // providing country and data version info about the data to be merged in.
  std::optional<BuiltinKeywordsMetadata> ComputeDatabaseUpdateRequirements(
      const WDKeywordsResult::Metadata& keywords_database_metadata) const;

 private:
  raw_ref<PrefService> profile_prefs_;
  raw_ref<regional_capabilities::RegionalCapabilitiesService>
      regional_capabilities_;
};

}  // namespace TemplateURLPrepopulateData

#endif  // COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_PREPOPULATE_DATA_RESOLVER_H_
