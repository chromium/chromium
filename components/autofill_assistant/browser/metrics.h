// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_METRICS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_METRICS_H_

#include <ostream>
#include "base/time/time.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/startup_util.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace autofill_assistant {

// A class to generate Autofill Assistant metrics.
class Metrics {
 public:
  // The different ways that autofill assistant can stop.
  //
  // GENERATED_JAVA_ENUM_PACKAGE: (
  // org.chromium.chrome.browser.autofill_assistant.metrics)
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

    kMaxValue = ONBOARDING_DIALOG_DISMISSED
  };

  // The different ways that autofill assistant can stop. Note that this only
  // covers regular onboarding. Trigger script onboarding is covered by
  // TriggerScriptOnboarding.
  //
  // GENERATED_JAVA_ENUM_PACKAGE: (
  // org.chromium.chrome.browser.autofill_assistant.metrics)
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: OnBoarding
  //
  // This enum is used in histograms, do not remove/renumber entries. Only add
  // at the end and update kMaxValue. Also remember to update the
  // AutofillAssistantOnBoarding enum listing in
  // tools/metrics/histograms/enums.xml.
  enum class OnBoarding {
    OB_SHOWN = 0,
    OB_NOT_SHOWN = 1,
    OB_ACCEPTED = 2,
    OB_CANCELLED = 3,
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
    PREFILLED_SUCCESS = 0,
    NOTPREFILLED_SUCCESS = 1,
    PREFILLED_FAILURE = 2,
    NOTPREFILLED_FAILURE = 3,

    kMaxValue = NOTPREFILLED_FAILURE
  };

  // Whether autofill info was changed during an autofill assistant payment
  // request, or not.
  //
  // This enum is used in histograms, do not remove/renumber entries. Only add
  // at the end and update kMaxValue. Also remember to update the
  // AutofillAssistantPaymentRequestAutofillInfoChanged enum listing
  // in tools/metrics/histograms/enums.xml.
  enum class PaymentRequestAutofillInfoChanged {
    CHANGED_SUCCESS = 0,
    NOTCHANGED_SUCCESS = 1,
    CHANGED_FAILURE = 2,
    NOTCHANGED_FAILURE = 3,

    kMaxValue = NOTCHANGED_FAILURE
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
  // org.chromium.chrome.browser.autofill_assistant.metrics)
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

  static void RecordDropOut(DropOutReason reason, const std::string& intent);
  static void RecordPaymentRequestPrefilledSuccess(bool initially_complete,
                                                   bool success);
  static void RecordPaymentRequestAutofillChanged(bool changed, bool success);
  static void RecordPaymentRequestFirstNameOnly(bool first_name_only);
  static void RecordTriggerScriptStarted(ukm::UkmRecorder* ukm_recorder,
                                         ukm::SourceId source_id,
                                         TriggerScriptStarted event);
  static void RecordTriggerScriptStarted(ukm::UkmRecorder* ukm_recorder,
                                         ukm::SourceId source_id,
                                         StartupUtil::StartupMode startup_mode,
                                         bool feature_module_installed,
                                         bool is_first_time_user);
  static void RecordTriggerScriptFinished(
      ukm::UkmRecorder* ukm_recorder,
      ukm::SourceId source_id,
      TriggerScriptProto::TriggerUIType trigger_ui_type,
      TriggerScriptFinishedState event);
  static void RecordTriggerScriptShownToUser(
      ukm::UkmRecorder* ukm_recorder,
      ukm::SourceId source_id,
      TriggerScriptProto::TriggerUIType trigger_ui_type,
      TriggerScriptShownToUser event);
  static void RecordTriggerScriptOnboarding(
      ukm::UkmRecorder* ukm_recorder,
      ukm::SourceId source_id,
      TriggerScriptProto::TriggerUIType trigger_ui_type,
      TriggerScriptOnboarding event);
  static void RecordInChromeTriggerAction(ukm::UkmRecorder* ukm_recorder,
                                          ukm::SourceId source_id,
                                          InChromeTriggerAction event);
  static void RecordOnboardingResult(OnBoarding event);
  static void RecordTtsButtonAction(TtsButtonAction action);
  static void RecordTtsEngineEvent(TtsEngineEvent event);
  static void RecordFeatureModuleInstallation(FeatureModuleInstallation event);
  static void RecordTriggerConditionEvaluationTime(
      ukm::UkmRecorder* ukm_recorder,
      ukm::SourceId source_id,
      base::TimeDelta evaluation_time);

  // Intended for debugging: writes string representation of |reason| to
  // |out|.
  friend std::ostream& operator<<(std::ostream& out,
                                  const DropOutReason& reason) {
#ifdef NDEBUG
    // Non-debugging builds write the enum number.
    out << static_cast<int>(reason);
    return out;
#else
    // Debugging builds write a string representation of |reason|.
    switch (reason) {
      case DropOutReason::AA_START:
        out << "AA_START";
        break;
      case DropOutReason::AUTOSTART_TIMEOUT:
        out << "AUTOSTART_TIMEOUT";
        break;
      case DropOutReason::NO_SCRIPTS:
        out << "NO_SCRIPTS";
        break;
      case DropOutReason::CUSTOM_TAB_CLOSED:
        out << "CUSTOM_TAB_CLOSED";
        break;
      case DropOutReason::DECLINED:
        out << "DECLINED";
        break;
      case DropOutReason::SHEET_CLOSED:
        out << "SHEET_CLOSED";
        break;
      case DropOutReason::SCRIPT_FAILED:
        out << "SCRIPT_FAILED";
        break;
      case DropOutReason::NAVIGATION:
        out << "NAVIGATION";
        break;
      case DropOutReason::OVERLAY_STOP:
        out << "OVERLAY_STOP";
        break;
      case DropOutReason::PR_FAILED:
        out << "PR_FAILED";
        break;
      case DropOutReason::CONTENT_DESTROYED:
        out << "CONTENT_DESTROYED";
        break;
      case DropOutReason::RENDER_PROCESS_GONE:
        out << "RENDER_PROCESS_GONE";
        break;
      case DropOutReason::INTERSTITIAL_PAGE:
        out << "INTERSTITIAL_PAGE";
        break;
      case DropOutReason::SCRIPT_SHUTDOWN:
        out << "SCRIPT_SHUTDOWN";
        break;
      case DropOutReason::SAFETY_NET_TERMINATE:
        out << "SAFETY_NET_TERMINATE";
        break;
      case DropOutReason::TAB_DETACHED:
        out << "TAB_DETACHED";
        break;
      case DropOutReason::TAB_CHANGED:
        out << "TAB_CHANGED";
        break;
      case DropOutReason::GET_SCRIPTS_FAILED:
        out << "GET_SCRIPTS_FAILED";
        break;
      case DropOutReason::GET_SCRIPTS_UNPARSABLE:
        out << "GET_SCRIPTS_UNPARSEABLE";
        break;
      case DropOutReason::NO_INITIAL_SCRIPTS:
        out << "NO_INITIAL_SCRIPTS";
        break;
      case DropOutReason::DFM_INSTALL_FAILED:
        out << "DFM_INSTALL_FAILED";
        break;
      case DropOutReason::DOMAIN_CHANGE_DURING_BROWSE_MODE:
        out << "DOMAIN_CHANGE_DURING_BROWSE_MODE";
        break;
      case DropOutReason::BACK_BUTTON_CLICKED:
        out << "BACK_BUTTON_CLICKED";
        break;
      case DropOutReason::ONBOARDING_BACK_BUTTON_CLICKED:
        out << "ONBOARDING_BACK_BUTTON_CLICKED";
        break;
      case DropOutReason::NAVIGATION_WHILE_RUNNING:
        out << "NAVIGATION_WHILE_RUNNING";
        break;
      case DropOutReason::UI_CLOSED_UNEXPECTEDLY:
        out << "UI_CLOSED_UNEXPECTEDLY";
        break;
      case DropOutReason::ONBOARDING_NAVIGATION:
        out << "ONBOARDING_NAVIGATION";
        break;
      case DropOutReason::ONBOARDING_DIALOG_DISMISSED:
        out << "ONBOARDING_DIALOG_DISMISSED";
        break;
        // Do not add default case to force compilation error for new values.
    }
    return out;
#endif  // NDEBUG
  }

  // Intended for debugging: writes string representation of |metric| to |out|.
  friend std::ostream& operator<<(std::ostream& out, const OnBoarding& metric) {
#ifdef NDEBUG
    // Non-debugging builds write the enum number.
    out << static_cast<int>(metric);
    return out;
#else
    // Debugging builds write a string representation of |metric|.
    switch (metric) {
      case OnBoarding::OB_SHOWN:
        out << "OB_SHOWN";
        break;
      case OnBoarding::OB_NOT_SHOWN:
        out << "OB_NOT_SHOWN";
        break;
      case OnBoarding::OB_ACCEPTED:
        out << "OB_ACCEPTED";
        break;
      case OnBoarding::OB_CANCELLED:
        out << "OB_CANCELLED";
        break;
      case OnBoarding::OB_NO_ANSWER:
        out << "OB_NO_ANSWER";
        break;
        // Do not add default case to force compilation error for new values.
    }
    return out;
#endif  // NDEBUG
  }

  friend std::ostream& operator<<(std::ostream& out,
                                  const TriggerScriptFinishedState& state) {
#ifdef NDEBUG
    // Non-debugging builds write the enum number.
    out << static_cast<int>(state);
    return out;
#else
    // Debugging builds write a string representation of |state|.
    switch (state) {
      case TriggerScriptFinishedState::GET_ACTIONS_FAILED:
        out << "GET_ACTIONS_FAILED";
        break;
      case TriggerScriptFinishedState::GET_ACTIONS_PARSE_ERROR:
        out << "GET_ACTIONS_PARSE_ERROR";
        break;
      case TriggerScriptFinishedState::PROMPT_FAILED_NAVIGATE:
        out << "PROMPT_FAILED_NAVIGATE";
        break;
      case TriggerScriptFinishedState::PROMPT_SUCCEEDED:
        out << "PROMPT_SUCCEEDED";
        break;
      case TriggerScriptFinishedState::PROMPT_FAILED_CANCEL_SESSION:
        out << "PROMPT_FAILED_CANCEL_SESSION";
        break;
      case TriggerScriptFinishedState::PROMPT_FAILED_CANCEL_FOREVER:
        out << "PROMPT_FAILED_CANCEL_FOREVER";
        break;
      case TriggerScriptFinishedState::TRIGGER_CONDITION_TIMEOUT:
        out << "TRIGGER_CONDITION_TIMEOUT";
        break;
      case TriggerScriptFinishedState::NAVIGATION_ERROR:
        out << "NAVIGATION_ERROR";
        break;
      case TriggerScriptFinishedState::WEB_CONTENTS_DESTROYED_WHILE_VISIBLE:
        out << "WEB_CONTENTS_DESTROYED_WHILE_VISIBLE";
        break;
      case TriggerScriptFinishedState::WEB_CONTENTS_DESTROYED_WHILE_INVISIBLE:
        out << "WEB_CONTENTS_DESTROYED_WHILE_INVISIBLE";
        break;
      case TriggerScriptFinishedState::NO_TRIGGER_SCRIPT_AVAILABLE:
        out << "NO_TRIGGER_SCRIPT_AVAILABLE";
        break;
      case TriggerScriptFinishedState::FAILED_TO_SHOW:
        out << "FAILED_TO_SHOW";
        break;
      case TriggerScriptFinishedState::DISABLED_PROACTIVE_HELP_SETTING:
        out << "DISABLED_PROACTIVE_HELP_SETTING";
        break;
      case TriggerScriptFinishedState::BASE64_DECODING_ERROR:
        out << "BASE64_DECODING_ERROR";
        break;
      case TriggerScriptFinishedState::BOTTOMSHEET_ONBOARDING_REJECTED:
        out << "BOTTOMSHEET_ONBOARDING_REJECTED";
        break;
      case TriggerScriptFinishedState::UNKNOWN_FAILURE:
        out << "UNKNOWN_FAILURE";
        break;
      case TriggerScriptFinishedState::SERVICE_DELETED:
        out << "SERVICE_DELETED";
        break;
      case TriggerScriptFinishedState::PATH_MISMATCH:
        out << "PATH_MISMATCH";
        break;
      case TriggerScriptFinishedState::UNSAFE_ACTIONS:
        out << "UNSAFE_ACTIONS";
        break;
      case TriggerScriptFinishedState::INVALID_SCRIPT:
        out << "INVALID_SCRIPT";
        break;
      case TriggerScriptFinishedState::BROWSE_FAILED_NAVIGATE:
        out << "BROWSE_FAILED_NAVIGATE";
        break;
      case TriggerScriptFinishedState::BROWSE_FAILED_OTHER:
        out << "BROWSE_FAILED_OTHER";
        break;
      case TriggerScriptFinishedState::PROMPT_FAILED_CONDITION_NO_LONGER_TRUE:
        out << "PROMPT_FAILED_CONDITION_NO_LONGER_TRUE";
        break;
      case TriggerScriptFinishedState::PROMPT_FAILED_CLOSE:
        out << "PROMPT_FAILED_CLOSE";
        break;
      case TriggerScriptFinishedState::PROMPT_FAILED_OTHER:
        out << "PROMPT_FAILED_OTHER";
        break;
      case TriggerScriptFinishedState::PROMPT_SWIPE_DISMISSED:
        out << "PROMPT_SWIPE_DISMISSED";
        break;
      case TriggerScriptFinishedState::CCT_TO_TAB_NOT_SUPPORTED:
        out << "CCT_TO_TAB_NOT_SUPPORTED";
        break;
      case TriggerScriptFinishedState::CANCELED:
        out << "CANCELED";
        break;
        // Do not add default case to force compilation error for new values.
    }
    return out;
#endif  // NDEBUG
  }
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_METRICS_H_
