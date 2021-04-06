// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/features.h"

#include "base/feature_list.h"

namespace autofill_assistant {
namespace features {

const base::Feature kAutofillAssistant{"AutofillAssistant",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to enable Assistant Autofill in a normal Chrome tab.
const base::Feature kAutofillAssistantChromeEntry{
    "AutofillAssistantChromeEntry", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to enable dialog onboarding for Autofill Assistant
const base::Feature kAutofillAssistantDialogOnboarding{
    "AutofillAssistantDialogOnboarding", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kAutofillAssistantDirectActions{
    "AutofillAssistantDirectActions", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to disable onboarding flow for Autofill Assistant
const base::Feature kAutofillAssistantDisableOnboardingFlow{
    "AutofillAssistantDisableOnboardingFlow",
    base::FEATURE_DISABLED_BY_DEFAULT};

// By default, proactive help is only offered if MSBB is turned on. This feature
// flag allows disabling the link. Proactive help can still be offered to users
// so long as no communication to a remote backend is required. Specifically,
// base64-injected trigger scripts can be shown even in the absence of MSBB.
const base::Feature kAutofillAssistantDisableProactiveHelpTiedToMSBB{
    "AutofillAssistantDisableProactiveHelpTiedToMSBB",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Whether Autofill Assistant should enable in-Chrome triggering, i.e., without
// requiring first party trigger surfaces.
const base::Feature kAutofillAssistantInChromeTriggering{
    "AutofillAssistantInChromeTriggering", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to show the "Send feedback" chip while in an error state.
const base::Feature kAutofillAssistantFeedbackChip{
    "AutofillAssistantFeedbackChip", base::FEATURE_ENABLED_BY_DEFAULT};

// Whether autofill assistant should load the DFM for trigger scripts when
// necessary. Without this feature, trigger scripts will exit if the DFM is not
// available.
const base::Feature kAutofillAssistantLoadDFMForTriggerScripts{
    "AutofillAssistantLoadDFMForTriggerScripts",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillAssistantProactiveHelp{
    "AutofillAssistantProactiveHelp", base::FEATURE_ENABLED_BY_DEFAULT};

// Use Chrome's TabHelper system to deal with the life cycle of WebContent's
// depending Autofill Assistant objects.
const base::Feature kAutofillAssistantWithTabHelper{
    "AutofillAssistantWithTabHelper", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace autofill_assistant
