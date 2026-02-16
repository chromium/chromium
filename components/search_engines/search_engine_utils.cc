// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engine_utils.h"

#include "base/not_fatal_until.h"
#include "components/google/core/common/google_util.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/regional_capabilities/regional_capabilities_utils.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/search_engines_data/resources/definitions/prepopulated_engines.h"
#include "url/gurl.h"

namespace search_engine_utils {

namespace {

using ::TemplateURLPrepopulateData::PrepopulatedEngine;

bool SameDomain(const GURL& given_url, const GURL& prepopulated_url) {
  return prepopulated_url.is_valid() &&
         net::registry_controlled_domains::SameDomainOrHost(
             given_url, prepopulated_url,
             net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

bool HasSameDomainUrl(const GURL& given_url,
                      const PrepopulatedEngine* prepopulated_engine) {
  if (SameDomain(given_url, GURL(prepopulated_engine->search_url))) {
    return true;
  }

  return std::ranges::any_of(prepopulated_engine->alternate_urls,
                             [&](const auto* alt_url) {
                               return SameDomain(given_url, GURL(alt_url));
                             });
}

}  // namespace

// Global functions -----------------------------------------------------------

SearchEngineType GetEngineType(const GURL& url) {
  DCHECK(url.is_valid());

  // Check using TLD+1s, in order to more aggressively match search engine types
  // for data imported from other browsers.
  //
  // First special-case Google, because the prepopulate URL for it will not
  // convert to a GURL and thus won't have an origin.  Instead see if the
  // incoming URL's host is "[*.]google.<TLD>".
  if (google_util::IsGoogleDomainUrl(url, google_util::DISALLOW_SUBDOMAIN,
                                     google_util::ALLOW_NON_STANDARD_PORTS))
    return TemplateURLPrepopulateData::google.type;

  // Now check the rest of the prepopulate data.
  const auto& all_engines = regional_capabilities::GetAllPrepopulatedEngines();

  auto it = std::ranges::find_if(all_engines, [&](const auto* engine) {
    return HasSameDomainUrl(url, engine);
  });
  if (it == all_engines.end()) {
    return SEARCH_ENGINE_OTHER;
  }

  const PrepopulatedEngine* matched_engine = *it;

  // Check whether the migration is needed.
  if (!matched_engine->migrate_to_id ||
      !base::FeatureList::IsEnabled(switches::kPrepopulatedEnginesMigration)) {
    return matched_engine->type;
  }

  // Apply the migration
  auto found_migrated_engine_it = std::ranges::find(
      all_engines, matched_engine->migrate_to_id, &PrepopulatedEngine::id);
  // Enforced in data pipeline checks and on startup by a CHECK in
  // `regional_capabilities::ComputeMigratedEnginesMapping`
  CHECK(found_migrated_engine_it != all_engines.end(),
        base::NotFatalUntil::M149);
  return (*found_migrated_engine_it)->type;
}

}  // namespace search_engine_utils
