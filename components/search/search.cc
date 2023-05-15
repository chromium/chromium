// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search/search.h"

#include "build/build_config.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"

namespace search {

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
  return template_url != nullptr &&
         template_url->GetEngineType(search_terms_data) == SEARCH_ENGINE_GOOGLE;
}

}  // namespace search
