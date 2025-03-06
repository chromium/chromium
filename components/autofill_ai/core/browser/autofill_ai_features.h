// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_FEATURES_H_
#define COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_FEATURES_H_

class PrefService;

namespace autofill_ai {

// Indicates whether the current platform and the enterprise policy allows
// Autofill with Ai. This considers the AutofillPredictionSettings policy.
// If this function returns false, no AutofillAiClient should be instantiated.
bool AutofillAiIsPlatformAndEnterprisePolicyEligible(const PrefService* prefs);

}  // namespace autofill_ai

#endif  // COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_FEATURES_H_
