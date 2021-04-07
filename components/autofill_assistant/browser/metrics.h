// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_METRICS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_METRICS_H_

#include <ostream>
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/startup_util.h"
#include "content/public/browser/web_contents.h"
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
  // LiteScriptOnboarding.
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

  // Whether a lite script was running invisibly or visible to the user.
  //
  // GENERATED_JAVA_ENUM_PACKAGE: (
  // org.chromium.chrome.browser.autofill_assistant.metrics)
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: LiteScriptShownToUser
  //
  // This enum is used in UKM metrics, do not remove/renumber entries. Only add
  // at the end and update kMaxValue. Also remember to update the
  // AutofillAssistantLiteScriptShownToUser enum listing in
  // tools/metrics/histograms/enums.xml and the description in
  // tools/metrics/ukm/ukm.xml as necessary.
  enum class LiteScriptShownToUser {
    // The number of times a lite script was successfully fetched and started.
    // Can happen multiple times per run (in case of tab switch).
    LITE_SCRIPT_RUNNING = 0,
    // The number of times a lite script was shown to the user. Can happen
    // multiple times per run.
    LITE_SCRIPT_SHOWN_TO_USER = 1,
    // Since Chrome M-88. The user tapped the 'not now' button. Can happen
    // multiple times per run.
    LITE_SCRIPT_NOT_NOW = 2,
    // Since Chrome M-88. The lite script was automatically hidden due to the
    // trigger condition no longer being true. Can happen multiple times per
    // run.
    LITE_SCRIPT_HIDE_ON_TRIGGER_CONDITION_NO_LONGER_TRUE = 3,
    // Since Chrome M-88. The user swipe-dismissed the bottom sheet. Depending
    // on configuration, this may happen multiple times per run.
    LITE_SCRIPT_SWIPE_DISMISSED = 4,

    kMaxValue = LITE_SCRIPT_SWIPE_DISMISSED
  };

  // The different ways a user might have opted out of the lite script
  // experience.
  //
  // GENERATED_JAVA_ENUM_PACKAGE: (
  // org.chromium.chrome.browser.autofill_assistant.metrics)
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: LiteScriptStarted
  //
  // This enum is used in UKM metrics, do not remove/renumber entries. Only add
  // at the end and update kMaxValue. Also remember to update the
  // AutofillAssistantLiteScriptStarted enum listing in
  // tools/metrics/histograms/enums.xml and the description in
  // tools/metrics/ukm/ukm.xml as necessary.
  enum class LiteScriptStarted {
    // Device did not have DFM downloaded.
    LITE_SCRIPT_DFM_UNAVAILABLE = 0,
    // User has not seen the lite script before and will see first time
    // experience.
    LITE_SCRIPT_FIRST_TIME_USER = 3,
    // User has seen the first-time experience before and will see returning
    // user experience.
    LITE_SCRIPT_RETURNING_USER = 4,
    // Since Chrome M-88. The proactive trigger setting is disabled. The user
    // has either chosen 'never show again' in the prompt or manually disabled
    // the setting in Chrome settings.
    LITE_SCRIPT_PROACTIVE_TRIGGERING_DISABLED = 5,
    // Since Chrome M-88. Intended as a catch-all. This is reported as soon as a
    // lite-script intent is received (of course, only for people with MSBB
    // enabled).
    LITE_SCRIPT_INTENT_RECEIVED = 6,
    // Since Chrome M-91. A required Chrome feature was disabled.
    LITE_SCRIPT_FEATURE_DISABLED = 7,
    // Since Chrome M-91. No initial url was set, neither in ORIGINAL_DEEPLINK
    // nor in the intent data.
    LITE_SCRIPT_NO_INITIAL_URL = 8,
    // Since Chrome M-91. A mandatory script parameter was missing.
    LITE_SCRIPT_MANDATORY_PARAMETER_MISSING = 9,

    // DEPRECATED, only sent by Chrome M-86 and M-87.
    //
    // User has explicitly rejected the lite script two times and thus opted
    // out of  the experience.
    LITE_SCRIPT_CANCELED_TWO_TIMES = 1,
    // User has rejected the onboarding and thus opted out of the experience.
    LITE_SCRIPT_ONBOARDING_REJECTED = 2,

    kMaxValue = LITE_SCRIPT_MANDATORY_PARAMETER_MISSING
  };

  // The different ways in which a lite script may finish.
  //
  // GENERATED_JAVA_ENUM_PACKAGE: (
  // org.chromium.chrome.browser.autofill_assistant.metrics)
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: LiteScriptFinishedState
  //
  // This enum is used in UKM metrics, do not remove/renumber entries. Only add
  // at the end and update kMaxValue. Also remember to update the
  // AutofillAssistantLiteScriptFinished enum listing in
  // tools/metrics/histograms/enums.xml and the description in
  // tools/metrics/ukm/ukm.xml as necessary.
  enum class LiteScriptFinishedState {
    // Communication with backend failed.
    LITE_SCRIPT_GET_ACTIONS_FAILED = 3,
    // Failed to parse the proto sent by the backend.
    LITE_SCRIPT_GET_ACTIONS_PARSE_ERROR = 4,
    // Lite script failed due to a navigation event to a non-allowed domain.
    LITE_SCRIPT_PROMPT_FAILED_NAVIGATE = 9,
    // Lite script succeeded. The user accepted the prompt.
    LITE_SCRIPT_PROMPT_SUCCEEDED = 13,
    // Since Chrome M-88. The user tapped the 'cancel for this session' button.
    LITE_SCRIPT_PROMPT_FAILED_CANCEL_SESSION = 14,
    // Since Chrome M-88. The user tapped the 'never show again' button.
    LITE_SCRIPT_PROMPT_FAILED_CANCEL_FOREVER = 15,
    // Since Chrome M-88. The trigger script has timed out. This indicates that
    // trigger conditions were evaluated for >= timeout without success. Time is
    // only counted while the tab is visible and the lite script is invisible.
    // The timeout resets on tab change.
    LITE_SCRIPT_TRIGGER_CONDITION_TIMEOUT = 17,
    // Since Chrome M-88. A navigation error occurred, leading to Chrome showing
    // an error page.
    LITE_SCRIPT_NAVIGATION_ERROR = 18,
    // Since Chrome M-88. The tab was closed while the prompt was visible.
    LITE_SCRIPT_WEB_CONTENTS_DESTROYED_WHILE_VISIBLE = 19,
    // Since Chrome M-88. The tab was closed while the prompt was invisible.
    LITE_SCRIPT_WEB_CONTENTS_DESTROYED_WHILE_INVISIBLE = 20,
    // Since Chrome M-88. The RPC to fetch the trigger scripts returned with an
    // empty response.
    LITE_SCRIPT_NO_TRIGGER_SCRIPT_AVAILABLE = 21,
    // Since Chrome M-88. The trigger script failed to show. This can happen,
    // for example, if the activity was changed after triggering (e.g.,
    // switching from CCT to regular tab).
    LITE_SCRIPT_FAILED_TO_SHOW = 22,
    // Since Chrome M-88. The proactive help switch was enabled at start, but
    // then manually disabled in the Chrome settings.
    LITE_SCRIPT_DISABLED_PROACTIVE_HELP_SETTING = 23,
    // Since Chrome M-88. The client failed to base64-decode the trigger script
    // specified in the script parameters.
    LITE_SCRIPT_BASE64_DECODING_ERROR = 24,
    // The user rejected the bottom sheet onboarding
    LITE_SCRIPT_BOTTOMSHEET_ONBOARDING_REJECTED = 25,

    // NOTE: All values in this block are DEPRECATED and will only be sent by
    // Chrome M-86 and M-87.
    //
    // The lite script failed for an unknown reason.
    LITE_SCRIPT_UNKNOWN_FAILURE = 0,
    // Can happen when users close the tab or similar.
    LITE_SCRIPT_SERVICE_DELETED = 1,
    // |GetActions| was asked to retrieve a wrong script path.
    LITE_SCRIPT_PATH_MISMATCH = 2,
    // One or multiple unsafe actions were contained in script.
    LITE_SCRIPT_UNSAFE_ACTIONS = 5,
    // The mini script is invalid. A valid script must contain a prompt
    // (browse=true) action and end in a prompt(browse=false) action.
    LITE_SCRIPT_INVALID_SCRIPT = 6,
    // The prompt(browse) action failed due to a navigation event to a
    // non-allowed domain.
    LITE_SCRIPT_BROWSE_FAILED_NAVIGATE = 7,
    // The prompt(browse) action failed for an unknown reason.
    LITE_SCRIPT_BROWSE_FAILED_OTHER = 8,
    // The prompt(regular) action failed because the condition to show it was no
    // longer true.
    LITE_SCRIPT_PROMPT_FAILED_CONDITION_NO_LONGER_TRUE = 10,
    // The prompt(regular) action failed because the user tapped the close chip.
    LITE_SCRIPT_PROMPT_FAILED_CLOSE = 11,
    // The prompt(regular) action failed for an unknown reason.
    LITE_SCRIPT_PROMPT_FAILED_OTHER = 12,
    // Since Chrome M-88. The bottom sheet was swipe-dismissed by the user.
    LITE_SCRIPT_PROMPT_SWIPE_DISMISSED = 16,

    kMaxValue = LITE_SCRIPT_BOTTOMSHEET_ONBOARDING_REJECTED
  };

  // The different ways a user who has successfully completed a light script may
  // accept or reject the onboarding
  //
  // GENERATED_JAVA_ENUM_PACKAGE: (
  // org.chromium.chrome.browser.autofill_assistant.metrics)
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: LiteScriptOnboarding
  //
  // This enum is used in UKM metrics, do not remove/renumber entries. Only add
  // at the end and update kMaxValue. Also remember to update the
  // AutofillAssistantLiteScriptOnboarding enum listing in
  // tools/metrics/histograms/enums.xml and the description in
  // tools/metrics/ukm/ukm.xml as necessary.
  enum class LiteScriptOnboarding {
    // The user has seen and accepted the onboarding.
    LITE_SCRIPT_ONBOARDING_SEEN_AND_ACCEPTED = 0,
    // The user has seen and rejected the onboarding.
    LITE_SCRIPT_ONBOARDING_SEEN_AND_REJECTED = 1,
    // The user has already accepted the onboarding in the past.
    LITE_SCRIPT_ONBOARDING_ALREADY_ACCEPTED = 2,
    // The user has seen and dismissed the onboarding.
    LITE_SCRIPT_ONBOARDING_SEEN_AND_DISMISSED = 3,
    // The onboarding was interrupted by a website navigation.
    LITE_SCRIPT_ONBOARDING_SEEN_AND_INTERRUPTED_BY_NAVIGATION = 4,

    kMaxValue = LITE_SCRIPT_ONBOARDING_SEEN_AND_INTERRUPTED_BY_NAVIGATION
  };

  static void RecordDropOut(DropOutReason reason, const std::string& intent);
  static void RecordPaymentRequestPrefilledSuccess(bool initially_complete,
                                                   bool success);
  static void RecordPaymentRequestAutofillChanged(bool changed, bool success);
  static void RecordPaymentRequestFirstNameOnly(bool first_name_only);
  static void RecordPaymentRequestMandatoryPostalCode(bool required,
                                                      bool initially_right,
                                                      bool success);
  static void RecordLiteScriptStarted(ukm::UkmRecorder* ukm_recorder,
                                      content::WebContents* web_contents,
                                      StartupUtil::StartupMode startup_mode,
                                      bool feature_module_installed,
                                      bool is_first_time_user);
  static void RecordLiteScriptFinished(ukm::UkmRecorder* ukm_recorder,
                                       content::WebContents* web_contents,
                                       TriggerUIType trigger_ui_type,
                                       LiteScriptFinishedState event);
  static void RecordLiteScriptShownToUser(ukm::UkmRecorder* ukm_recorder,
                                          content::WebContents* web_contents,
                                          TriggerUIType trigger_ui_type,
                                          LiteScriptShownToUser event);
  static void RecordLiteScriptOnboarding(ukm::UkmRecorder* ukm_recorder,
                                         content::WebContents* web_contents,
                                         TriggerUIType trigger_ui_type,
                                         LiteScriptOnboarding event);
  static void RecordOnboardingResult(OnBoarding event);

  // Intended for debugging: writes string representation of |reason| to |out|.
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
                                  const LiteScriptFinishedState& state) {
#ifdef NDEBUG
    // Non-debugging builds write the enum number.
    out << static_cast<int>(state);
    return out;
#else
    // Debugging builds write a string representation of |state|.
    switch (state) {
      case LiteScriptFinishedState::LITE_SCRIPT_GET_ACTIONS_FAILED:
        out << "LITE_SCRIPT_GET_ACTIONS_FAILED";
        break;
      case LiteScriptFinishedState::LITE_SCRIPT_GET_ACTIONS_PARSE_ERROR:
        out << "LITE_SCRIPT_GET_ACTIONS_PARSE_ERROR";
        break;
      case LiteScriptFinishedState::LITE_SCRIPT_PROMPT_FAILED_NAVIGATE:
        out << "LITE_SCRIPT_PROMPT_FAILED_NAVIGATE";
        break;
      case LiteScriptFinishedState::LITE_SCRIPT_PROMPT_SUCCEEDED:
        out << "LITE_SCRIPT_PROMPT_SUCCEEDED";
        break;
      case LiteScriptFinishedState::LITE_SCRIPT_PROMPT_FAILED_CANCEL_SESSION:
        out << "LITE_SCRIPT_PROMPT_FAILED_CANCEL_SESSION";
        break;
      case LiteScriptFinishedState::LITE_SCRIPT_PROMPT_FAILED_CANCEL_FOREVER:
        out << "LITE_SCRIPT_PROMPT_FAILED_CANCEL_FOREVER";
        break;
      case LiteScriptFinishedState::LITE_SCRIPT_TRIGGER_CONDITION_TIMEOUT:
        out << "LITE_SCRIPT_TRIGGER_CONDITION_TIMEOUT";
        break;
      case LiteScriptFinishedState::LITE_SCRIPT_NAVIGATION_ERROR:
        out << "LITE_SCRIPT_NAVIGATION_ERROR";
        break;
      case LiteScriptFinishedState::
          LITE_SCRIPT_WEB_CONTENTS_DESTROYED_WHILE_VISIBLE:
        out << "LITE_SCRIPT_WEB_CONTENTS_DESTROYED_WHILE_VISIBLE";
        break;
      case LiteScriptFinishedState::
          LITE_SCRIPT_WEB_CONTENTS_DESTROYED_WHILE_INVISIBLE:
        out << "LITE_SCRIPT_WEB_CONTENTS_DESTROYED_WHILE_INVISIBLE";
        break;
      case LiteScriptFinishedState::LITE_SCRIPT_NO_TRIGGER_SCRIPT_AVAILABLE:
        out << "LITE_SCRIPT_NO_TRIGGER_SCRIPT_AVAILABLE";
        break;
      case LiteScriptFinishedState::LITE_SCRIPT_FAILED_TO_SHOW:
        out << "LITE_SCRIPT_FAILED_TO_SHOW";
        break;
      case LiteScriptFinishedState::LITE_SCRIPT_DISABLED_PROACTIVE_HELP_SETTING:
        out << "LITE_SCRIPT_DISABLED_PROACTIVE_HELP_SETTING";
        break;
      case LiteScriptFinishedState::LITE_SCRIPT_BASE64_DECODING_ERROR:
        out << "LITE_SCRIPT_BASE64_DECODING_ERROR";
        break;
      case LiteScriptFinishedState::LITE_SCRIPT_BOTTOMSHEET_ONBOARDING_REJECTED:
        out << "LITE_SCRIPT_BOTTOMSHEET_ONBOARDING_REJECTED";
        break;
      case LiteScriptFinishedState::LITE_SCRIPT_UNKNOWN_FAILURE:
        out << "LITE_SCRIPT_UNKNOWN_FAILURE";
        break;
      case LiteScriptFinishedState::LITE_SCRIPT_SERVICE_DELETED:
        out << "LITE_SCRIPT_SERVICE_DELETED";
        break;
      case LiteScriptFinishedState::LITE_SCRIPT_PATH_MISMATCH:
        out << "LITE_SCRIPT_PATH_MISMATCH";
        break;
      case LiteScriptFinishedState::LITE_SCRIPT_UNSAFE_ACTIONS:
        out << "LITE_SCRIPT_UNSAFE_ACTIONS";
        break;
      case LiteScriptFinishedState::LITE_SCRIPT_INVALID_SCRIPT:
        out << "LITE_SCRIPT_INVALID_SCRIPT";
        break;
      case LiteScriptFinishedState::LITE_SCRIPT_BROWSE_FAILED_NAVIGATE:
        out << "LITE_SCRIPT_BROWSE_FAILED_NAVIGATE";
        break;
      case LiteScriptFinishedState::LITE_SCRIPT_BROWSE_FAILED_OTHER:
        out << "LITE_SCRIPT_BROWSE_FAILED_OTHER";
        break;
      case LiteScriptFinishedState::
          LITE_SCRIPT_PROMPT_FAILED_CONDITION_NO_LONGER_TRUE:
        out << "LITE_SCRIPT_PROMPT_FAILED_CONDITION_NO_LONGER_TRUE";
        break;
      case LiteScriptFinishedState::LITE_SCRIPT_PROMPT_FAILED_CLOSE:
        out << "LITE_SCRIPT_PROMPT_FAILED_CLOSE";
        break;
      case LiteScriptFinishedState::LITE_SCRIPT_PROMPT_FAILED_OTHER:
        out << "LITE_SCRIPT_PROMPT_FAILED_OTHER";
        break;
      case LiteScriptFinishedState::LITE_SCRIPT_PROMPT_SWIPE_DISMISSED:
        out << "LITE_SCRIPT_PROMPT_SWIPE_DISMISSED";
        // Do not add default case to force compilation error for new values.
    }
    return out;
#endif  // NDEBUG
  }
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_METRICS_H_
