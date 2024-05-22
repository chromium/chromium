// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_SAVE_PASSWORD_PROGRESS_LOGGER_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_SAVE_PASSWORD_PROGRESS_LOGGER_H_

#include <stddef.h>

#include <string>

#include "components/autofill/core/common/form_data.h"
#include "url/gurl.h"

namespace base {
class Value;
}

namespace autofill {

// When logging decisions made by password management code about whether to
// offer user-entered credentials for saving or not, do use this class. It
// offers a suite of convenience methods to format and scrub logs. The methods
// have built-in privacy protections (never include a password, scrub URLs), so
// that the result is appropriate for display on the internals page.
//
// To use this class, the method SendLog needs to be overriden to send the logs
// for display as appropriate.
class SavePasswordProgressLogger {
 public:
  // IDs of strings allowed in the logs: for security reasons, we only pass the
  // IDs from the renderer, and map them to strings in the browser.
  enum StringID {
    STRING_DECISION_ASK,
    STRING_DECISION_DROP,
    STRING_DECISION_SAVE,
    STRING_OTHER,
    STRING_SCHEME_HTML,
    STRING_SCHEME_BASIC,
    STRING_SCHEME_DIGEST,
    STRING_SCHEME_USERNAME_ONLY,
    STRING_SCHEME_MESSAGE,
    STRING_SIGNON_REALM,
    STRING_ORIGIN,
    STRING_ACTION,
    STRING_USERNAME_ELEMENT,
    STRING_USERNAME_ELEMENT_RENDERER_ID,
    STRING_PASSWORD_ELEMENT,
    STRING_PASSWORD_ELEMENT_RENDERER_ID,
    STRING_NEW_PASSWORD_ELEMENT,
    STRING_NEW_PASSWORD_ELEMENT_RENDERER_ID,
    STRING_CONFIRMATION_PASSWORD_ELEMENT,
    STRING_CONFIRMATION_PASSWORD_ELEMENT_RENDERER_ID,
    STRING_PASSWORD_GENERATED,
    STRING_TIMES_USED,
    STRING_NAME_OR_ID,
    STRING_MESSAGE,
    STRING_SET_AUTH_METHOD,
    STRING_AUTHENTICATION_HANDLED,
    STRING_LOGINHANDLER_FORM,
    STRING_SEND_PASSWORD_FORMS_METHOD,
    STRING_SECURITY_ORIGIN,
    STRING_SECURITY_ORIGIN_FAILURE,
    STRING_WEBPAGE_EMPTY,
    STRING_NUMBER_OF_ALL_FORMS,
    STRING_FORM_FOUND_ON_PAGE,
    STRING_FORM_IS_VISIBLE,
    STRING_FORM_IS_PASSWORD,
    STRING_HTML_FORM_FOR_SUBMIT,
    STRING_DID_START_PROVISIONAL_LOAD_METHOD,
    STRING_FRAME_NOT_MAIN_FRAME,
    STRING_PROVISIONALLY_SAVE_FORM_METHOD,
    STRING_EMPTY_PASSWORD,
    STRING_MATCHING_NOT_COMPLETE,
    STRING_INVALID_FORM,
    STRING_SYNC_CREDENTIAL,
    STRING_BLOCK_PASSWORD_SAME_ORIGIN_INSECURE_SCHEME,
    STRING_ON_PASSWORD_FORMS_RENDERED_METHOD,
    STRING_ON_DYNAMIC_FORM_SUBMISSION,
    STRING_ON_PASSWORD_FORM_CLEARED,
    STRING_ON_SUBFRAME_FORM_SUBMISSION,
    STRING_ON_ASK_USER_OR_SAVE_PASSWORD,
    STRING_CAN_PROVISIONAL_MANAGER_SAVE_METHOD,
    STRING_NO_PROVISIONAL_SAVE_MANAGER,
    STRING_ANOTHER_MANAGER_WAS_SUBMITTED,
    STRING_NUMBER_OF_VISIBLE_FORMS,
    STRING_PASSWORD_FORM_REAPPEARED,
    STRING_SAVING_DISABLED,
    STRING_NO_MATCHING_FORM,
    STRING_SSL_ERRORS_PRESENT,
    STRING_ONLY_VISIBLE,
    STRING_SHOW_PASSWORD_PROMPT,
    STRING_PASSWORDMANAGER_AUTOFILL,
    STRING_WAIT_FOR_USERNAME,
    STRING_WAS_LAST_NAVIGATION_HTTP_ERROR_METHOD,
    STRING_HTTP_STATUS_CODE,
    STRING_PROVISIONALLY_SAVED_FORM_IS_NOT_HTML,
    STRING_ON_GET_STORE_RESULTS_METHOD,
    STRING_NUMBER_RESULTS,
    STRING_FETCH_METHOD,
    STRING_NO_STORE,
    STRING_CREATE_LOGIN_MANAGERS_METHOD,
    STRING_PASSWORD_MANAGEMENT_ENABLED_FOR_CURRENT_PAGE,
    STRING_SHOW_LOGIN_PROMPT_METHOD,
    STRING_NEW_UI_STATE,
    STRING_FORM_SIGNATURE,
    STRING_ALTERNATIVE_FORM_SIGNATURE,
    STRING_FORM_FETCHER_STATE,
    STRING_UNOWNED_INPUTS_VISIBLE,
    STRING_ON_FILL_PASSWORD_FORM_METHOD,
    STRING_FORM_DATA_WAIT,
    STRING_FILL_USERNAME_AND_PASSWORD_METHOD,
    STRING_USERNAMES_MATCH,
    STRING_MATCH_IN_ADDITIONAL,
    STRING_USERNAME_FILLED,
    STRING_PASSWORD_FILLED,
    STRING_FORM_NAME,
    STRING_FIELDS,
    STRING_FIRSTUSE_FORM_VOTE,
    STRING_PASSWORD_FORM_VOTE,
    STRING_REUSE_FOUND,
    STRING_GENERATION_DISABLED_SAVING_DISABLED,
    STRING_GENERATION_DISABLED_CHROME_DOES_NOT_SYNC_PASSWORDS,
    STRING_GENERATION_DISABLED_NOT_ABLE_TO_SAVE_PASSWORDS,
    STRING_GENERATION_DISABLED_NO_SYNC,
    STRING_GENERATION_RENDERER_AUTOMATIC_GENERATION_AVAILABLE,
    STRING_GENERATION_RENDERER_SHOW_GENERATION_POPUP,
    STRING_GENERATION_RENDERER_GENERATED_PASSWORD_ACCEPTED,
    STRING_SUCCESSFUL_SUBMISSION_INDICATOR_EVENT,
    STRING_MAIN_FRAME_ORIGIN,
    STRING_IS_FORM_TAG,
    STRING_FORM_PARSING_INPUT,
    STRING_FORM_PARSING_OUTPUT,
    STRING_FAILED_TO_FILL_INTO_IFRAME,
    STRING_FAILED_TO_FILL_NO_AUTOCOMPLETEABLE_ELEMENT,
    STRING_FAILED_TO_FILL_PREFILLED_USERNAME,
    STRING_FAILED_TO_FILL_FOUND_NO_PASSWORD_FOR_USERNAME,
    STRING_HTTPAUTH_ON_ASK_USER_OR_SAVE_PASSWORD,
    STRING_HTTPAUTH_ON_PROMPT_USER,
    STRING_HTTPAUTH_ON_SET_OBSERVER,
    STRING_HTTPAUTH_ON_DETACH_OBSERVER,
    STRING_LEAK_DETECTION_DISABLED_FEATURE,
    STRING_LEAK_DETECTION_DISABLED_SAFE_BROWSING,
    STRING_LEAK_DETECTION_FINISHED,
    STRING_LEAK_DETECTION_HASH_ERROR,
    STRING_LEAK_DETECTION_INVALID_SERVER_RESPONSE_ERROR,
    STRING_LEAK_DETECTION_SIGNED_OUT_ERROR,
    STRING_LEAK_DETECTION_TOKEN_REQUEST_ERROR,
    STRING_LEAK_DETECTION_NETWORK_ERROR,
    STRING_LEAK_DETECTION_QUOTA_LIMIT,
    STRING_LEAK_DETECTION_URL_BLOCKED,
    STRING_PASSWORD_REQUIREMENTS_VOTE_FOR_LETTER,
    STRING_PASSWORD_REQUIREMENTS_VOTE_FOR_SPECIAL_SYMBOL,
    STRING_PASSWORD_REQUIREMENTS_VOTE_FOR_SPECIFIC_SPECIAL_SYMBOL,
    STRING_PASSWORD_REQUIREMENTS_VOTE_FOR_PASSWORD_LENGTH,
    STRING_SAVE_PASSWORD_HASH,
    STRING_DID_NAVIGATE_MAIN_FRAME,
    STRING_NAVIGATION_NTP,
    STRING_SERVER_PREDICTIONS,
    STRING_USERNAME_FIRST_FLOW_VOTE,
    STRING_POSSIBLE_USERNAME_USED,
    STRING_POSSIBLE_USERNAME_NOT_USED,
    STRING_SAVING_BLOCKLISTED_EXPLICITLY,
    STRING_SAVING_BLOCKLISTED_BY_SMART_BUBBLE,
    STRING_INVALID,  // Represents a string returned in a case of an error.
    STRING_MAX = STRING_INVALID
  };

  SavePasswordProgressLogger();

  SavePasswordProgressLogger(const SavePasswordProgressLogger&) = delete;
  SavePasswordProgressLogger& operator=(const SavePasswordProgressLogger&) =
      delete;

  virtual ~SavePasswordProgressLogger();

  // Call these methods to log information. They sanitize the input and call
  // SendLog to pass it for display.
  void LogFormData(StringID label, const FormData& form_data);
  void LogHTMLForm(StringID label,
                   const std::string& name_or_id,
                   const GURL& action);
  void LogURL(StringID label, const GURL& url);
  void LogBoolean(StringID label, bool truth_value);
  void LogNumber(StringID label, int signed_number);
  void LogNumber(StringID label, size_t unsigned_number);
  void LogMessage(StringID message);

  // Returns a log string representing `field`.
  static std::string GetFormFieldDataLogString(const FormFieldData& field);

  // Removes privacy sensitive parts of `url` (currently all but host and
  // scheme).
  static std::string ScrubURL(const GURL& url);

  // Replaces all characters satisfying IsUnwantedInElementID with a ' '.
  // This damages some valid HTML element IDs or names, but it is likely that it
  // will be still possible to match the scrubbed string to the original ID or
  // name in the HTML doc. That's good enough for the logging purposes, and
  // provides some security benefits.
  static std::string ScrubElementID(const std::u16string& element_id);

  // The UTF-8 version of the function above.
  static std::string ScrubElementID(std::string element_id);

  // Translates the StringID values into the corresponding strings.
  static std::string GetStringFromID(SavePasswordProgressLogger::StringID id);

 protected:
  // Sends `log` immediately for display.
  virtual void SendLog(const std::string& log) = 0;

  // Converts `log` and its `label` to a string and calls SendLog on the result.
  void LogValue(StringID label, const base::Value& log);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_SAVE_PASSWORD_PROGRESS_LOGGER_H_
