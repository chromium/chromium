// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/common/language_experiments.h"

#include <map>
#include <string>

#include "build/build_config.h"

namespace language {
// Features:
BASE_FEATURE(kDetailedLanguageSettings,
             "DetailedLanguageSettings",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kContentLanguagesInLanguagePicker,
             "ContentLanguagesInLanguagePicker",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCctAutoTranslate,
             "CCTAutoTranslate",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTranslateOpenSettings,
             "TranslateOpenSettings",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Params:
const char kContentLanguagesDisableObserversParam[] = "disable_observers";

}  // namespace language
