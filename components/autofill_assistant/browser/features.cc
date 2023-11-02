// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/features.h"

#include "base/feature_list.h"

namespace autofill_assistant {
namespace features {

// Controls whether to enable Autofill Assistant.
BASE_FEATURE(kAutofillAssistant,
             "AutofillAssistant",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to enable Autofill Assistant's way of annotating DOM. If
// enabled will create an |AnnotateDomModelService|.
BASE_FEATURE(kAutofillAssistantAnnotateDom,
             "AutofillAssistantAnnotateDom",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to enable Assistant Autofill in a normal Chrome tab.
BASE_FEATURE(kAutofillAssistantChromeEntry,
             "AutofillAssistantChromeEntry",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether RPC responses from the backend should be verified for
// |GetActions| calls.
BASE_FEATURE(kAutofillAssistantVerifyGetActionsResponses,
             "AutofillAssistantVerifyGetActionsResponses",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether RPC requests to the backend should be signed for
// |GetActions| calls.
BASE_FEATURE(kAutofillAssistantSignGetActionsRequests,
             "AutofillAssistantSignGetActionsRequests",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether RPC requests to the backend should be signed for
// |GetNoRoundTripScriptsByHash| calls.
BASE_FEATURE(kAutofillAssistantSignGetNoRoundTripScriptsByHashRequests,
             "AutofillAssistantSignGetNoRoundTripByHashRequests",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether RPC responses from the backend should be verified for
// |GetNoRoundTripScriptsByHash| calls.
BASE_FEATURE(kAutofillAssistantVerifyGetNoRoundTripScriptsByHashResponses,
             "AutofillAssistantVerifyGetNoRoundTripByHashResponses",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to enable dialog onboarding for Autofill Assistant
BASE_FEATURE(kAutofillAssistantDialogOnboarding,
             "AutofillAssistantDialogOnboarding",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAutofillAssistantDirectActions,
             "AutofillAssistantDirectActions",
             base::FEATURE_ENABLED_BY_DEFAULT);

// By default, proactive help is only offered if MSBB is turned on. This feature
// flag allows disabling the link. Proactive help can still be offered to users
// so long as no communication to a remote backend is required. Specifically,
// base64-injected trigger scripts can be shown even in the absence of MSBB.
BASE_FEATURE(kAutofillAssistantDisableProactiveHelpTiedToMSBB,
             "AutofillAssistantDisableProactiveHelpTiedToMSBB",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Emergency off-switch for full JS flow stack traces. Collecting full stack
// traces may be computationally expensive, though exceptions are not expected
// to happen frequently in our flows, and when they do, having full stack traces
// would be preferred, so this is enabled by default for now.
BASE_FEATURE(kAutofillAssistantFullJsFlowStackTraces,
             "AutofillAssistantFullJsFlowStackTraces",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Same as AutofillAssistantFullJsFlowStackTraces, but for JS snippets instead
// of JS flows. Since snippets are used quite extensively already, this is
// disabled by default. This feature will let us ramp this safely in the future.
//
// TODO: this requires some more work, since we will likely need this not just
// in the main frame, but in all nested frames as well.
BASE_FEATURE(kAutofillAssistantFullJsSnippetStackTraces,
             "AutofillAssistantFullJsSnippetStackTraces",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Get a payments client token from GMS. This is an emergency off-switch in
// case calling this by default has a negative impact.
BASE_FEATURE(kAutofillAssistantGetPaymentsClientToken,
             "AutofillAssistantGetPaymentsClientToken",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether Autofill Assistant should enable getting the list of trigger scripts
// from the backend in a privacy sensitive way. This would enable in-CCT
// triggering for users who have "Make Searches and Browsing Better" disabled.
BASE_FEATURE(kAutofillAssistantGetTriggerScriptsByHashPrefix,
             "AutofillAssistantGetTriggerScriptsByHashPrefix",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether Autofill Assistant should enable in-CCT triggering, i.e., requesting
// and showing trigger scripts in CCTs without explicit user request. This
// requires also specifying valid URL heuristics via
// |kAutofillAssistantUrlHeuristics| to take effect.
BASE_FEATURE(kAutofillAssistantInCCTTriggering,
             "AutofillAssistantInCctTriggering",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to show the "Send feedback" chip while in an error state.
BASE_FEATURE(kAutofillAssistantFeedbackChip,
             "AutofillAssistantFeedbackChip",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether autofill assistant should load the DFM for trigger scripts when
// necessary. Without this feature, trigger scripts will exit if the DFM is not
// available.
BASE_FEATURE(kAutofillAssistantLoadDFMForTriggerScripts,
             "AutofillAssistantLoadDFMForTriggerScripts",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAutofillAssistantProactiveHelp,
             "AutofillAssistantProactiveHelp",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables assistant UI (once the feature is enabled, scripts need to use the
// USE_ASSISTANT_UI=true flag to use the assistant UI).
BASE_FEATURE(kAutofillAssistantRemoteAssistantUi,
             "AutofillAssistantRemoteAssistantUi",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Send the Moonracer model version in the client context.
BASE_FEATURE(kAutofillAssistantSendModelVersionInClientContext,
             "AutofillAssistantSendModelVersionInClientContext",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Used to configure URL heuristics for upcoming new features.
BASE_FEATURE(kAutofillAssistantUrlHeuristic1,
             "AutofillAssistantUrlHeuristic1",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAutofillAssistantUrlHeuristic2,
             "AutofillAssistantUrlHeuristic2",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAutofillAssistantUrlHeuristic3,
             "AutofillAssistantUrlHeuristic3",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAutofillAssistantUrlHeuristic4,
             "AutofillAssistantUrlHeuristic4",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAutofillAssistantUrlHeuristic5,
             "AutofillAssistantUrlHeuristic5",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAutofillAssistantUrlHeuristic6,
             "AutofillAssistantUrlHeuristic6",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAutofillAssistantUrlHeuristic7,
             "AutofillAssistantUrlHeuristic7",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAutofillAssistantUrlHeuristic8,
             "AutofillAssistantUrlHeuristic8",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAutofillAssistantUrlHeuristic9,
             "AutofillAssistantUrlHeuristic9",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether Autofill Assistant is enabled on desktop.
BASE_FEATURE(kAutofillAssistantDesktop,
             "AutofillAssistantDesktop",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to filter existing profiles in Collect User Data action
BASE_FEATURE(kAutofillAssistantCudFilterProfiles,
             "AutofillAssistantCudFilterProfiles",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the DidFinishNavigation should be used instead of
// PrimaryPageChanged. This is a just-in-case switch that will be removed once
// we are confident using DidFinishNavigation is working properly. b/243897243
BASE_FEATURE(kAutofillAssistantUseDidFinishNavigation,
             "AutofillAssistantUseDidFinishNavigation",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace features
}  // namespace autofill_assistant
