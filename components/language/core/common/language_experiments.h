// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CORE_COMMON_LANGUAGE_EXPERIMENTS_H_
#define COMPONENTS_LANGUAGE_CORE_COMMON_LANGUAGE_EXPERIMENTS_H_

#include "base/feature_list.h"

namespace language {

// The feature that enables explicitly asking for user preferred
// Accept-Languages on second run on Android. Replaced by kAppLanguagePrompt.
extern const base::Feature kExplicitLanguageAsk;
// The feature that enables a second run prompt to select the app UI language on
// Android.
extern const base::Feature kAppLanguagePrompt;
// When enabled does not show the AppLanguagePrompt to users whose base UI
// language is their top ULP language.
extern const base::Feature kAppLanguagePromptULP;
// This feature forces the app UI prompt even if it has already been shown.
extern const base::Feature kForceAppLanguagePrompt;

// This feature controls the activation of the experiment to trigger Translate
// in India on English pages independent of the user's UI language. The params
// associated with the experiment dictate which model is used to determine the
// target language.
extern const base::Feature kOverrideTranslateTriggerInIndia;
extern const char kOverrideModelKey[];
extern const char kEnforceRankerKey[];
extern const char kOverrideModelGeoValue[];
extern const char kOverrideModelDefaultValue[];
extern const char kBackoffThresholdKey[];
extern const char kContentLanguagesDisableObserversParam[];

// Notify sync to update data on language determined.
extern const base::Feature kNotifySyncOnLanguageDetermined;

// This feature uses the existing UI for translate bubble.
extern const base::Feature kUseButtonTranslateBubbleUi;

// This feature enables setting the application language on Android.
extern const base::Feature kDetailedLanguageSettings;

// This feature enables the desktop version's redesigned language settings
// layout.
extern const base::Feature kDesktopRestructuredLanguageSettings;

// This feature enables setting the application language on Desktop.
extern const base::Feature kDesktopDetailedLanguageSettings;

// This feature enables providing Translate data to Assistant.
extern const base::Feature kTranslateAssistContent;

// This feature enables an intent that starts translating the foreground tab.
extern const base::Feature kTranslateIntent;

// This feature enables an intent that starts translating the foreground tab.
extern const base::Feature kContentLanguagesInLanguagePicker;

// This feature enables use of ULP language data in Chrome.
extern const base::Feature kUseULPLanguagesInChrome;

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
