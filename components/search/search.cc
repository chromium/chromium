// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search/search.h"

#include "build/build_config.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/search_engine_utils.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace search {

namespace {

bool IsCryptographicOrLocalhost(const GURL& url) {
  return url.is_valid() &&
         (url.SchemeIsCryptographic() || net::IsLocalhost(url));
}

}  // namespace

bool IsInstantExtendedAPIEnabled() {
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
  return false;
#else
  return true;
#endif
}

bool DefaultSearchProviderIsGoogle(
    const TemplateURLService* template_url_service) {
  if (!template_url_service)
    return false;
  return TemplateURLIsGoogle(template_url_service->GetDefaultSearchProvider(),
                             template_url_service->search_terms_data());
}

bool TemplateURLIsGoogle(const TemplateURL* template_url,
                         const SearchTermsData& search_terms_data) {
  if (!template_url ||
      template_url->GetEngineType(search_terms_data) != SEARCH_ENGINE_GOOGLE) {
    return false;
  }

  // The search URL must be valid and use HTTPS (or localhost in tests).
  const GURL search_url = template_url->GenerateSearchURL(search_terms_data);
  if (!IsCryptographicOrLocalhost(search_url)) {
    return false;
  }

  // If a suggestion URL is configured, it must also be valid, use HTTPS (or
  // localhost in tests), and resolve to Google. This prevents engines with a
  // Google search endpoint but an external suggestion endpoint (e.g. spoofed
  // OpenSearch engines) from passing as trusted Google engines and injecting
  // privileged actions or answer templates.
  if (!template_url->suggestions_url().empty()) {
    const GURL suggest_url =
        template_url->GenerateSuggestionURL(search_terms_data);
    if (!IsCryptographicOrLocalhost(suggest_url) ||
        search_engine_utils::GetEngineType(suggest_url) !=
            SEARCH_ENGINE_GOOGLE) {
      return false;
    }
  }

  // When no suggestion URL is specified, trust relies on the verified search
  // URL.
  return true;
}

}  // namespace search
