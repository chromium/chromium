// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/common/language_util.h"

#include "base/feature_list.h"
#include "components/country_codes/country_codes.h"
#include "components/language/core/common/language_experiments.h"

namespace language {

bool OverrideTranslateTriggerInIndia() {
#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(language::kDisableGeoLanguageModel)) {
    return false;
  }
  return country_codes::GetCurrentCountryID().CountryCode() == "IN";
#else
  return false;
#endif
}

OverrideLanguageModel GetOverrideLanguageModel() {
  // Note: when there are multiple possible override models, the overrides
  // ordering is important as it allows us to have concurrent overrides in
  // experiment without having to partition them explicitly.
  if (OverrideTranslateTriggerInIndia()) {
    return OverrideLanguageModel::GEO;
  }

  return OverrideLanguageModel::DEFAULT;
}

}  // namespace language
