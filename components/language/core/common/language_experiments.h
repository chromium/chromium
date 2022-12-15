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

// This feature controls the activation of the experiment to trigger Translate
// in India on English pages independent of the user's UI language. The params
// associated with the experiment dictate which model is used to determine the
// target language.
BASE_DECLARE_FEATURE(kOverrideTranslateTriggerInIndia);
extern const char kOverrideModelKey[];
extern const char kEnforceRankerKey[];
extern const char kOverrideModelGeoValue[];
extern const char kOverrideModelDefaultValue[];
extern const char kBackoffThresholdKey[];
extern const char kContentLanguagesDisableObserversParam[];

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

// This feature enables certain trusted Android apps to open a Chrome Custom Tab
// and cause it to immediately automatically translate into a desired target
// language.
BASE_DECLARE_FEATURE(kCctAutoTranslate);

enum class OverrideLanguageModel {
  DEFAULT,
  GEO,
};

// Returns which language model to use depending on the state of all Language
// experiments.
OverrideLanguageModel GetOverrideLanguageModel();

// Returns true if kOverrideTranslateTriggerInIndia is enabled, false otherwise.
// It should be interpreted as a signal to trigger translate UI on English
// pages, even when the UI language is English. This function also records
// whether the backoff threshold was reached in UMA.
bool ShouldForceTriggerTranslateOnEnglishPages(int force_trigger_count);

// Returns true if kOverrideTranslateTriggerInIndia is enabled and the current
// experiment group specifies the param to enforce Ranker decisions, false
// otherwise.
bool ShouldPreventRankerEnforcementInIndia(int force_trigger_count);

// Returns true if the user ignored or dismissed a prompt that was displayed
// because of kOverrideTranslateTriggerInIndia often enough that the experiment
// should stop being taken into account.
bool IsForceTriggerBackoffThresholdReached(int force_trigger_count);
}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CORE_COMMON_LANGUAGE_EXPERIMENTS_H_
