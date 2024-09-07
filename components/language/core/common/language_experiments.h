// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CORE_COMMON_LANGUAGE_EXPERIMENTS_H_
#define COMPONENTS_LANGUAGE_CORE_COMMON_LANGUAGE_EXPERIMENTS_H_

#include "base/feature_list.h"

namespace language {

// This feature uses the existing UI for the Full Page Translate bubble.
BASE_DECLARE_FEATURE(kUseButtonTranslateBubbleUi);

// This feature enables setting the application language on Android.
BASE_DECLARE_FEATURE(kDetailedLanguageSettings);

// This feature enables showing the user's content languages separately at the
// top of the language picker menu in the Translate UI.
BASE_DECLARE_FEATURE(kContentLanguagesInLanguagePicker);
extern const char kContentLanguagesDisableObserversParam[];

// This feature enables certain trusted Android apps to open a Chrome Custom Tab
// and cause it to immediately automatically translate into a desired target
// language.
BASE_DECLARE_FEATURE(kCctAutoTranslate);

// This feature enables opening language settings in a new tab when the user
// clicks "Open language settings" in the Translate bubble menu.
BASE_DECLARE_FEATURE(kTranslateOpenSettings);

}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CORE_COMMON_LANGUAGE_EXPERIMENTS_H_
