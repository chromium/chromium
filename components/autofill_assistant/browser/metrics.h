// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_METRICS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_METRICS_H_

#include <iosfwd>
#include <string>

#include "services/metrics/public/cpp/ukm_source_id.h"

namespace base {
class TimeDelta;
}

namespace ukm {
class UkmRecorder;
}

namespace autofill_assistant {

enum class StartupMode;
class ScriptParameters;
enum TriggerScriptProto_TriggerUIType : int;

// A class to generate Autofill Assistant metrics.
class Metrics {
 public:
  // The different ways that autofill assistant can stop.
  //
  // GENERATED_JAVA_ENUM_PACKAGE: (
  // org.chromium.components.autofill_assistant.metrics)
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: DropOutReason
  //
  // This enum is used in histograms, do not remove/renumber entries. Only add
  // at the end and update kMaxValue. Also remember to update the
  // AutofillAssistantDropOutReason enum listing in
  // tools/metrics/histograms/enums.xml.
  enum class DropOutReason {
    AA_START = 0,
    AUTOSTART_TIMEOUT = 1,
    NO_SCRIPTS = 2,
    CUSTOM_TAB_CLOSED = 3,
    DECLINED = 4,
    SHEET_CLOSED = 5,
    SCRIPT_FAILED = 6,
    NAVIGATION = 7,
    OVERLAY_STOP = 8,
    PR_FAILED = 9,
    CONTENT_DESTROYED = 10,
    RENDER_PROCESS_GONE = 11,
    INTERSTITIAL_PAGE = 12,
    SCRIPT_SHUTDOWN = 13,
    SAFETY_NET_TERMINATE = 14,  // This is a "should never happen" entry.
    TAB_DETACHED = 15,
    TAB_CHANGED = 16,
    GET_SCRIPTS_FAILED = 17,
    GET_SCRIPTS_UNPARSABLE = 18,
    NO_INITIAL_SCRIPTS = 19,
    DFM_INSTALL_FAILED = 20,
    DOMAIN_CHANGE_DURING_BROWSE_MODE = 21,
    BACK_BUTTON_CLICKED = 22,
    ONBOARDING_BACK_BUTTON_CLICKED = 23,
    NAVIGATION_WHILE_RUNNING = 24,
    UI_CLOSED_UNEXPECTEDLY = 25,  // This is a "should never happen" entry.
    ONBOARDING_NAVIGATION = 26,
    ONBOARDING_DIALOG_DISMISSED = 27,
    MULTIPLE_AUTOSTARTABLE_SCRIPTS = 28,

    kMaxValue = MULTIPLE_AUTOSTARTABLE_SCRIPTS
  };

  // The different ways to complete the onboarding / user consent screen.
  //
  // This enum is used in UKM metrics, do not remove/renumber entries. Only add
  // at the end and update kMaxValue. Also remember to update the
  // AutofillAssistantOnboarding enum listing in
  // tools/metrics/histograms/enums.xml.
  enum class Onboarding {
    // The onboarding was shown to the user.
    OB_SHOWN = 0,
    // The onboarding was not shown to the user, because the user has already
    // accepted the onboarding in a previous flow.
    OB_NOT_SHOWN = 1,
    // The user explicitly accepted the onboarding by tapping the relevant chip.
    OB_ACCEPTED = 2,
    // The user explicitly rejected the onboarding by tapping the relevant chip.
    OB_CANCELLED = 3,
    // The user implicitly rejected the onboarding. Some of the possible reasons
    // include navigating away, tapping the back button, closing the tab, etc.
    OB_NO_ANSWER = 4,

    kMaxValue = OB_NO_ANSWER
  };

  // The different actions that can be performed on TTS button click.
  //
  // This enum is used in histograms, do not remove/renumber entries. Only add
  // at the end and update kMaxValue. Also remember to update the
  // AutofillAssistantTextToSpeechButtonAction enum listing in
  // tools/metrics/histograms/enums.xml.
  enum class TtsButtonAction {
    PLAY_TTS = 0,
    DISABLE_BUTTON = 1,
    ENABLE_BUTTON_AND_PLAY_TTS = 2,

    kMaxValue = ENABLE_BUTTON_AND_PLAY_TTS
  };

  // The different TTS engine events that are received by the autofill
  // assistant TTS controller.
  //
  // This enum is used in histograms, do not remove/renumber entries. Only add
  // at the end and update kMaxValue. Also remember to update the
  // AutofillAssistantTextToSpeechEngineEvent enum listing in
  // tools/metrics/histograms/enums.xml.
  enum class TtsEngineEvent {
    TTS_EVENT_START = 0,
    TTS_EVENT_END = 1,
    TTS_EVENT_ERROR = 2,
    TTS_EVENT_OTHER = 3,

    kMaxValue = TTS_EVENT_OTHER
  };

  // The different ways for payment request to succeed or fail, broken down by
  // whether the PR initially presented to the user was completely pre-filled
  // or not.
  //
  // This enum is used in histograms, do not remove/renumber entries. Only add
  // at the end and update kMaxValue. Also remember to update the
  // AutofillAssistantPaymentRequestPrefilled enum listing in
  // tools/metrics/histograms/enums.xml.
  enum class PaymentRequestPrefilled {
    // The action finished successfully (the user clicked the "confirm" chip).
    PREFILLED_SUCCESS = 0,
    NOTPREFILLED_SUCCESS = 1,
    // The action finished unsuccessfully.
    PREFILLED_FAILURE = 2,
    NOTPREFILLED_FAILURE = 3,
    // Unknown result, should not happen.
    PREFILLED_UNKNOWN = 4,
    NOTPREFILLED_UNKNOWN = 5,
    // The user clicked the link in the terms and condition text.
    PREFILLED_TERMS_AND_CONDITIONS_LINK_CLICKED = 6,
    NOTPREFILLED_TERMS_AND_CONDITIONS_LINK_CLICKED = 7,
    // The user clicked one of the non-confirm chips.
    PREFILLED_ADDITIONAL_ACTION_SELECTED = 8,
    NOTPREFILLED_ADDITIONAL_ACTION_SELECTED = 9,

    kMaxValue = NOTPREFILLED_ADDITIONAL_ACTION_SELECTED
  };

  // Whether autofill info was changed during an autofill assistant payment
  // request, or not.
  //
  // This enum is used in histograms, do not remove/renumber entries. Only add
  // at the end and update kMaxValue. Also remember to update the
  // AutofillAssistantPaymentRequestAutofillInfoChanged enum listing
  // in tools/metrics/histograms/enums.xml.
  enum class PaymentRequestAutofillInfoChanged {
    // The action finished successfully (the user clicked the "confirm" chip).
    CHANGED_SUCCESS = 0,
    NOTCHANGED_SUCCESS = 1,
    // The action finished unsuccessfully.
    CHANGED_FAILURE = 2,
    NOTCHANGED_FAILURE = 3,
    // Unknown result, should not happen.
    CHANGED_UNKNOWN = 4,
    NOTCHANGED_UNKNOWN = 5,
    // The user clicked the link in the terms and condition text.
    CHANGED_TERMS_AND_CONDITIONS_LINK_CLICKED = 6,
    NOTCHANGED_TERMS_AND_CONDITIONS_LINK_CLICKED = 7,
    // The user clicked one of the non-confirm chips.
    CHANGED_ADDITIONAL_ACTION_SELECTED = 8,
    NOTCHANGED_ADDITIONAL_ACTION_SELECTED = 9,

    kMaxValue = NOTCHANGED_ADDITIONAL_ACTION_SELECTED
  };

  // Whether a billing postal code was required and whether the user ultimately
  // succeeded or not.
  //
  // This enum is used in histograms, do not remove/renumber entries. Only add
  // at the end and update kMaxValue. Also remember to update the
  // AutofillAssistantPaymentRequestMandatoryPostalCode enum listing
  // in tools/metrics/histograms/enums.xml.
  // Note: This is deprecated and no longer logged.
  enum class PaymentRequestMandatoryPostalCode {
    REQUIRED_INITIALLY_WRONG_SUCCESS = 0,
    REQUIRED_INITIALLY_WRONG_FAILURE = 1,
    REQUIRED_INITIALLY_RIGHT_SUCCESS = 2,
    REQUIRED_INITIALLY_RIGHT_FAILURE = 3,
    NOT_REQUIRED = 4,

    kMaxValue = NOT_REQUIRED
  };

  // The different ways in which DFM can be installed.
  //
  // GENERATED_JAVA_ENUM_PACKAGE: (
  // org.chromium.components.autofill_assistant.metrics)
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: FeatureModuleInstallation
  //
  // This enum is used in histograms, do not remove/renumber entries. Only add
  // at the end and update kMaxValue. Also remember to update the
  // AutofillAssistantFeatureModuleInstallation enum listing in
  // tools/metrics/histograms/enums.xml.
  enum class FeatureModuleInstallation {
    DFM_BACKGROUND_INSTALLATION_REQUESTED = 0,
    DFM_FOREGROUND_INSTALLATION_SUCCEEDED = 1,
    DFM_FOREGROUND_INSTALLATION_FAILED = 2,
    DFM_ALREADY_INSTALLED = 3,

    kMaxValue = DFM_ALREADY_INSTALLED
  };

  // Whether a trigger script was running invisibly or visible to the user.
  //
  // This enum is used in UKM metrics, do not remove/renumber entries. Only add
  // at the end and update kMaxValue. Also remember to update the
  // AutofillAssistantTriggerScriptShownToUser enum listing in
  // tools/metrics/histograms/enums.xml and the description in
  // tools/metrics/ukm/ukm.xml as necessary.
  enum class TriggerScriptShownToUser {
    // The number of times a trigger script was successfully fetched and
    // started.
    // Can happen multiple times per run (in case of tab switch).
    RUNNING = 0,
    // The number of times a trigger script was shown to the user. Can happen
    // multiple times per run.
    SHOWN_TO_USER = 1,
    // Since Chrome M-88. The user tapped the 'not now' button. Can happen
    // multiple times per run.
    NOT_NOW = 2,
    // Since Chrome M-88. The trigger script was automatically hidden due to the
    // trigger condition no longer being true. Can happen multiple times per
    // run.
    HIDE_ON_TRIGGER_CONDITION_NO_LONGER_TRUE = 3,
    // Since Chrome M-88. The user swipe-dismissed the bottom sheet. Depending
    // on configuration, this may happen multiple times per run.
    SWIPE_DISMISSED = 4,
    // Since Chrome M-93. The UI has timed out without receiving any user
    // interaction.
    UI_TIMEOUT = 5,

    kMaxValue = UI_TIMEOUT
  };

  // The different ways a user might have opted out of the trigger script
  // experience.
  //
  // This enum is used in UKM metrics, do not remove/renumber entries. Only add
  // at the end and update kMaxValue. Also remember to update the
  // AutofillAssistantTriggerScriptStarted enum listing in
  // tools/metrics/histograms/enums.xml and the description in
  // tools/metrics/ukm/ukm.xml as necessary.
  enum class TriggerScriptStarted {
    // Device did not have DFM downloaded.
    DFM_UNAVAILABLE = 0,
    // User has not seen the trigger script before and will see first time
    // experience.
    FIRST_TIME_USER = 3,
    // User has seen the first-time experience before and will see returning
    // user experience.
    RETURNING_USER = 4,
    // Since Chrome M-88. The proactive trigger setting is disabled. The user
    // has either chosen 'never show again' in the prompt or manually disabled
    // the setting in Chrome settings.
    PROACTIVE_TRIGGERING_DISABLED = 5,
    // Since Chrome M-88. Intended as a catch-all. This is reported as soon as a
    // lite-script intent is received (of course, only for people with MSBB
    // enabled).
    INTENT_RECEIVED = 6,
    // Since Chrome M-91. A required Chrome feature was disabled.
    FEATURE_DISABLED = 7,
    // Since Chrome M-91. No initial url was set, neither in ORIGINAL_DEEPLINK
    // nor in the intent data.
    NO_INITIAL_URL = 8,
    // Since Chrome M-91. A mandatory script parameter was missing.
    MANDATORY_PARAMETER_MISSING = 9,
    // Since Chrome M-92. The user never navigated to a different domain.
    NAVIGATED_AWAY = 10,
    // Since Chrome M-92. The navigation to the target domain failed.
    NAVIGATION_ERROR = 11,

    // DEPRECATED, only sent by Chrome M-86 and M-87.
    //
    // User has explicitly rejected the trigger script two times and thus opted
    // out of  the experience.
    CANCELED_TWO_TIMES = 1,
    // User has rejected the onboarding and thus opted out of the experience.
    ONBOARDING_REJECTED = 2,

    kMaxValue = NAVIGATION_ERROR
  };

  // The different ways in which a trigger script may finish.
  //
  // This enum is used in UKM metrics, do not remove/renumber entries. Only add
  // at the end and update kMaxValue. Also remember to update the
  // AutofillAssistantTriggerScriptFinished enum listing in
  // tools/metrics/histograms/enums.xml and the description in
  // tools/metrics/ukm/ukm.xml as necessary.
  enum class TriggerScriptFinishedState {
    // Communication with backend failed.
    GET_ACTIONS_FAILED = 3,
    // Failed to parse the proto sent by the backend.
    GET_ACTIONS_PARSE_ERROR = 4,
    // Trigger script failed due to a navigation event to a non-allowed domain.
    PROMPT_FAILED_NAVIGATE = 9,
    // Trigger script succeeded. The user accepted the prompt.
    PROMPT_SUCCEEDED = 13,
    // Since Chrome M-88. The user tapped the 'cancel for this session' button.
    PROMPT_FAILED_CANCEL_SESSION = 14,
    // Since Chrome M-88. The user tapped the 'never show again' button.
    PROMPT_FAILED_CANCEL_FOREVER = 15,
    // Since Chrome M-88. The trigger script has timed out. This indicates that
    // trigger conditions were evaluated for >= timeout without success. Time is
    // only counted while the tab is visible and the trigger script is
    // invisible.
    // The timeout resets on tab change.
    TRIGGER_CONDITION_TIMEOUT = 17,
    // Since Chrome M-88. A navigation error occurred, leading to Chrome showing
    // an error page.
    NAVIGATION_ERROR = 18,
    // Since Chrome M-88. The tab was closed while the prompt was visible.
    WEB_CONTENTS_DESTROYED_WHILE_VISIBLE = 19,
    // Since Chrome M-88. The tab was closed while the prompt was invisible.
    WEB_CONTENTS_DESTROYED_WHILE_INVISIBLE = 20,
    // Since Chrome M-88. The RPC to fetch the trigger scripts returned with an
    // empty response.
    NO_TRIGGER_SCRIPT_AVAILABLE = 21,
    // Since Chrome M-88. The trigger script failed to show. This can happen,
    // for example, if the activity was changed after triggering (e.g.,
    // switching from CCT to regular tab).
    FAILED_TO_SHOW = 22,
    // Since Chrome M-88. The proactive help switch was enabled at start, but
    // then manually disabled in the Chrome settings.
    DISABLED_PROACTIVE_HELP_SETTING = 23,
    // Since Chrome M-88. The client failed to base64-decode the trigger script
    // specified in the script parameters.
    BASE64_DECODING_ERROR = 24,
    // The user rejected the bottom sheet onboarding
    BOTTOMSHEET_ONBOARDING_REJECTED = 25,
    // Transitioning from CCT to regular tab is currently not supported.
    CCT_TO_TAB_NOT_SUPPORTED = 26,
    // The current trigger script was canceled. This typically happens when a
    // new startup request takes precedence.
    CANCELED = 27,

    // NOTE: All values in this block are DEPRECATED and will only be sent by
    // Chrome M-86 and M-87.
    //
    // The trigger script failed for an unknown reason.
    UNKNOWN_FAILURE = 0,
    // Can happen when users close the tab or similar.
    SERVICE_DELETED = 1,
    // |GetActions| was asked to retrieve a wrong script path.
    PATH_MISMATCH = 2,
    // One or multiple unsafe actions were contained in script.
    UNSAFE_ACTIONS = 5,
    // The mini script is invalid. A valid script must contain a prompt
    // (browse=true) action and end in a prompt(browse=false) action.
    INVALID_SCRIPT = 6,
    // The prompt(browse) action failed due to a navigation event to a
    // non-allowed domain.
    BROWSE_FAILED_NAVIGATE = 7,
    // The prompt(browse) action failed for an unknown reason.
    BROWSE_FAILED_OTHER = 8,
    // The prompt(regular) action failed because the condition to show it was no
    // longer true.
    PROMPT_FAILED_CONDITION_NO_LONGER_TRUE = 10,
    // The prompt(regular) action failed because the user tapped the close chip.
    PROMPT_FAILED_CLOSE = 11,
    // The prompt(regular) action failed for an unknown reason.
    PROMPT_FAILED_OTHER = 12,
    // Since Chrome M-88. The bottom sheet was swipe-dismissed by the user.
    PROMPT_SWIPE_DISMISSED = 16,

    kMaxValue = CANCELED
  };

  // The different ways a user who has successfully completed a light script may
  // accept or reject the onboarding
  //
  // This enum is used in UKM metrics, do not remove/renumber entries. Only add
  // at the end and update kMaxValue. Also remember to update the
  // AutofillAssistantTriggerScriptOnboarding enum listing in
  // tools/metrics/histograms/enums.xml and the description in
  // tools/metrics/ukm/ukm.xml as necessary.
  enum class TriggerScriptOnboarding {
    // The user has seen and accepted the onboarding.
    ONBOARDING_SEEN_AND_ACCEPTED = 0,
    // The user has seen and rejected the onboarding.
    ONBOARDING_SEEN_AND_REJECTED = 1,
    // The user has already accepted the onboarding in the past.
    ONBOARDING_ALREADY_ACCEPTED = 2,
    // The user has seen and dismissed the onboarding.
    ONBOARDING_SEEN_AND_DISMISSED = 3,
    // The onboarding was interrupted by a website navigation.
    ONBOARDING_SEEN_AND_INTERRUPTED_BY_NAVIGATION = 4,

    kMaxValue = ONBOARDING_SEEN_AND_INTERRUPTED_BY_NAVIGATION
  };

  // Metric describing in-Chrome triggering. Only reported if the corresponding
  // feature is enabled.
  //
  // This enum is used in UKM metrics, do not remove/renumber entries. Only add
  // at the end and update kMaxValue. Also remember to update the
  // AutofillAssistantInChromeTriggerAction enum listing in
  // tools/metrics/histograms/enums.xml and the description in
  // tools/metrics/ukm/ukm.xml as necessary.
  enum class InChromeTriggerAction {
    // No trigger script was requested for an unspecified reason. This is
    // intended as a catch-all bucket and should ideally always be empty.
    OTHER = 0,
    // No trigger script was requested because the user has temporarily opted
    // out of receiving implicit prompts for this domain (either until tab close
    // or until the cache entry is stale, whichever is sooner).
    USER_DENYLISTED_DOMAIN = 1,
    // No trigger script was requested because an earlier request to the same
    // domain failed and the cache was still fresh.
    CACHE_HIT_UNSUPPORTED_DOMAIN = 2,
    // No trigger script was requested because the heuristic response did not
    // match any trigger intent.
    NO_HEURISTIC_MATCH = 3,
    // The heuristic reported a match and a trigger script was requested.
    // Note that this does not indicate that a trigger script was fetched
    // successfully, merely that it was requested.
    TRIGGER_SCRIPT_REQUESTED = 4,

    kMaxValue = TRIGGER_SCRIPT_REQUESTED
  };

  // Used for logging when the platform-specific dependencies are invalidated.
  // For example: When the activity is changed on Android.
  //
  // This enum is used in histograms, do not remove/renumber entries. Only add
  // at the end and update kMaxValue. Also remember to update the
  // AutofillAssistantDependenciesInvalidated enum listing in
  // tools/metrics/histograms/enums.xml.
  enum class DependenciesInvalidated {
    // The dependencies were invalidated while the starter existed but before
    // Start() was called.
    OUTSIDE_FLOW = 0,
    // The dependencies were invalidated while the flow was trying to start. For
    // example during onboarding or during the execution of the trigger script.
    DURING_STARTUP = 1,
    // The dependencies were invalidated during the execution of a flow.
    DURING_FLOW = 2,

    kMaxValue = DURING_FLOW
  };

  // Used for logging the CALLER script parameter.
  //
  // This enum is used in histograms, do not remove/renumber entries. Only add
  // at the end and update kMaxValue. Also remember to update the
  // AutofillAssistantCaller enum listing in
  // tools/metrics/histograms/enums.xml.
  enum class AutofillAssistantCaller {
    UNKNOWN_CALLER = 0,
    ASSISTANT = 1,
    SEARCH = 2,
    STARTER_APP = 3,
    SEARCH_ADS = 4,
    SHOPPING_PROPERTY = 5,
    EMULATOR = 6,
    // The run was started from within Chrome (e.g., URL heuristic match or
    // password change launched from the settings page).
    IN_CHROME = 7,
    // The run was triggered by the Direction Action API in Chrome.
    DIRECT_ACTION = 8,
    // The run was started by Google Password Manager (passwords.google.com or
    // credential_manager in gmscore module on Android).
    GOOGLE_PASSWORD_MANAGER = 9,

    kMaxValue = GOOGLE_PASSWORD_MANAGER
  };

  // Used for logging the SOURCE script parameter.
  //
  // This enum is used in histograms, do not remove/renumber entries. Only add
  // at the end and update kMaxValue. Also remember to update the
  // AutofillAssistantSource enum listing in
  // tools/metrics/histograms/enums.xml.
  enum class AutofillAssistantSource {
    UNKNOWN_SOURCE = 0,
    ORGANIC = 1,
    FRESH_DEEPLINK = 2,
    STARTER_APP = 3,
    EMULATOR_VALIDATION = 4,
    S_API = 5,
    C_CARD = 6,
    MD_CARD = 7,
    C_NOTIFICATION = 8,
    G_CAROUSEL = 9,

    kMaxValue = G_CAROUSEL
  };

  // Used for logging the intent of an autofill-assistant flow.
  //
  // This enum is used in UKM metrics, do not remove/renumber entries. Only add
  // at the end and update kMaxValue. Also remember to update the
  // AutofillAssistantIntent enum listing in
  // tools/metrics/histograms/enums.xml.
  enum class AutofillAssistantIntent {
    UNDEFINED_INTENT = 0,
    BUY_MOVIE_TICKET = 3,
    RENT_CAR = 9,
    SHOPPING = 10,
    TELEPORT = 11,
    SHOPPING_ASSISTED_CHECKOUT = 14,
    FLIGHTS_CHECKIN = 15,
    FOOD_ORDERING = 17,
    PASSWORD_CHANGE = 18,
    FOOD_ORDERING_PICKUP = 19,
    FOOD_ORDERING_DELIVERY = 20,
    UNLAUNCHED_VERTICAL_1 = 22,
    FIND_COUPONS = 25,

    kMaxValue = FIND_COUPONS
  };

  // Used for logging active autofill-assistant experiments. This is intended
  // to be a bitmask to support cases where more than one experiment is running.
  //
  // This enum is used in UKM metrics, do not remove/renumber entries. Only add
  // at the end and update kMaxValue. Also remember to update the
  // AutofillAssistantExperiment enum listing in
  // tools/metrics/histograms/enums.xml.
  enum class AutofillAssistantExperiment {
    // No experiment is running.
    NO_EXPERIMENT = 0,
    // An unknown experiment is running.
    UNKNOWN_EXPERIMENT = 1,

    kMaxValue = UNKNOWN_EXPERIMENT
  };

  // Used to record successful and failed autofill-assistant startup requests.
  //
  // This enum is used in UKM metrics, do not remove/renumber entries. Only add
  // at the end and update kMaxValue. Also remember to update the
  // AutofillAssistantStarted enum listing in
  // tools/metrics/histograms/enums.xml.
  enum class AutofillAssistantStarted {
    FAILED_FEATURE_DISABLED = 0,
    FAILED_MANDATORY_PARAMETER_MISSING = 1,
    FAILED_SETTING_DISABLED = 2,
    FAILED_NO_INITIAL_URL = 3,
    OK_DELAYED_START = 4,
    OK_IMMEDIATE_START = 5,

    kMaxValue = OK_IMMEDIATE_START
  };

  // Used to track what action the user performed on the contact/shipping/card
  // data. Only reported at the end of a CollectUserData action. Reported up to
  // three times for each of contact/shipping/credit card and only if the given
  // entry was requested at that step.
  //
  // This enum is used in UKM metrics, do not remove/renumber entries. Only add
  // at the end and update kMaxValue. Also remember to update the
  // AutofillAssistantUserDataSelectionState enum listing in
  // tools/metrics/histograms/enums.xml and the description in
  // tools/metrics/ukm/ukm.xml as necessary.
  enum class UserDataSelectionState {
    // The user kept the initial selection without modifications.
    NO_CHANGE,
    // The user kept the initial selection but edited it.
    EDIT_PRESELECTED,
    // The user changed the selected option.
    SELECTED_DIFFERENT_ENTRY,
    // The user changed the selected option and edited the newly selected entry.
    SELECTED_DIFFERENT_AND_MODIFIED_ENTRY,
    // The user added a new entry.
    NEW_ENTRY,

    kMaxValue = NEW_ENTRY
  };

  // Used to log the initial number of contact/shipping/card entries. Reported
  // at the end of a CollectUserData action.
  //
  // This enum is used in UKM metrics, do not remove/renumber entries. Only add
  // at the end and update kMaxValue. Also remember to update the
  // AutofillAssistantUserDataEntryCount enum listing in
  // tools/metrics/histograms/enums.xml and the description in
  // tools/metrics/ukm/ukm.xml as necessary.
  enum class UserDataEntryCount {
    ZERO,
    ONE,
    TWO,
    THREE,
    FOUR,
    FIVE_OR_MORE,

    kMaxValue = FIVE_OR_MORE
  };

  // Whether the CollectUserData action was successfully completed.
  //
  // This enum is used in UKM metrics, do not remove/renumber entries. Only add
  // at the end and update kMaxValue. Also remember to update the
  // AutofillAssistantCollectUserDataResult enum listing in
  // tools/metrics/histograms/enums.xml and the description in
  // tools/metrics/ukm/ukm.xml as necessary.
  enum class CollectUserDataResult {
    UNKNOWN,
    // The action finished successfully (the user clicked the "confirm" chip).
    SUCCESS,
    // The action finished unsuccessfully.
    FAILURE,
    // The user clicked the link in the terms and condition text.
    TERMS_AND_CONDITIONS_LINK_CLICKED,
    // The user clicked one of the non-confirm chips.
    ADDITIONAL_ACTION_SELECTED,

    kMaxValue = ADDITIONAL_ACTION_SELECTED
  };

  // The source of the initial data for the current CollectUserData action.
  //
  // This enum is used in UKM metrics, do not remove/renumber entries. Only add
  // at the end and update kMaxValue. Also remember to update the
  // AutofillAssistantUserDataSource enum listing in
  // tools/metrics/histograms/enums.xml and the description in
  // tools/metrics/ukm/ukm.xml as necessary.
  enum class UserDataSource {
    UNKNOWN,
    BACKEND,
    CHROME_AUTOFILL,

    kMaxValue = CHROME_AUTOFILL
  };

  // Outcome of the CUP verification process for GetAction RPC calls. CUP
  // verification is used to check whether the actions delivered to the client
  // come from a trusted source, and requires the request from the client to be
  // signed first. Events are only recorded for RPC calls where we support CUP.
  //
  // This verification event is recorded after the response is deserialized but
  // before it's actually used in the client. This is the case even if the
  // verification doesn't happen due to the request not being signed in the
  // first place. HTTP failures are checked before the feature flags for
  // signing and verification, and therefore a failing HTTP request with
  // verification disabled will be logged as |HTTP_FAILED|.
  //
  // This enum is used in histograms, do not remove/renumber entries. Only add
  // at the end and update kMaxValue. Also remember to update the
  // CupRpcVerificationEvent enum listing in tools/metrics/histograms/enums.xml.
  enum class CupRpcVerificationEvent {
    // Signature doesn't match response or context, message origin cannot be
    // confirmed.
    VERIFICATION_FAILED = 0,
    // Signature correctly matches the response and context.
    VERIFICATION_SUCCEEDED = 1,
    // Response parsing failed. Rpc verification won't be performed.
    PARSING_FAILED = 2,
    // Response verification is disabled. Rpc verification won't be performed.
    VERIFICATION_DISABLED = 3,
    // Request signing is disabled. Rpc verification won't be performed.
    SIGNING_DISABLED = 4,
    // HTTP call didn't return "OK" 200. Rpc verification won't be performed.
    HTTP_FAILED = 5,

    kMaxValue = HTTP_FAILED
  };

  // Used for bitmasks for the InitialContactFieldsStatus,
  // InitialBillingFieldsStatus and InitialShippingFieldsStatus metrics.
  enum AutofillAssistantProfileFields {
    NAME_FIRST = 1 << 0,
    NAME_LAST = 1 << 1,
    NAME_FULL = 1 << 2,
    EMAIL_ADDRESS = 1 << 3,
    PHONE_HOME_NUMBER = 1 << 4,
    PHONE_HOME_COUNTRY_CODE = 1 << 5,
    PHONE_HOME_WHOLE_NUMBER = 1 << 6,
    ADDRESS_HOME_COUNTRY = 1 << 7,
    ADDRESS_HOME_STATE = 1 << 8,
    ADDRESS_HOME_CITY = 1 << 9,
    ADDRESS_HOME_ZIP = 1 << 10,
    ADDRESS_HOME_LINE1 = 1 << 11,
  };

  enum AutofillAssistantCreditCardFields {
    CREDIT_CARD_NAME_FULL = 1 << 0,
    CREDIT_CARD_EXP_MONTH = 1 << 1,
    CREDIT_CARD_EXP_2_DIGIT_YEAR = 1 << 2,
    CREDIT_CARD_EXP_4_DIGIT_YEAR = 1 << 3,
    MASKED = 1 << 4,
    // If the card is masked, this is always logged as present, to match what
    // CollectUserData considers complete for the purposes of enabling the
    // "Continue" button.
    VALID_NUMBER = 1 << 5,
  };

  // This enum is used in histograms, do not remove/renumber entries. Only add
  // at the end and update kMaxValue. Also remember to update the
  // AutofillAssistantOnboardingFetcherResultStatus enum listing in
  // tools/metrics/histograms/enums.xml.
  enum class OnboardingFetcherResultStatus {
    kOk = 0,
    // No body was received from the server.
    kNoBody = 1,
    // Parsing the JSON failed.
    kInvalidJson = 1,
    // The JSON was not in the form we expected it to be.
    kInvalidData = 2,
    kMaxValue = kInvalidData
  };

  static void RecordDropOut(DropOutReason reason, const std::string& intent);
  static void RecordPaymentRequestPrefilledSuccess(
      bool initially_complete,
      CollectUserDataResult result);
  static void RecordPaymentRequestAutofillChanged(bool changed,
                                                  CollectUserDataResult result);
  static void RecordPaymentRequestFirstNameOnly(bool first_name_only);
  static void RecordTriggerScriptStarted(ukm::UkmRecorder* ukm_recorder,
                                         ukm::SourceId source_id,
                                         TriggerScriptStarted event);
  static void RecordTriggerScriptStarted(ukm::UkmRecorder* ukm_recorder,
                                         ukm::SourceId source_id,
                                         StartupMode startup_mode,
                                         bool feature_module_installed,
                                         bool is_first_time_user);
  static void RecordTriggerScriptFinished(
      ukm::UkmRecorder* ukm_recorder,
      ukm::SourceId source_id,
      TriggerScriptProto_TriggerUIType trigger_ui_type,
      TriggerScriptFinishedState event);
  static void RecordTriggerScriptShownToUser(
      ukm::UkmRecorder* ukm_recorder,
      ukm::SourceId source_id,
      TriggerScriptProto_TriggerUIType trigger_ui_type,
      TriggerScriptShownToUser event);
  static void RecordTriggerScriptOnboarding(
      ukm::UkmRecorder* ukm_recorder,
      ukm::SourceId source_id,
      TriggerScriptProto_TriggerUIType trigger_ui_type,
      TriggerScriptOnboarding event);
  static void RecordRegularScriptOnboarding(ukm::UkmRecorder* ukm_recorder,
                                            ukm::SourceId source_id,
                                            Metrics::Onboarding event);
  static void RecordInChromeTriggerAction(ukm::UkmRecorder* ukm_recorder,
                                          ukm::SourceId source_id,
                                          InChromeTriggerAction event);
  static void RecordTtsButtonAction(TtsButtonAction action);
  static void RecordTtsEngineEvent(TtsEngineEvent event);
  static void RecordFeatureModuleInstallation(FeatureModuleInstallation event);
  static void RecordTriggerConditionEvaluationTime(
      ukm::UkmRecorder* ukm_recorder,
      ukm::SourceId source_id,
      base::TimeDelta evaluation_time);
  static void RecordDependenciesInvalidated(
      DependenciesInvalidated dependencies_invalidated);
  static void RecordStartRequest(ukm::UkmRecorder* ukm_recorder,
                                 ukm::SourceId source_id,
                                 const ScriptParameters& script_parameters,
                                 StartupMode event);
  static void RecordContactMetrics(ukm::UkmRecorder* ukm_recorder,
                                   ukm::SourceId source_id,
                                   int complete_count,
                                   int incomplete_count,
                                   int initially_selected_field_bitmask,
                                   UserDataSelectionState selection_state);
  static void RecordCreditCardMetrics(
      ukm::UkmRecorder* ukm_recorder,
      ukm::SourceId source_id,
      int complete_count,
      int incomplete_count,
      int initially_selected_card_field_bitmask,
      int initially_selected_billing_address_field_bitmask,
      UserDataSelectionState selection_state);
  static void RecordShippingMetrics(ukm::UkmRecorder* ukm_recorder,
                                    ukm::SourceId source_id,
                                    int complete_count,
                                    int incomplete_count,
                                    int initially_selected_field_bitmask,
                                    UserDataSelectionState selection_state);
  static void RecordCollectUserDataSuccess(ukm::UkmRecorder* ukm_recorder,
                                           ukm::SourceId source_id,
                                           CollectUserDataResult result,
                                           int64_t time_taken_ms,
                                           UserDataSource source);
  static void RecordOnboardingFetcherResult(
      OnboardingFetcherResultStatus status);
  static void RecordCupRpcVerificationEvent(CupRpcVerificationEvent event);
  static void RecordServiceRequestRetryCount(int count, bool success);

  // Intended for debugging: writes string representation of |reason| to
  // |out|.
  friend std::ostream& operator<<(std::ostream& out,
                                  const DropOutReason& reason);

  // Intended for debugging: writes string representation of |metric| to |out|.
  friend std::ostream& operator<<(std::ostream& out, const Onboarding& metric);

  friend std::ostream& operator<<(std::ostream& out,
                                  const TriggerScriptFinishedState& state);
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_METRICS_H_
