// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engine_utils.h"

#include "components/google/core/common/google_util.h"
#include "components/search_engines/prepopulated_engines.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

namespace SearchEngineUtils {

namespace {

bool SameDomain(const GURL& given_url, const GURL& prepopulated_url) {
  return prepopulated_url.is_valid() &&
         net::registry_controlled_domains::SameDomainOrHost(
             given_url, prepopulated_url,
             net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
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
  for (size_t i = 0; i < TemplateURLPrepopulateData::kAllEnginesLength; ++i) {
    // First check the main search URL.
    if (SameDomain(
            url, GURL(TemplateURLPrepopulateData::kAllEngines[i]->search_url)))
      return TemplateURLPrepopulateData::kAllEngines[i]->type;

    // Then check the alternate URLs.
    for (size_t j = 0;
         j < TemplateURLPrepopulateData::kAllEngines[i]->alternate_urls_size;
         ++j) {
      if (SameDomain(url, GURL(TemplateURLPrepopulateData::kAllEngines[i]
                                   ->alternate_urls[j])))
        return TemplateURLPrepopulateData::kAllEngines[i]->type;
    }
  }

  return SEARCH_ENGINE_OTHER;
}

}  // namespace SearchEngineUtils
