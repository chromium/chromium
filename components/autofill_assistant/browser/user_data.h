// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_USER_DATA_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_USER_DATA_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/optional.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/user_action.h"

namespace autofill {
class AutofillProfile;
class CreditCard;
}  // namespace autofill

namespace autofill_assistant {

// GENERATED_JAVA_ENUM_PACKAGE: (
// org.chromium.chrome.browser.autofill_assistant.user_data)
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: AssistantTermsAndConditionsState
enum TermsAndConditionsState {
  NOT_SELECTED = 0,
  ACCEPTED = 1,
  REQUIRES_REVIEW = 2,
};

// GENERATED_JAVA_ENUM_PACKAGE: (
// org.chromium.chrome.browser.autofill_assistant.user_data.additional_sections)
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: AssistantTextInputType
enum TextInputType { INPUT_TEXT = 0, INPUT_ALPHANUMERIC = 1 };

// Represents a concrete login choice in the UI, e.g., 'Guest checkout' or
// a particular Chrome PWM login account.
struct LoginChoice {
  LoginChoice(const std::string& id,
              const std::string& label,
              const std::string& sublabel,
              const std::string& sublabel_accessibility_hint,
              int priority,
              const base::Optional<InfoPopupProto>& info_popup);
  LoginChoice(const LoginChoice& another);
  ~LoginChoice();

  // Uniquely identifies this login choice.
  std::string identifier;
  // The label to display to the user.
  std::string label;
  // The sublabel to display to the user.
  std::string sublabel;
  // The a11y hint for |sublabel|.
  std::string sublabel_accessibility_hint;
  // The priority to pre-select this choice (-1 == not set/automatic).
  int preselect_priority = -1;
  // The popup to show to provide more information about this login choice.
  base::Optional<InfoPopupProto> info_popup;
};

// Struct for holding the user data.
struct UserData {
  UserData();
  ~UserData();

  enum class FieldChange {
    NONE,
    ALL,
    CONTACT_PROFILE,
    CARD,
    SHIPPING_ADDRESS,
    BILLING_ADDRESS,
    LOGIN_CHOICE,
    TERMS_AND_CONDITIONS,
    DATE_TIME_RANGE_START,
    DATE_TIME_RANGE_END,
    ADDITIONAL_VALUES,
    AVAILABLE_PROFILES,
  };

  bool succeed = false;
  std::unique_ptr<autofill::AutofillProfile> contact_profile;
  std::unique_ptr<autofill::CreditCard> card;
  std::unique_ptr<autofill::AutofillProfile> shipping_address;
  std::unique_ptr<autofill::AutofillProfile> billing_address;
  std::string login_choice_identifier;
  TermsAndConditionsState terms_and_conditions = NOT_SELECTED;
  DateTimeProto date_time_range_start;
  DateTimeProto date_time_range_end;

  // A set of additional key/value pairs to be stored in client_memory.
  std::map<std::string, std::string> additional_values_to_store;

  std::vector<std::unique_ptr<autofill::AutofillProfile>> available_profiles;
};

// Struct for holding the payment request options.
struct CollectUserDataOptions {
  CollectUserDataOptions();
  ~CollectUserDataOptions();

  bool request_payer_name = false;
  bool request_payer_email = false;
  bool request_payer_phone = false;
  bool request_shipping = false;
  bool request_payment_method = false;
  bool request_login_choice = false;
  bool request_date_time_range = false;

  bool require_billing_postal_code = false;
  std::string billing_postal_code_missing_text;

  // If empty, terms and conditions should not be shown.
  std::string accept_terms_and_conditions_text;
  std::string terms_require_review_text;
  std::string thirdparty_privacy_notice_text;
  bool show_terms_as_checkbox = false;

  std::vector<std::string> supported_basic_card_networks;
  std::vector<LoginChoice> login_choices;
  std::string default_email;
  std::string login_section_title;
  UserActionProto confirm_action;
  std::vector<UserActionProto> additional_actions;
  TermsAndConditionsState initial_terms_and_conditions = NOT_SELECTED;
  DateTimeRangeProto date_time_range;
  std::vector<UserFormSectionProto> additional_prepended_sections;
  std::vector<UserFormSectionProto> additional_appended_sections;

  base::OnceCallback<void(std::unique_ptr<UserData>)> confirm_callback;
  base::OnceCallback<void(int)> additional_actions_callback;
  base::OnceCallback<void(int)> terms_link_callback;
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_USER_DATA_H_
