// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_PREPOPULATE_DATA_RESOLVER_H_
#define COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_PREPOPULATE_DATA_RESOLVER_H_

#include <vector>

#include "base/memory/raw_ref.h"
#include "components/keyed_service/core/keyed_service.h"

struct TemplateURLData;
class PrefService;

namespace regional_capabilities {
class RegionalCapabilitiesService;
}

namespace TemplateURLPrepopulateData {

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

 private:
  raw_ref<PrefService> profile_prefs_;
  raw_ref<regional_capabilities::RegionalCapabilitiesService>
      regional_capabilities_;
};

}  // namespace TemplateURLPrepopulateData

#endif  // COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_PREPOPULATE_DATA_RESOLVER_H_
