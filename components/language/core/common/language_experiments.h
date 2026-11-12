// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CORE_COMMON_LANGUAGE_EXPERIMENTS_H_
#define COMPONENTS_LANGUAGE_CORE_COMMON_LANGUAGE_EXPERIMENTS_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace language {

// This feature uses the existing UI for the Full Page Translate bubble.
BASE_DECLARE_FEATURE(kUseButtonTranslateBubbleUi);

// This feature enables setting the application language on Android.
BASE_DECLARE_FEATURE(kDetailedLanguageSettings);

// This feature enables certain trusted Android apps to open a Chrome Custom Tab
// and cause it to immediately automatically translate into a desired target
// language.
BASE_DECLARE_FEATURE(kCctAutoTranslate);

// This feature enables opening language settings in a new tab when the user
// clicks "Open language settings" in the Translate bubble menu.
BASE_DECLARE_FEATURE(kTranslateOpenSettings);

// When enabled this feature will disable the GeoLanguage model and default the
// user to the fluent language model.
BASE_DECLARE_FEATURE(kDisableGeoLanguageModel);

// When enabled, this feature enables querying ULP languages from GmsCore and
// the creation of the ULP language model.
BASE_DECLARE_FEATURE(kGmsCoreUlp);

// When enabled, this feature will limit the total number of languages for
// Accept-Language to enhance privacy.
BASE_DECLARE_FEATURE(kReduceAcceptLanguageCount);
BASE_DECLARE_FEATURE_PARAM(int, kMaxAcceptLanguage);

// Enables localizing the UI using the grammatical gender from the primary
// Google Account's capabilities.
BASE_DECLARE_FEATURE(kGrammaticalGenderLocalization);

}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CORE_COMMON_LANGUAGE_EXPERIMENTS_H_
