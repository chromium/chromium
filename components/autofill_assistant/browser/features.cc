// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/features.h"

#include "base/feature_list.h"

namespace autofill_assistant {
namespace features {

// Controls whether to enable Autofill Assistant.
const base::Feature kAutofillAssistant{"AutofillAssistant",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to enable Autofill Assistant's way of annotating DOM. If
// enabled will create an |AnnotateDomModelService|.
const base::Feature kAutofillAssistantAnnotateDom{
    "AutofillAssistantAnnotateDom", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to enable Assistant Autofill in a normal Chrome tab.
const base::Feature kAutofillAssistantChromeEntry{
    "AutofillAssistantChromeEntry", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether RPC responses from the backend should be verified for
// |GetActions| calls.
const base::Feature kAutofillAssistantVerifyGetActionsResponses{
    "AutofillAssistantVerifyGetActionsResponses",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether RPC requests to the backend should be signed for
// |GetActions| calls.
const base::Feature kAutofillAssistantSignGetActionsRequests{
    "AutofillAssistantSignGetActionsRequests",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether RPC requests to the backend should be signed for
// |GetNoRoundTripScriptsByHash| calls.
const base::Feature kAutofillAssistantSignGetNoRoundTripScriptsByHashRequests{
    "AutofillAssistantSignGetNoRoundTripByHashRequests",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether RPC responses from the backend should be verified for
// |GetNoRoundTripScriptsByHash| calls.
const base::Feature
    kAutofillAssistantVerifyGetNoRoundTripScriptsByHashResponses{
        "AutofillAssistantVerifyGetNoRoundTripByHashResponses",
        base::FEATURE_DISABLED_BY_DEFAULT};

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

// Emergency off-switch for full JS flow stack traces. Collecting full stack
// traces may be computationally expensive, though exceptions are not expected
// to happen frequently in our flows, and when they do, having full stack traces
// would be preferred, so this is enabled by default for now.
const base::Feature kAutofillAssistantFullJsFlowStackTraces{
    "AutofillAssistantFullJsFlowStackTraces", base::FEATURE_ENABLED_BY_DEFAULT};

// Same as AutofillAssistantFullJsFlowStackTraces, but for JS snippets instead
// of JS flows. Since snippets are used quite extensively already, this is
// disabled by default. This feature will let us ramp this safely in the future.
//
// TODO: this requires some more work, since we will likely need this not just
// in the main frame, but in all nested frames as well.
const base::Feature kAutofillAssistantFullJsSnippetStackTraces{
    "AutofillAssistantFullJsSnippetStackTraces",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Get a payments client token from GMS. This is an emergency off-switch in
// case calling this by default has a negative impact.
const base::Feature kAutofillAssistantGetPaymentsClientToken{
    "AutofillAssistantGetPaymentsClientToken",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Whether Autofill Assistant should enable getting the list of trigger scripts
// from the backend in a privacy sensitive way. This would enable in-CCT
// triggering for users who have "Make Searches and Browsing Better" disabled.
const base::Feature kAutofillAssistantGetTriggerScriptsByHashPrefix{
    "AutofillAssistantGetTriggerScriptsByHashPrefix",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Whether Autofill Assistant should enable in-CCT triggering, i.e., requesting
// and showing trigger scripts in CCTs without explicit user request. This
// requires also specifying valid URL heuristics via
// |kAutofillAssistantUrlHeuristics| to take effect.
const base::Feature kAutofillAssistantInCCTTriggering{
    "AutofillAssistantInCctTriggering", base::FEATURE_DISABLED_BY_DEFAULT};

// Whether Autofill Assistant should enable in-tab triggering, i.e., requesting
// and showing trigger scripts in regular tabs without explicit user request.
// This requires also specifying valid URL heuristics via
// |kAutofillAssistantUrlHeuristics| to take effect.
const base::Feature kAutofillAssistantInTabTriggering{
    "AutofillAssistantInTabTriggering", base::FEATURE_DISABLED_BY_DEFAULT};

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

// Enables assistant UI (once the feature is enabled, scripts need to use the
// USE_ASSISTANT_UI=true flag to use the assistant UI).
const base::Feature kAutofillAssistantRemoteAssistantUi{
    "AutofillAssistantRemoteAssistantUi", base::FEATURE_DISABLED_BY_DEFAULT};

// Used to configure URL heuristics for upcoming new features.
extern const base::Feature kAutofillAssistantUrlHeuristic1{
    "AutofillAssistantUrlHeuristic1", base::FEATURE_DISABLED_BY_DEFAULT};
extern const base::Feature kAutofillAssistantUrlHeuristic2{
    "AutofillAssistantUrlHeuristic2", base::FEATURE_DISABLED_BY_DEFAULT};
extern const base::Feature kAutofillAssistantUrlHeuristic3{
    "AutofillAssistantUrlHeuristic3", base::FEATURE_DISABLED_BY_DEFAULT};
extern const base::Feature kAutofillAssistantUrlHeuristic4{
    "AutofillAssistantUrlHeuristic4", base::FEATURE_DISABLED_BY_DEFAULT};
extern const base::Feature kAutofillAssistantUrlHeuristic5{
    "AutofillAssistantUrlHeuristic5", base::FEATURE_DISABLED_BY_DEFAULT};

// Legacy URL heuristics. Used to configure the start heuristics for
// |kAutofillAssistantInCctTriggering| and/or
// |kAutofillAssistantInTabTriggering|.
const base::Feature kAutofillAssistantUrlHeuristics{
    "AutofillAssistantUrlHeuristics", base::FEATURE_DISABLED_BY_DEFAULT};

// Whether Autofill Assistant is enabled on desktop.
const base::Feature kAutofillAssistantDesktop{"AutofillAssistantDesktop",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to filter existing profiles in Collect User Data action
const base::Feature kAutofillAssistantCudFilterProfiles{
    "AutofillAssistantCudFilterProfiles", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether the DidFinishNavigation should be used instead of
// PrimaryPageChanged. This is a just-in-case switch that will be removed once
// we are confident using DidFinishNavigation is working properly. b/243897243
const base::Feature kAutofillAssistantUseDidFinishNavigation{
    "AutofillAssistantUseDidFinishNavigation",
    base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace features
}  // namespace autofill_assistant
