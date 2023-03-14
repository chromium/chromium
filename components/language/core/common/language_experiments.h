// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CORE_COMMON_LANGUAGE_EXPERIMENTS_H_
#define COMPONENTS_LANGUAGE_CORE_COMMON_LANGUAGE_EXPERIMENTS_H_

#include "base/feature_list.h"

namespace language {

// The feature that enables explicitly asking for user preferred
// Accept-Languages on second run on Android. Replaced by kAppLanguagePrompt.
BASE_DECLARE_FEATURE(kExplicitLanguageAsk);

// The feature that enables a second run prompt to select the app UI language on
// Android.
BASE_DECLARE_FEATURE(kAppLanguagePrompt);

// When enabled does not show the AppLanguagePrompt to users whose base UI
// language is their top ULP language.
BASE_DECLARE_FEATURE(kAppLanguagePromptULP);

// This feature forces the app UI prompt even if it has already been shown.
BASE_DECLARE_FEATURE(kForceAppLanguagePrompt);

// This feature uses the existing UI for the Full Page Translate bubble.
BASE_DECLARE_FEATURE(kUseButtonTranslateBubbleUi);

// This feature enables setting the application language on Android.
BASE_DECLARE_FEATURE(kDetailedLanguageSettings);

// This feature enables setting the application language on Desktop.
BASE_DECLARE_FEATURE(kDesktopDetailedLanguageSettings);

// This feature enables providing Translate data to Assistant.
BASE_DECLARE_FEATURE(kTranslateAssistContent);

// This feature enables an intent that starts translating the foreground tab.
BASE_DECLARE_FEATURE(kTranslateIntent);

// This feature enables showing the user's content languages separately at the
// top of the language picker menu in the Translate UI.
BASE_DECLARE_FEATURE(kContentLanguagesInLanguagePicker);
extern const char kContentLanguagesDisableObserversParam[];

// This feature enables certain trusted Android apps to open a Chrome Custom Tab
// and cause it to immediately automatically translate into a desired target
// language.
BASE_DECLARE_FEATURE(kCctAutoTranslate);

}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CORE_COMMON_LANGUAGE_EXPERIMENTS_H_
