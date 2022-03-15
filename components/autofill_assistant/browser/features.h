// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FEATURES_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FEATURES_H_

namespace base {
struct Feature;
}

namespace autofill_assistant {
namespace features {

// All features in alphabetical order.
extern const base::Feature kAutofillAssistant;
extern const base::Feature kAutofillAssistantAnnotateDom;
extern const base::Feature kAutofillAssistantChromeEntry;
extern const base::Feature kAutofillAssistantDialogOnboarding;
extern const base::Feature kAutofillAssistantDirectActions;
extern const base::Feature kAutofillAssistantDisableOnboardingFlow;
extern const base::Feature kAutofillAssistantDisableProactiveHelpTiedToMSBB;
extern const base::Feature kAutofillAssistantFullJsFlowStackTraces;
extern const base::Feature kAutofillAssistantFullJsSnippetStackTraces;
extern const base::Feature kAutofillAssistantGetPaymentsClientToken;
extern const base::Feature kAutofillAssistantInCCTTriggering;
extern const base::Feature kAutofillAssistantInTabTriggering;
extern const base::Feature kAutofillAssistantFeedbackChip;
extern const base::Feature kAutofillAssistantLoadDFMForTriggerScripts;
extern const base::Feature kAutofillAssistantProactiveHelp;
extern const base::Feature kAutofillAssistantSignGetActionsRequests;
extern const base::Feature kAutofillAssistantUrlHeuristics;
extern const base::Feature kAutofillAssistantVerifyGetActionsResponses;

}  // namespace features
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FEATURES_H_
