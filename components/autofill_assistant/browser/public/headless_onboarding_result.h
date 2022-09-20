// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_HEADLESS_ONBOARDING_RESULT_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_HEADLESS_ONBOARDING_RESULT_H_

namespace autofill_assistant {

// All possible onboarding states of a headless run. It can be seen as a public,
// extended version of `autofill_assistant::OnboardingResult`, e.g. it contains
// `kSkipped` which is currently not a possibility for non-headless runs.
enum class HeadlessOnboardingResult {
  // The state of onboarding is undefined.
  kUndefined = 0,

  // The onboarding was dismissed. No explicit choice was made.
  kDismissed = 1,

  // The onboarding was explicitly rejected.
  kRejected = 2,

  // The onboarding was interrupted by a website navigation.
  kNavigation = 3,

  // The onboarding was explicitly accepted.
  kAccepted = 4,

  // Onboarding was not shown.
  kNotShown = 5,

  // Onboarding skipped because it was never intended to be shown to the user.
  kSkipped = 6,
};
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_HEADLESS_ONBOARDING_RESULT_H_
