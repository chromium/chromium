// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/common/language_experiments.h"

namespace language {
// Features:
BASE_FEATURE(kDetailedLanguageSettings, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCctAutoTranslate,
             "CCTAutoTranslate",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDisableGeoLanguageModel, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGmsCoreUlp, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled this feature will limit the total number of languages for
// Accept-Language to enhance privacy. See details in
// https://datatracker.ietf.org/doc/html/rfc7231#section-9.7.
BASE_FEATURE(kReduceAcceptLanguageCount, base::FEATURE_ENABLED_BY_DEFAULT);

// When non-zero, the number of Accept-Language of this size will return as the
// HTTP header and JavaScript getter.
BASE_FEATURE_PARAM(int,
                   kMaxAcceptLanguage,
                   &kReduceAcceptLanguageCount,
                   /*name=*/"MaxAcceptLanguage",
                   /*default_value=*/10);

BASE_FEATURE(kGrammaticalGenderLocalization, base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace language
