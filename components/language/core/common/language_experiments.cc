// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/common/language_experiments.h"

#include <map>
#include <string>

#include "build/build_config.h"

namespace language {
// Features:
BASE_FEATURE(kExplicitLanguageAsk,
             "ExplicitLanguageAsk",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAppLanguagePrompt,
             "AppLanguagePrompt",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAppLanguagePromptULP,
             "AppLanguagePromptULP",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kForceAppLanguagePrompt,
             "ForceAppLanguagePrompt",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kDetailedLanguageSettings,
             "DetailedLanguageSettings",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kDesktopDetailedLanguageSettings,
             "DesktopDetailedLanguageSettings",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTranslateAssistContent,
             "TranslateAssistContent",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTranslateIntent,
             "TranslateIntent",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kContentLanguagesInLanguagePicker,
             "ContentLanguagesInLanguagePicker",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kCctAutoTranslate,
             "CCTAutoTranslate",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Params:
const char kContentLanguagesDisableObserversParam[] = "disable_observers";

}  // namespace language
