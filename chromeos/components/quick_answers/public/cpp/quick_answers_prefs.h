// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_PUBLIC_CPP_QUICK_ANSWERS_PREFS_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_PUBLIC_CPP_QUICK_ANSWERS_PREFS_H_

class PrefRegistrySimple;

namespace quick_answers {
namespace prefs {

// The status of the user's consent. The enum values cannot be changed because
// they are persisted on disk.
enum ConsentStatus {
  // The status is unknown.
  kUnknown = 0,

  // The user has accepted the Quick Answers consent impression.
  kAccepted = 1,

  // The user has rejected the Quick Answers consent impression.
  kRejected = 2,
};

extern const char kQuickAnswersNoticed[];
extern const char kQuickAnswersEnabled[];
extern const char kQuickAnswersConsentStatus[];
extern const char kQuickAnswersDefinitionEnabled[];
extern const char kQuickAnswersTranslationEnabled[];
extern const char kQuickAnswersUnitConversionEnabled[];
extern const char kQuickAnswersNoticeImpressionCount[];
extern const char kQuickAnswersNoticeImpressionDuration[];

// Registers Quick Answers specific profile preferences for browser prefs.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace prefs
}  // namespace quick_answers

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_PUBLIC_CPP_QUICK_ANSWERS_PREFS_H_
