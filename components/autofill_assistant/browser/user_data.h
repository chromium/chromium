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
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/user_action.h"
#include "components/autofill_assistant/browser/website_login_manager.h"

namespace autofill {
class AutofillProfile;
class CreditCard;
}  // namespace autofill

namespace autofill_assistant {
class UserModel;

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

// GENERATED_JAVA_ENUM_PACKAGE: (
// org.chromium.chrome.browser.autofill_assistant.user_data)
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: AssistantContactField
enum AutofillContactField {
  NAME_FULL = 7,
  EMAIL_ADDRESS = 9,
  PHONE_HOME_WHOLE_NUMBER = 14,
};

// Represents a concrete login choice in the UI, e.g., 'Guest checkout' or
// a particular Chrome PWM login account.
struct LoginChoice {
  LoginChoice(const std::string& id,
              const std::string& label,
              const std::string& sublabel,
              const base::Optional<std::string>& sublabel_accessibility_hint,
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
  base::Optional<std::string> sublabel_accessibility_hint;
  // The priority to pre-select this choice (-1 == not set/automatic).
  int preselect_priority = -1;
  // The popup to show to provide more information about this login choice.
  base::Optional<InfoPopupProto> info_popup;
};

// Tuple for holding credit card and billing address;
struct PaymentInstrument {
  PaymentInstrument();
  PaymentInstrument(std::unique_ptr<autofill::CreditCard> card,
                    std::unique_ptr<autofill::AutofillProfile> billing_address);
  ~PaymentInstrument();

  std::unique_ptr<autofill::CreditCard> card;
  std::unique_ptr<autofill::AutofillProfile> billing_address;
};

// Struct for holding the user data.
class UserData {
 public:
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
    AVAILABLE_PAYMENT_INSTRUMENTS,
  };

  std::unique_ptr<autofill::CreditCard> selected_card_;
  std::string login_choice_identifier_;
  TermsAndConditionsState terms_and_conditions_ = NOT_SELECTED;
  base::Optional<DateProto> date_time_range_start_date_;
  base::Optional<DateProto> date_time_range_end_date_;
  base::Optional<int> date_time_range_start_timeslot_;
  base::Optional<int> date_time_range_end_timeslot_;

  // A set of additional key/value pairs to be stored in client_memory.
  std::map<std::string, ValueProto> additional_values_;

  std::vector<std::unique_ptr<autofill::AutofillProfile>> available_profiles_;
  std::vector<std::unique_ptr<PaymentInstrument>>
      available_payment_instruments_;

  // The address key requested by the autofill action.
  std::map<std::string, std::unique_ptr<autofill::AutofillProfile>>
      selected_addresses_;

  base::Optional<WebsiteLoginManager::Login> selected_login_;

  // Return true if address has been selected, otherwise return false.
  // Note that selected_address() might return nullptr when
  // has_selected_address() is true because fill manually was chosen.
  bool has_selected_address(const std::string& name) const;

  // Returns true if an additional value is stored for |key|.
  bool has_additional_value(const std::string& key) const;

  // Selected address for |name|. It will be a nullptr if didn't select anything
  // or if selected 'Fill manually'.
  const autofill::AutofillProfile* selected_address(
      const std::string& name) const;

  // The additional value for |key|, or nullptr if it does not exist.
  const ValueProto* additional_value(const std::string& key) const;

  // The form data of the password change form. This is stored at the time of
  // password generation (GeneratePasswordForFormFieldProto) to allow a
  // subsequent PresaveGeneratedPasswordProto to presave the password prior to
  // submission.
  base::Optional<autofill::FormData> password_form_data_;

  std::string GetAllAddressKeyNames() const;
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
  std::vector<AutofillContactField> contact_summary_fields;
  int contact_summary_max_lines;
  std::vector<AutofillContactField> contact_full_fields;
  int contact_full_max_lines;

  bool require_billing_postal_code = false;
  std::string billing_postal_code_missing_text;
  std::string credit_card_expired_text;

  // If empty, terms and conditions should not be shown.
  std::string accept_terms_and_conditions_text;
  std::string terms_require_review_text;
  std::string info_section_text;
  bool info_section_text_center = false;
  std::string privacy_notice_text;
  bool show_terms_as_checkbox = false;

  std::string billing_address_name;
  std::string shipping_address_name;
  std::string contact_details_name;

  std::vector<std::string> supported_basic_card_networks;
  std::vector<LoginChoice> login_choices;
  std::string default_email;
  std::string contact_details_section_title;
  std::string login_section_title;
  std::string shipping_address_section_title;
  UserActionProto confirm_action;
  std::vector<UserActionProto> additional_actions;
  TermsAndConditionsState initial_terms_and_conditions = NOT_SELECTED;
  DateTimeRangeProto date_time_range;
  std::vector<UserFormSectionProto> additional_prepended_sections;
  std::vector<UserFormSectionProto> additional_appended_sections;
  base::Optional<GenericUserInterfaceProto> generic_user_interface_prepended;
  base::Optional<GenericUserInterfaceProto> generic_user_interface_appended;
  base::Optional<std::string> additional_model_identifier_to_check;

  base::OnceCallback<void(UserData*, const UserModel*)> confirm_callback;
  base::OnceCallback<void(int, UserData*, const UserModel*)>
      additional_actions_callback;
  base::OnceCallback<void(int, UserData*, const UserModel*)>
      terms_link_callback;
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_USER_DATA_H_
