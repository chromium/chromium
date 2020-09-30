// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_METRICS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_METRICS_H_

#include <ostream>

namespace autofill_assistant {

// A class to generate Autofill Assistant related histograms.
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

    kMaxValue = UI_CLOSED_UNEXPECTEDLY
  };

  // The different ways that autofill assistant can stop.
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
    LITE_SCRIPT_RUNNING = 0,
    // The subset of |LITE_SCRIPT_RUNNING| where the lite script prompt was
    // shown.
    LITE_SCRIPT_SHOWN_TO_USER = 1,

    kMaxValue = LITE_SCRIPT_SHOWN_TO_USER
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
    // User has explicitly rejected the lite script two times and thus opted
    // out of  the experience.
    LITE_SCRIPT_CANCELED_TWO_TIMES = 1,
    // User has rejected the onboarding and thus opted out of the experience.
    LITE_SCRIPT_ONBOARDING_REJECTED = 2,
    // User has not seen the lite script before and will see first time
    // experience.
    LITE_SCRIPT_FIRST_TIME_USER = 3,
    // User has seen the first-time experience before and will see returning
    // user experience.
    LITE_SCRIPT_RETURNING_USER = 4,

    kMaxValue = LITE_SCRIPT_RETURNING_USER
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
    // The lite script failed for an unknown reason.
    LITE_SCRIPT_UNKNOWN_FAILURE = 0,
    // Can happen when users close the tab or similar.
    LITE_SCRIPT_SERVICE_DELETED = 1,
    // |GetActions| was asked to retrieve a wrong script path.
    LITE_SCRIPT_PATH_MISMATCH = 2,
    // Communication with backend failed.
    LITE_SCRIPT_GET_ACTIONS_FAILED = 3,
    // Failed to parse the proto response to |GetActions|.
    LITE_SCRIPT_GET_ACTIONS_PARSE_ERROR = 4,
    // One or multiple unsafe actions were contained in script.
    LITE_SCRIPT_UNSAFE_ACTIONS = 5,
    // The mini script is invalid. A valid script must contain a prompt
    // (browse=true) action and end in a prompt(browse=false) action.
    LITE_SCRIPT_INVALID_SCRIPT = 6,

    // The prompt(browse) action failed due to a navigation event to a
    // non-whitelisted domain.
    LITE_SCRIPT_BROWSE_FAILED_NAVIGATE = 7,
    // The prompt(browse) action failed for an unknown reason.
    LITE_SCRIPT_BROWSE_FAILED_OTHER = 8,

    // The prompt(regular) action failed due to a navigation event to a
    // non-whitelisted domain.
    LITE_SCRIPT_PROMPT_FAILED_NAVIGATE = 9,
    // The prompt(regular) action failed because the condition to show it was no
    // longer true.
    LITE_SCRIPT_PROMPT_FAILED_CONDITION_NO_LONGER_TRUE = 10,
    // The prompt(regular) action failed because the user tapped the close chip.
    LITE_SCRIPT_PROMPT_FAILED_CLOSE = 11,
    // The prompt(regular) action failed for an unknown reason.
    LITE_SCRIPT_PROMPT_FAILED_OTHER = 12,
    // The prompt(regular) action succeeded because the user tapped the continue
    // chip.
    LITE_SCRIPT_PROMPT_SUCCEEDED = 13,

    kMaxValue = LITE_SCRIPT_PROMPT_SUCCEEDED
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

    kMaxValue = LITE_SCRIPT_ONBOARDING_ALREADY_ACCEPTED
  };

  static void RecordDropOut(DropOutReason reason);
  static void RecordPaymentRequestPrefilledSuccess(bool initially_complete,
                                                   bool success);
  static void RecordPaymentRequestAutofillChanged(bool changed, bool success);
  static void RecordPaymentRequestFirstNameOnly(bool first_name_only);
  static void RecordPaymentRequestMandatoryPostalCode(bool required,
                                                      bool initially_right,
                                                      bool success);

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
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_METRICS_H_
