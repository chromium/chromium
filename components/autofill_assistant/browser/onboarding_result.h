// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ONBOARDING_RESULT_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ONBOARDING_RESULT_H_

namespace autofill_assistant {

// GENERATED_JAVA_ENUM_PACKAGE: (
// org.chromium.components.autofill_assistant.onboarding)
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: AssistantOnboardingResult
enum class OnboardingResult {
  // The onboarding was dismissed. No explicit choice was made.
  DISMISSED = 0,

  // The onboarding was explicitly rejected.
  REJECTED = 1,

  // The onboarding was interrupted by a website navigation.
  NAVIGATION = 2,

  // THe onboarding was explicitly accepted.
  ACCEPTED = 3,
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ONBOARDING_RESULT_H_
