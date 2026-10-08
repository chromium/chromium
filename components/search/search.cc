// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search/search.h"

#include "base/feature_list.h"
#include "build/android_buildflags.h"
#include "build/build_config.h"
#include "components/search/ntp_features.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"

namespace search {

bool IsInstantExtendedAPIEnabled() {
// See exception on b/532190074 for WebUI NTP on Desktop Android.
#if BUILDFLAG(IS_DESKTOP_ANDROID)
  return base::FeatureList::IsEnabled(
      ntp_features::kNtpEnableInstantApiAndroid);
#elif BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
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
  return template_url && template_url->IsTrustedGoogleEngine(search_terms_data);
}

}  // namespace search
