// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CORE_COMMON_LANGUAGE_EXPERIMENTS_H_
#define COMPONENTS_LANGUAGE_CORE_COMMON_LANGUAGE_EXPERIMENTS_H_

#include "base/feature_list.h"

namespace language {

// The feature that enables the heuristic model of user language. If disabled,
// the baseline model is used instead.
extern const base::Feature kUseHeuristicLanguageModel;

// The feature that enables explicitly asking for user preferences on startup on
// Android.
extern const base::Feature kExplicitLanguageAsk;

// This feature controls the activation of the experiment to trigger Translate
// in India on English pages independent of the user's UI language. The params
// associated with the experiment dictate which model is used to determine the
// target language. This can in turn be overriden by the Heuristic Model
// experiment.
extern const base::Feature kOverrideTranslateTriggerInIndia;
extern const char kOverrideModelKey[];
extern const char kEnforceRankerKey[];
extern const char kOverrideModelHeuristicValue[];
extern const char kOverrideModelGeoValue[];
extern const char kOverrideModelDefaultValue[];
extern const char kBackoffThresholdKey[];

// Notify sync to update data on language determined.
extern const base::Feature kNotifySyncOnLanguageDetermined;

// This feature uses the existing UI for translate bubble.
extern const base::Feature kUseButtonTranslateBubbleUi;

// These feature params controls what translate bubble UI to display.
extern const char kTranslateUIBubbleKey[];
extern const char kTranslateUIBubbleButtonValue[];
extern const char kTranslateUIBubbleTabValue[];
extern const char kTranslateUIBubbleButtonGM2Value[];

enum class OverrideLanguageModel {
  DEFAULT,
  FLUENT,
  HEURISTIC,
  GEO,
};

// Options for the translate desktop UI experiment.
enum class TranslateUIBubbleModel {
  DEFAULT,
  BUTTON,
  TAB,
  BUTTON_GM2,
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

// Returns which translate bubble UI to use depending on selection in
// kTranslateUIBubbleKey.
TranslateUIBubbleModel GetTranslateUiBubbleModel();
}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CORE_COMMON_LANGUAGE_EXPERIMENTS_H_
