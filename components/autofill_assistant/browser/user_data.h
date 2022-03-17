// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_USER_DATA_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_USER_DATA_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill_assistant/browser/cud_condition.pb.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/user_action.h"
#include "components/autofill_assistant/browser/website_login_manager.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill {
class AutofillProfile;
class CreditCard;
}  // namespace autofill

namespace autofill_assistant {
class UserModel;

// GENERATED_JAVA_ENUM_PACKAGE: (
// org.chromium.components.autofill_assistant.user_data)
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: AssistantTermsAndConditionsState
enum TermsAndConditionsState {
  NOT_SELECTED = 0,
  ACCEPTED = 1,
  REQUIRES_REVIEW = 2,
};

// GENERATED_JAVA_ENUM_PACKAGE: (
// org.chromium.components.autofill_assistant.user_data.additional_sections)
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: AssistantTextInputType
enum TextInputType { INPUT_TEXT = 0, INPUT_ALPHANUMERIC = 1 };

// GENERATED_JAVA_ENUM_PACKAGE: (
// org.chromium.components.autofill_assistant.user_data)
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: AssistantContactField
enum AutofillContactField {
  NAME_FULL = 7,
  EMAIL_ADDRESS = 9,
  PHONE_HOME_WHOLE_NUMBER = 14,
};

// GENERATED_JAVA_ENUM_PACKAGE: (
// org.chromium.components.autofill_assistant.user_data)
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: AssistantUserDataEventType
enum UserDataEventType {
  UNKNOWN,
  NO_NOTIFICATION,
  SELECTION_CHANGED,
  ENTRY_EDITED,
  ENTRY_CREATED
};

enum UserDataEventField { CONTACT_EVENT, CREDIT_CARD_EVENT, SHIPPING_EVENT };

// Represents a concrete login choice in the UI, e.g., 'Guest checkout' or
// a particular Chrome PWM login account.
struct LoginChoice {
  LoginChoice(
      const std::string& id,
      const std::string& label,
      const std::string& sublabel,
      const absl::optional<std::string>& sublabel_accessibility_hint,
      int priority,
      const absl::optional<InfoPopupProto>& info_popup,
      const absl::optional<std::string>& edit_button_content_description);
  LoginChoice();
  LoginChoice(const LoginChoice& another);
  ~LoginChoice();

  // Compares login choices by preselect_priority. Sorts in ascending order.
  static bool CompareByPriority(const LoginChoice& lhs, const LoginChoice& rhs);

  // Uniquely identifies this login choice.
  std::string identifier;
  // The label to display to the user.
  std::string label;
  // The sublabel to display to the user.
  std::string sublabel;
  // The a11y hint for |sublabel|.
  absl::optional<std::string> sublabel_accessibility_hint;
  // The priority to pre-select this choice (-1 == not set/automatic).
  int preselect_priority = -1;
  // The popup to show to provide more information about this login choice.
  absl::optional<InfoPopupProto> info_popup;
  // The a11y hint for the edit button.
  absl::optional<std::string> edit_button_content_description;
};

// Struct for holding payment information, such as credit card and billing
// address. This is a wrapper around Autofill entities to easily bundle and
// extend them for the purposes of Autofill Assistant.
struct PaymentInstrument {
  PaymentInstrument();
  PaymentInstrument(std::unique_ptr<autofill::CreditCard> card,
                    std::unique_ptr<autofill::AutofillProfile> billing_address);
  ~PaymentInstrument();

  absl::optional<std::string> identifier;
  std::unique_ptr<autofill::CreditCard> card;
  std::unique_ptr<autofill::AutofillProfile> billing_address;
  // This field is only filled for payment instruments being sent from our own
  // endpoint. It is absl::nullopt for Chrome Autofill data.
  absl::optional<std::string> edit_token;
};

// Struct for holding a contact. This is a wrapper around AutofillProfile to
// easily extend it for the purposes of Autofill Assistant.
struct Contact {
  Contact();
  Contact(std::unique_ptr<autofill::AutofillProfile> profile);
  ~Contact();

  absl::optional<std::string> identifier;
  std::unique_ptr<autofill::AutofillProfile> profile;
};

// Struct for holding a phone number. This is a wrapper around AutofillProfile
// to easily extend it for the purposes of Autofill Assistant.
struct PhoneNumber {
  PhoneNumber();
  PhoneNumber(std::unique_ptr<autofill::AutofillProfile> profile);
  ~PhoneNumber();

  absl::optional<std::string> identifier;
  std::unique_ptr<autofill::AutofillProfile> profile;
};

// Struct for holding an address. This is a wrapper around AutofillProfile to
// easily extend it for the purposes of Autofill Assistant.
struct Address {
  Address();
  Address(std::unique_ptr<autofill::AutofillProfile> profile);
  ~Address();

  absl::optional<std::string> identifier;
  std::unique_ptr<autofill::AutofillProfile> profile;
  // This field is only filled for addresses being sent from our own endpoint.
  // It is absl::nullopt for Chrome Autofill data.
  absl::optional<std::string> edit_token;
};

// Struct for holding metrics data used by CollectUserDataAction.
struct UserDataMetrics {
  UserDataMetrics();
  ~UserDataMetrics();
  UserDataMetrics(const UserDataMetrics&);
  UserDataMetrics& operator=(const UserDataMetrics&);

  bool metrics_logged = false;
  ukm::SourceId source_id;

  bool initially_prefilled = false;
  bool personal_data_changed = false;
  Metrics::CollectUserDataResult action_result =
      Metrics::CollectUserDataResult::FAILURE;

  Metrics::UserDataSource user_data_source = Metrics::UserDataSource::UNKNOWN;

  // Selection states.
  Metrics::UserDataSelectionState contact_selection_state =
      Metrics::UserDataSelectionState::NO_CHANGE;
  Metrics::UserDataSelectionState credit_card_selection_state =
      Metrics::UserDataSelectionState::NO_CHANGE;
  Metrics::UserDataSelectionState shipping_selection_state =
      Metrics::UserDataSelectionState::NO_CHANGE;

  // Initial counts of complete/incomplete entries.
  int complete_contacts_initial_count = 0;
  int incomplete_contacts_initial_count = 0;
  int complete_credit_cards_initial_count = 0;
  int incomplete_credit_cards_initial_count = 0;
  int complete_shipping_addresses_initial_count = 0;
  int incomplete_shipping_addresses_initial_count = 0;

  // Bitmasks of fields present in the initially selected entries.
  int selected_contact_field_bitmask = 0;
  int selected_shipping_address_field_bitmask = 0;
  int selected_credit_card_field_bitmask = 0;
  int selected_billing_address_field_bitmask = 0;
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
    PHONE_NUMBER,
    CARD,
    SHIPPING_ADDRESS,
    BILLING_ADDRESS,
    LOGIN_CHOICE,
    TERMS_AND_CONDITIONS,
    ADDITIONAL_VALUES,
    AVAILABLE_PROFILES,
    AVAILABLE_PAYMENT_INSTRUMENTS,
  };

  TermsAndConditionsState terms_and_conditions_ = NOT_SELECTED;

  std::vector<std::unique_ptr<Contact>> available_contacts_;
  std::vector<std::unique_ptr<PhoneNumber>> available_phone_numbers_;
  std::vector<std::unique_ptr<Address>> available_addresses_;
  std::vector<std::unique_ptr<PaymentInstrument>>
      available_payment_instruments_;

  absl::optional<WebsiteLoginManager::Login> selected_login_;

  absl::optional<UserDataMetrics> previous_user_data_metrics_;

  // Return true if address has been selected, otherwise return false.
  // Note that selected_address() might return nullptr when
  // has_selected_address() is true because fill manually was chosen.
  bool has_selected_address(const std::string& name) const;

  // Selected address for |name|. It will be a nullptr if didn't select anything
  // or if selected 'Fill manually'.
  const autofill::AutofillProfile* selected_address(
      const std::string& name) const;

  // The selected phone number.
  const autofill::AutofillProfile* selected_phone_number() const;

  // The selected card.
  const autofill::CreditCard* selected_card() const;

  // The selected login choice.
  const LoginChoice* selected_login_choice() const;

  // Set an additional value for |key|.
  void SetAdditionalValue(const std::string& name, const ValueProto& value);

  // Returns true if an additional value is stored for |key|.
  bool HasAdditionalValue(const std::string& key) const;

  // The additional value for |key|, or nullptr if it does not exist.
  const ValueProto* GetAdditionalValue(const std::string& key) const;

  // The form data of the password change form. This is stored at the time of
  // password generation (GeneratePasswordForFormFieldProto) to allow a
  // subsequent PresaveGeneratedPasswordProto to presave the password prior to
  // submission.
  absl::optional<autofill::FormData> password_form_data_;

  std::string GetAllAddressKeyNames() const;

  void SetSelectedPhoneNumber(
      std::unique_ptr<autofill::AutofillProfile> profile);

 private:
  friend class UserModel;
  // The address key requested by the autofill action.
  // Written by |UserModel| to ensure that it stays in sync.
  base::flat_map<std::string, std::unique_ptr<autofill::AutofillProfile>>
      selected_addresses_;

  // The selected credit card.
  // Written by |UserModel| to ensure that it stays in sync.
  std::unique_ptr<autofill::CreditCard> selected_card_;

  // The selected phone number.
  std::unique_ptr<autofill::AutofillProfile> selected_phone_number_;

  // The selected login choice.
  // Written by |UserModel| to ensure that it stays in sync.
  std::unique_ptr<LoginChoice> selected_login_choice_;

  // A set of additional key/value pairs to be stored in client_memory.
  base::flat_map<std::string, ValueProto> additional_values_;
};

// Struct for holding the payment request options.
struct CollectUserDataOptions {
  CollectUserDataOptions();
  ~CollectUserDataOptions();

  // TODO(b/180705720): Eventually remove |request_payer_name|,
  // |request_payer_email| and |request_payer_phone|. They're still used to
  // control the ContactEditor.
  bool request_payer_name = false;
  bool request_payer_email = false;
  bool request_payer_phone = false;
  bool request_phone_number_separately = false;
  bool request_shipping = false;
  bool request_payment_method = false;
  bool request_login_choice = false;
  std::vector<AutofillContactField> contact_summary_fields;
  int contact_summary_max_lines;
  std::vector<AutofillContactField> contact_full_fields;
  int contact_full_max_lines;

  // TODO(b/180705720): Eventually remove |credit_card_expired_text| and
  // place it into |required_credit_card_pieces|.
  std::string credit_card_expired_text;

  std::vector<RequiredDataPiece> required_contact_data_pieces;
  std::vector<RequiredDataPiece> required_phone_number_data_pieces;
  std::vector<RequiredDataPiece> required_shipping_address_data_pieces;
  std::vector<RequiredDataPiece> required_credit_card_data_pieces;
  std::vector<RequiredDataPiece> required_billing_address_data_pieces;

  bool should_store_data_changes = false;
  bool can_edit_contacts = true;
  bool use_gms_core_edit_dialogs = false;

  absl::optional<std::string> add_payment_instrument_action_token;
  absl::optional<std::string> add_address_token;

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
  std::string phone_number_section_title;
  std::string login_section_title;
  std::string shipping_address_section_title;
  UserActionProto confirm_action;
  std::vector<UserActionProto> additional_actions;
  TermsAndConditionsState initial_terms_and_conditions = NOT_SELECTED;
  std::vector<UserFormSectionProto> additional_prepended_sections;
  std::vector<UserFormSectionProto> additional_appended_sections;
  absl::optional<GenericUserInterfaceProto> generic_user_interface_prepended;
  absl::optional<GenericUserInterfaceProto> generic_user_interface_appended;
  absl::optional<std::string> additional_model_identifier_to_check;
  absl::optional<DataOriginNoticeProto> data_origin_notice;

  base::OnceCallback<void(UserData*, const UserModel*)> confirm_callback;
  base::OnceCallback<void(int, UserData*, const UserModel*)>
      additional_actions_callback;
  base::OnceCallback<void(int, UserData*, const UserModel*)>
      terms_link_callback;
  base::OnceCallback<void(UserData*)> reload_data_callback;
  // Called whenever there is a change to the selected user data.
  base::RepeatingCallback<void(UserDataEventField, UserDataEventType)>
      selected_user_data_changed_callback;
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_USER_DATA_H_
