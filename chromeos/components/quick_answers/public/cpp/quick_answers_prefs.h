// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_PUBLIC_CPP_QUICK_ANSWERS_PREFS_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_PUBLIC_CPP_QUICK_ANSWERS_PREFS_H_

class PrefRegistrySimple;

namespace quick_answers::prefs {

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

// A preference that indicates the user has seen the Quick Answers notice.
inline constexpr char kQuickAnswersNoticed[] =
    "settings.quick_answers.consented";

// A preference that indicates the user has enabled the Quick Answers services.
// This preference can be overridden by the administrator policy.
inline constexpr char kQuickAnswersEnabled[] = "settings.quick_answers.enabled";

// A preference that indicates the user consent status for the Quick
// Answers feature.
inline constexpr char kQuickAnswersConsentStatus[] =
    "settings.quick_answers.consent_status";

// A preference that indicates the user has enabled the Quick Answers definition
// services.
// This preference can be overridden by the administrator policy.
inline constexpr char kQuickAnswersDefinitionEnabled[] =
    "settings.quick_answers.definition.enabled";

// A preference that indicates the user has enabled the Quick Answers
// translation services.
// This preference can be overridden by the administrator policy.
inline constexpr char kQuickAnswersTranslationEnabled[] =
    "settings.quick_answers.translation.enabled";

// A preference that indicates the user has enabled the Quick Answers unit
// conversion services.
// This preference can be overridden by the administrator policy.
inline constexpr char kQuickAnswersUnitConversionEnabled[] =
    "settings.quick_answers.unit_conversion.enabled";

// A preference to keep track of the number of Quick Answers notice impression.
inline constexpr char kQuickAnswersNoticeImpressionCount[] =
    "settings.quick_answers.consent.count";

// A preference to keep track of how long (in seconds) the Quick Answers notice
// has shown to the user.
// TODO(b/340628526): this pref is not used. Delete.
inline constexpr char kQuickAnswersNoticeImpressionDuration[] =
    "settings.quick_answers.consent.duration";

// Registers Quick Answers specific profile preferences for browser prefs.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace quick_answers::prefs

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_PUBLIC_CPP_QUICK_ANSWERS_PREFS_H_
