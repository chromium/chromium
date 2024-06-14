// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/save_password_progress_logger.h"

#include <algorithm>
#include <string>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/common/signatures.h"

namespace autofill {

namespace {

// Returns true for all characters which we don't want to see in the logged IDs
// or names of HTML elements.
bool IsUnwantedInElementID(char c) {
  return !(c == '_' || c == '-' || base::IsAsciiAlpha(c) ||
           base::IsAsciiDigit(c));
}

}  // namespace

SavePasswordProgressLogger::SavePasswordProgressLogger() = default;
SavePasswordProgressLogger::~SavePasswordProgressLogger() = default;

std::string FormSignatureToDebugString(FormSignature form_signature) {
  return base::StrCat(
      {base::NumberToString(form_signature.value()), " - ",
       base::NumberToString(HashFormSignature(form_signature))});
}

void SavePasswordProgressLogger::LogFormData(
    SavePasswordProgressLogger::StringID label,
    const FormData& form_data) {
  CHECK(!form_data.url().is_empty());
  std::string message = GetStringFromID(label) + ": {\n";
  message += GetStringFromID(STRING_FORM_SIGNATURE) + ": " +
             FormSignatureToDebugString(CalculateFormSignature(form_data)) +
             "\n";
  message +=
      GetStringFromID(STRING_ALTERNATIVE_FORM_SIGNATURE) + ": " +
      FormSignatureToDebugString(CalculateAlternativeFormSignature(form_data)) +
      "\n";
  message +=
      GetStringFromID(STRING_ORIGIN) + ": " + ScrubURL(form_data.url()) + "\n";
  message += GetStringFromID(STRING_ACTION) + ": " +
             ScrubURL(form_data.action()) + "\n";
  if (form_data.main_frame_origin().GetURL().is_valid()) {
    message += GetStringFromID(STRING_MAIN_FRAME_ORIGIN) + ": " +
               ScrubURL(form_data.main_frame_origin().GetURL()) + "\n";
  }
  message += GetStringFromID(STRING_FORM_NAME) + ": " +
             ScrubElementID(form_data.name()) + "\n";

  if (!form_data.renderer_id().is_null()) {
    message += "Form renderer id: " +
               base::NumberToString(form_data.renderer_id().value()) + "\n";
  }

  // Log fields.
  message += GetStringFromID(STRING_FIELDS) + ": " + "\n";
  for (const auto& field : form_data.fields()) {
    message += GetFormFieldDataLogString(field) + "\n";
  }
  message += "}";
  SendLog(message);
}

void SavePasswordProgressLogger::LogHTMLForm(
    SavePasswordProgressLogger::StringID label,
    const std::string& name_or_id,
    const GURL& action) {
  base::Value::Dict log;
  log.Set(GetStringFromID(STRING_NAME_OR_ID), ScrubElementID(name_or_id));
  log.Set(GetStringFromID(STRING_ACTION), ScrubURL(action));
  LogValue(label, base::Value(std::move(log)));
}

void SavePasswordProgressLogger::LogURL(
    SavePasswordProgressLogger::StringID label,
    const GURL& url) {
  LogValue(label, base::Value(ScrubURL(url)));
}

void SavePasswordProgressLogger::LogBoolean(
    SavePasswordProgressLogger::StringID label,
    bool truth_value) {
  LogValue(label, base::Value(truth_value));
}

void SavePasswordProgressLogger::LogNumber(
    SavePasswordProgressLogger::StringID label,
    int signed_number) {
  LogValue(label, base::Value(signed_number));
}

void SavePasswordProgressLogger::LogNumber(
    SavePasswordProgressLogger::StringID label,
    size_t unsigned_number) {
  LogNumber(label, base::checked_cast<int>(unsigned_number));
}

void SavePasswordProgressLogger::LogMessage(
    SavePasswordProgressLogger::StringID message) {
  LogValue(STRING_MESSAGE, base::Value(GetStringFromID(message)));
}

// static
std::string SavePasswordProgressLogger::GetFormFieldDataLogString(
    const FormFieldData& field) {
  const char* const is_visible = field.is_focusable() ? "visible" : "invisible";
  const char* const is_empty = field.value().empty() ? "empty" : "non-empty";
  std::string autocomplete =
      field.autocomplete_attribute().empty()
          ? std::string()
          : (", autocomplete=" +
             ScrubElementID(field.autocomplete_attribute()));
  return base::StringPrintf(
      "%s: signature=%s, type=%s, renderer_id=%s, %s, %s%s",
      ScrubElementID(field.name()).c_str(),
      base::NumberToString(*CalculateFieldSignatureForField(field)).c_str(),
      ScrubElementID(std::string(autofill::FormControlTypeToString(
                         field.form_control_type())))
          .c_str(),
      base::NumberToString(*field.renderer_id()).c_str(), is_visible, is_empty,
      autocomplete.c_str());
}

// static
std::string SavePasswordProgressLogger::ScrubURL(const GURL& url) {
  if (url.is_valid())
    return url.GetWithEmptyPath().spec();
  return std::string();
}

void SavePasswordProgressLogger::LogValue(StringID label,
                                          const base::Value& log) {
  std::string log_string;
  bool conversion_to_string_successful = base::JSONWriter::WriteWithOptions(
      log, base::JSONWriter::OPTIONS_PRETTY_PRINT, &log_string);
  DCHECK(conversion_to_string_successful);
  std::replace(log_string.begin(), log_string.end(), '"', ' ');
  SendLog(GetStringFromID(label) + ": " + log_string);
}

// static
std::string SavePasswordProgressLogger::ScrubElementID(
    const std::u16string& element_id) {
  return ScrubElementID(base::UTF16ToUTF8(element_id));
}

// static
std::string SavePasswordProgressLogger::ScrubElementID(std::string element_id) {
  std::replace_if(element_id.begin(), element_id.end(), IsUnwantedInElementID,
                  '_');
  return element_id;
}

// Note 1: Caching the ID->string map in an array would be probably faster, but
// the switch statement is (a) robust against re-ordering, and (b) checks in
// compile-time, that all IDs get a string assigned. The expected frequency of
// calls is low enough (in particular, zero if password manager internals page
// is not open), that optimizing for code robustness is preferred against speed.
// Note 2: Do not use '.' in the message strings -- the ending of the log items
// should be controlled by the logger. Also, some of the messages below can be
// used as dictionary keys.
//
// static
std::string SavePasswordProgressLogger::GetStringFromID(
    SavePasswordProgressLogger::StringID id) {
  switch (id) {
    case SavePasswordProgressLogger::STRING_DECISION_ASK:
      return "Decision: ASK the user";
    case SavePasswordProgressLogger::STRING_DECISION_DROP:
      return "Decision: DROP the password";
    case SavePasswordProgressLogger::STRING_DECISION_SAVE:
      return "Decision: SAVE the password";
    case SavePasswordProgressLogger::STRING_OTHER:
      return "(other)";
    case SavePasswordProgressLogger::STRING_SCHEME_HTML:
      return "HTML";
    case SavePasswordProgressLogger::STRING_SCHEME_BASIC:
      return "Basic";
    case SavePasswordProgressLogger::STRING_SCHEME_DIGEST:
      return "Digest";
    case SavePasswordProgressLogger::STRING_SCHEME_USERNAME_ONLY:
      return "Username only";
    case SavePasswordProgressLogger::STRING_SCHEME_MESSAGE:
      return "Scheme";
    case SavePasswordProgressLogger::STRING_SIGNON_REALM:
      return "Signon realm";
    case SavePasswordProgressLogger::STRING_ORIGIN:
      return "Origin";
    case SavePasswordProgressLogger::STRING_ACTION:
      return "Action";
    case SavePasswordProgressLogger::STRING_USERNAME_ELEMENT:
      return "Username element";
    case SavePasswordProgressLogger::STRING_USERNAME_ELEMENT_RENDERER_ID:
      return "Username element renderer id";
    case SavePasswordProgressLogger::STRING_PASSWORD_ELEMENT:
      return "Password element";
    case SavePasswordProgressLogger::STRING_PASSWORD_ELEMENT_RENDERER_ID:
      return "Password element renderer id";
    case SavePasswordProgressLogger::STRING_NEW_PASSWORD_ELEMENT:
      return "New password element";
    case SavePasswordProgressLogger::STRING_NEW_PASSWORD_ELEMENT_RENDERER_ID:
      return "New password element renderer id";
    case SavePasswordProgressLogger::STRING_CONFIRMATION_PASSWORD_ELEMENT:
      return "Confirmation password element";
    case SavePasswordProgressLogger::
        STRING_CONFIRMATION_PASSWORD_ELEMENT_RENDERER_ID:
      return "Confirmation password element renderer id";
    case SavePasswordProgressLogger::STRING_PASSWORD_GENERATED:
      return "Password generated";
    case SavePasswordProgressLogger::STRING_TIMES_USED:
      return "Times used";
    case SavePasswordProgressLogger::STRING_NAME_OR_ID:
      return "Form name or ID";
    case SavePasswordProgressLogger::STRING_MESSAGE:
      return "Message";
    case SavePasswordProgressLogger::STRING_SET_AUTH_METHOD:
      return "LoginHandler::SetAuth";
    case SavePasswordProgressLogger::STRING_AUTHENTICATION_HANDLED:
      return "Authentication already handled";
    case SavePasswordProgressLogger::STRING_LOGINHANDLER_FORM:
      return "LoginHandler reports this form";
    case SavePasswordProgressLogger::STRING_SEND_PASSWORD_FORMS_METHOD:
      return "PasswordAutofillAgent::SendPasswordForms";
    case SavePasswordProgressLogger::STRING_SECURITY_ORIGIN:
      return "Security origin";
    case SavePasswordProgressLogger::STRING_SECURITY_ORIGIN_FAILURE:
      return "Security origin cannot access password manager";
    case SavePasswordProgressLogger::STRING_WEBPAGE_EMPTY:
      return "Webpage is empty";
    case SavePasswordProgressLogger::STRING_NUMBER_OF_ALL_FORMS:
      return "Number of all forms";
    case SavePasswordProgressLogger::STRING_FORM_FOUND_ON_PAGE:
      return "Form found on page";
    case SavePasswordProgressLogger::STRING_FORM_IS_VISIBLE:
      return "Form is visible";
    case SavePasswordProgressLogger::STRING_FORM_IS_PASSWORD:
      return "Form is a password form";
    case SavePasswordProgressLogger::STRING_HTML_FORM_FOR_SUBMIT:
      return "HTML form for submit";
    case SavePasswordProgressLogger::STRING_DID_START_PROVISIONAL_LOAD_METHOD:
      return "PasswordAutofillAgent::DidStartProvisionalLoad";
    case SavePasswordProgressLogger::STRING_FRAME_NOT_MAIN_FRAME:
      return "|frame| is not the main frame";
    case SavePasswordProgressLogger::STRING_PROVISIONALLY_SAVE_FORM_METHOD:
      return "PasswordManager::ProvisionallySaveForm";
    case SavePasswordProgressLogger::STRING_EMPTY_PASSWORD:
      return "Empty password";
    case SavePasswordProgressLogger::STRING_MATCHING_NOT_COMPLETE:
      return "No form manager has completed matching";
    case SavePasswordProgressLogger::STRING_INVALID_FORM:
      return "Invalid form";
    case SavePasswordProgressLogger::STRING_SYNC_CREDENTIAL:
      return "Credential is used for syncing passwords";
    case STRING_BLOCK_PASSWORD_SAME_ORIGIN_INSECURE_SCHEME:
      return "Blocked password due to same origin but insecure scheme";
    case SavePasswordProgressLogger::STRING_ON_PASSWORD_FORMS_RENDERED_METHOD:
      return "PasswordManager::OnPasswordFormsRendered";
    case SavePasswordProgressLogger::STRING_ON_DYNAMIC_FORM_SUBMISSION:
      return "PasswordManager::OnDynamicFormSubmission";
    case SavePasswordProgressLogger::STRING_ON_PASSWORD_FORM_CLEARED:
      return "PasswordManager::OnPasswordFormCleared";
    case SavePasswordProgressLogger::STRING_ON_SUBFRAME_FORM_SUBMISSION:
      return "PasswordManager::OnSubframeFormSubmission";
    case SavePasswordProgressLogger::STRING_ON_ASK_USER_OR_SAVE_PASSWORD:
      return "PasswordManager::AskUserOrSavePassword";
    case SavePasswordProgressLogger::STRING_CAN_PROVISIONAL_MANAGER_SAVE_METHOD:
      return "PasswordManager::IsAutomaticSavePromptAvailable";
    case SavePasswordProgressLogger::STRING_NO_PROVISIONAL_SAVE_MANAGER:
      return "No provisional save manager";
    case SavePasswordProgressLogger::STRING_ANOTHER_MANAGER_WAS_SUBMITTED:
      return "Another form manager was submitted";
    case SavePasswordProgressLogger::STRING_NUMBER_OF_VISIBLE_FORMS:
      return "Number of visible forms";
    case SavePasswordProgressLogger::STRING_PASSWORD_FORM_REAPPEARED:
      return "Password form re-appeared";
    case SavePasswordProgressLogger::STRING_SAVING_DISABLED:
      return "Saving disabled";
    case SavePasswordProgressLogger::STRING_NO_MATCHING_FORM:
      return "No matching form";
    case SavePasswordProgressLogger::STRING_SSL_ERRORS_PRESENT:
      return "SSL errors present";
    case SavePasswordProgressLogger::STRING_ONLY_VISIBLE:
      return "only_visible";
    case SavePasswordProgressLogger::STRING_SHOW_PASSWORD_PROMPT:
      return "Show password prompt";
    case SavePasswordProgressLogger::STRING_PASSWORDMANAGER_AUTOFILL:
      return "PasswordManager::Autofill";
    case SavePasswordProgressLogger::STRING_WAIT_FOR_USERNAME:
      return "wait_for_username";
    case SavePasswordProgressLogger::
        STRING_WAS_LAST_NAVIGATION_HTTP_ERROR_METHOD:
      return "ChromePasswordManagerClient::WasLastNavigationHTTPError";
    case SavePasswordProgressLogger::STRING_HTTP_STATUS_CODE:
      return "HTTP status code for landing page";
    case SavePasswordProgressLogger::
        STRING_PROVISIONALLY_SAVED_FORM_IS_NOT_HTML:
      return "Provisionally saved form is not HTML";
    case SavePasswordProgressLogger::STRING_ON_GET_STORE_RESULTS_METHOD:
      return "FormFetcherImpl::OnGetPasswordStoreResults";
    case SavePasswordProgressLogger::STRING_NUMBER_RESULTS:
      return "Number of results from the password store";
    case SavePasswordProgressLogger::STRING_FETCH_METHOD:
      return "FormFetcherImpl::Fetch";
    case SavePasswordProgressLogger::STRING_NO_STORE:
      return "PasswordStore is not available";
    case SavePasswordProgressLogger::STRING_CREATE_LOGIN_MANAGERS_METHOD:
      return "PasswordManager::CreatePendingLoginManagers";
    case SavePasswordProgressLogger::
        STRING_PASSWORD_MANAGEMENT_ENABLED_FOR_CURRENT_PAGE:
      return "IsPasswordManagementEnabledForCurrentPage";
    case SavePasswordProgressLogger::STRING_SHOW_LOGIN_PROMPT_METHOD:
      return "ShowLoginPrompt";
    case SavePasswordProgressLogger::STRING_NEW_UI_STATE:
      return "The new state of the UI";
    case SavePasswordProgressLogger::STRING_FORM_SIGNATURE:
      return "Signature of form";
    case SavePasswordProgressLogger::STRING_ALTERNATIVE_FORM_SIGNATURE:
      return "Alternative signature of form";
    case SavePasswordProgressLogger::STRING_FORM_FETCHER_STATE:
      return "FormFetcherImpl::state_";
    case SavePasswordProgressLogger::STRING_UNOWNED_INPUTS_VISIBLE:
      return "Some control elements not associated to a form element are "
             "visible";
    case SavePasswordProgressLogger::STRING_ON_FILL_PASSWORD_FORM_METHOD:
      return "PasswordAutofillAgent::OnFillPasswordForm";
    case SavePasswordProgressLogger::STRING_FORM_DATA_WAIT:
      return "form_data's wait_for_username";
    case SavePasswordProgressLogger::STRING_FILL_USERNAME_AND_PASSWORD_METHOD:
      return "FillUserNameAndPassword in PasswordAutofillAgent";
    case SavePasswordProgressLogger::STRING_USERNAMES_MATCH:
      return "Username to fill matches that on the page";
    case SavePasswordProgressLogger::STRING_MATCH_IN_ADDITIONAL:
      return "Match found in additional logins";
    case SavePasswordProgressLogger::STRING_USERNAME_FILLED:
      return "Filled username element";
    case SavePasswordProgressLogger::STRING_PASSWORD_FILLED:
      return "Filled password element";
    case SavePasswordProgressLogger::STRING_FORM_NAME:
      return "Form name";
    case SavePasswordProgressLogger::STRING_FIELDS:
      return "Form fields";
    case SavePasswordProgressLogger::STRING_FIRSTUSE_FORM_VOTE:
      return "FirstUse vote";
    case SavePasswordProgressLogger::STRING_PASSWORD_FORM_VOTE:
      return "PasswordForm vote";
    case SavePasswordProgressLogger::STRING_REUSE_FOUND:
      return "Password reused from ";
    case SavePasswordProgressLogger::STRING_GENERATION_DISABLED_SAVING_DISABLED:
      return "Generation disabled: saving disabled";
    case SavePasswordProgressLogger::
        STRING_GENERATION_DISABLED_CHROME_DOES_NOT_SYNC_PASSWORDS:
      return "Generation disabled: Chrome no longer syncs passwords and GMS is "
             "no up to date to do it either";
    case SavePasswordProgressLogger::
        STRING_GENERATION_DISABLED_NOT_ABLE_TO_SAVE_PASSWORDS:
      return "Generation disabled: not able to save passwords";
    case SavePasswordProgressLogger::STRING_GENERATION_DISABLED_NO_SYNC:
      return "Generation disabled: no sync";
    case STRING_GENERATION_RENDERER_AUTOMATIC_GENERATION_AVAILABLE:
      return "Generation: automatic generation is available";
    case STRING_GENERATION_RENDERER_SHOW_GENERATION_POPUP:
      return "Show generation popup triggered";
    case STRING_GENERATION_RENDERER_GENERATED_PASSWORD_ACCEPTED:
      return "Generated password accepted";
    case STRING_SUCCESSFUL_SUBMISSION_INDICATOR_EVENT:
      return "Successful submission indicator event";
    case STRING_MAIN_FRAME_ORIGIN:
      return "Main frame origin";
    case STRING_IS_FORM_TAG:
      return "Form with form tag";
    case STRING_FORM_PARSING_INPUT:
      return "Form parsing input";
    case STRING_FORM_PARSING_OUTPUT:
      return "Form parsing output";
    case STRING_FAILED_TO_FILL_INTO_IFRAME:
      return "Failed to fill: Form is in iframe on a non-PSL-matching security "
             "origin";
    case STRING_FAILED_TO_FILL_NO_AUTOCOMPLETEABLE_ELEMENT:
      return "Failed to fill: No autocompleteable element found";
    case STRING_FAILED_TO_FILL_PREFILLED_USERNAME:
      return "Failed to fill: Username field was prefilled, but no credential "
             "exists whose username matches the prefilled value";
    case STRING_FAILED_TO_FILL_FOUND_NO_PASSWORD_FOR_USERNAME:
      return "Failed to fill: No credential matching found";
    case SavePasswordProgressLogger::
        STRING_HTTPAUTH_ON_ASK_USER_OR_SAVE_PASSWORD:
      return "HttpAuthManager::AskUserOrSavePassword";
    case SavePasswordProgressLogger::STRING_HTTPAUTH_ON_PROMPT_USER:
      return "HttpAuthManager::PromptUser";
    case SavePasswordProgressLogger::STRING_HTTPAUTH_ON_SET_OBSERVER:
      return "HttpAuthManager::SetObserver";
    case SavePasswordProgressLogger::STRING_HTTPAUTH_ON_DETACH_OBSERVER:
      return "HttpAuthManager::DetachObserver";
    case STRING_LEAK_DETECTION_DISABLED_FEATURE:
      return "Leak detection disabled in settings";
    case STRING_LEAK_DETECTION_DISABLED_SAFE_BROWSING:
      return "Leak detection is off as the safe browsing is disabled";
    case STRING_LEAK_DETECTION_FINISHED:
      return "Leak detection finished with result";
    case STRING_LEAK_DETECTION_HASH_ERROR:
      return "Leak detection failed: hashing/encryption error";
    case STRING_LEAK_DETECTION_INVALID_SERVER_RESPONSE_ERROR:
      return "Leak detection failed: invalid server response";
    case STRING_LEAK_DETECTION_SIGNED_OUT_ERROR:
      return "Leak detection failed: signed out";
    case STRING_LEAK_DETECTION_TOKEN_REQUEST_ERROR:
      return "Leak detection failed: can't get a token";
    case STRING_LEAK_DETECTION_NETWORK_ERROR:
      return "Leak detection failed: network error";
    case STRING_LEAK_DETECTION_QUOTA_LIMIT:
      return "Leak detection failed: quota limit";
    case STRING_LEAK_DETECTION_URL_BLOCKED:
      return "Leak detection disabled by SafeBrowsingAllowlistDomains policy";
    case SavePasswordProgressLogger::
        STRING_PASSWORD_REQUIREMENTS_VOTE_FOR_LETTER:
      return "Uploading password requirements vote for using letters";
    case SavePasswordProgressLogger::
        STRING_PASSWORD_REQUIREMENTS_VOTE_FOR_SPECIAL_SYMBOL:
      return "Uploading password requirements vote for using special symbols";
    case SavePasswordProgressLogger::
        STRING_PASSWORD_REQUIREMENTS_VOTE_FOR_SPECIFIC_SPECIAL_SYMBOL:
      return "Used specific special symbol";
    case SavePasswordProgressLogger::
        STRING_PASSWORD_REQUIREMENTS_VOTE_FOR_PASSWORD_LENGTH:
      return "Uploading password requirements vote for password length";
    case STRING_SAVE_PASSWORD_HASH:
      return "Password hash is saved";
    case STRING_DID_NAVIGATE_MAIN_FRAME:
      return "PasswordManager::DidNavigateMainFrame";
    case STRING_NAVIGATION_NTP:
      return "Navigation to New Tab page";
    case STRING_SERVER_PREDICTIONS:
      return "Server predictions";
    case STRING_USERNAME_FIRST_FLOW_VOTE:
      return "Username first flow vote";
    case STRING_POSSIBLE_USERNAME_USED:
      return "Possible username is used";
    case STRING_POSSIBLE_USERNAME_NOT_USED:
      return "Possible username is not used";
    case STRING_SAVING_BLOCKLISTED_EXPLICITLY:
      return "Saving on this domain is explicitly blocklisted";
    case STRING_SAVING_BLOCKLISTED_BY_SMART_BUBBLE:
      return "Saving on this domain is blocklisted by the smart bubble";
    case SavePasswordProgressLogger::STRING_INVALID:
      return "INVALID";
      // Intentionally no default: clause here -- all IDs need to get covered.
  }
  NOTREACHED_IN_MIGRATION();  // Win compilers don't believe this is
                              // unreachable.
  return std::string();
}

}  // namespace autofill
