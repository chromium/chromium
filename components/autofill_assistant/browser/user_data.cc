// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/user_data.h"

#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"

namespace autofill_assistant {

LoginChoice::LoginChoice(
    const std::string& _identifier,
    const std::string& _label,
    const std::string& _sublabel,
    const absl::optional<std::string>& _sublabel_accessibility_hint,
    int _preselect_priority,
    const absl::optional<InfoPopupProto>& _info_popup,
    const absl::optional<std::string>& _edit_button_content_description)
    : identifier(_identifier),
      label(_label),
      sublabel(_sublabel),
      sublabel_accessibility_hint(_sublabel_accessibility_hint),
      preselect_priority(_preselect_priority),
      info_popup(_info_popup),
      edit_button_content_description(_edit_button_content_description) {}
LoginChoice::LoginChoice() = default;
LoginChoice::LoginChoice(const LoginChoice& another) = default;
LoginChoice::~LoginChoice() = default;

bool LoginChoice::CompareByPriority(const LoginChoice& lhs,
                                    const LoginChoice& rhs) {
  return lhs.preselect_priority < rhs.preselect_priority;
}

PaymentInstrument::PaymentInstrument() = default;
PaymentInstrument::PaymentInstrument(
    std::unique_ptr<autofill::CreditCard> _card,
    std::unique_ptr<autofill::AutofillProfile> _billing_address)
    : card(std::move(_card)), billing_address(std::move(_billing_address)) {}
PaymentInstrument::~PaymentInstrument() = default;

Contact::Contact() = default;
Contact::Contact(std::unique_ptr<autofill::AutofillProfile> _profile)
    : profile(std::move(_profile)) {}
Contact::~Contact() = default;

PhoneNumber::PhoneNumber() = default;
PhoneNumber::PhoneNumber(std::unique_ptr<autofill::AutofillProfile> _profile)
    : profile(std::move(_profile)) {}
PhoneNumber::~PhoneNumber() = default;

Address::Address() = default;
Address::Address(std::unique_ptr<autofill::AutofillProfile> _profile)
    : profile(std::move(_profile)) {}
Address::~Address() = default;

UserDataMetrics::UserDataMetrics() = default;
UserDataMetrics::~UserDataMetrics() = default;
UserDataMetrics::UserDataMetrics(const UserDataMetrics&) = default;
UserDataMetrics& UserDataMetrics::operator=(const UserDataMetrics&) = default;

UserData::UserData() = default;
UserData::~UserData() = default;

CollectUserDataOptions::CollectUserDataOptions() = default;
CollectUserDataOptions::~CollectUserDataOptions() = default;

bool UserData::has_selected_address(const std::string& name) const {
  return selected_address(name) != nullptr;
}

const autofill::AutofillProfile* UserData::selected_address(
    const std::string& name) const {
  auto it = selected_addresses_.find(name);
  if (it == selected_addresses_.end()) {
    return nullptr;
  }

  return it->second.get();
}

const autofill::AutofillProfile* UserData::selected_phone_number() const {
  return selected_phone_number_.get();
}

const autofill::CreditCard* UserData::selected_card() const {
  return selected_card_.get();
}

const LoginChoice* UserData::selected_login_choice() const {
  return selected_login_choice_.get();
}

void UserData::SetAdditionalValue(const std::string& key,
                                  const ValueProto& value) {
  additional_values_[key] = value;
}

bool UserData::HasAdditionalValue(const std::string& key) const {
  return additional_values_.find(key) != additional_values_.end();
}

const ValueProto* UserData::GetAdditionalValue(const std::string& key) const {
  auto it = additional_values_.find(key);
  if (it == additional_values_.end()) {
    return nullptr;
  }
  return &it->second;
}

std::string UserData::GetAllAddressKeyNames() const {
  std::vector<std::string> entries;
  for (const auto& entry : selected_addresses_) {
    entries.emplace_back(entry.first);
  }
  std::sort(entries.begin(), entries.end());
  return base::JoinString(entries, ",");
}

void UserData::SetSelectedPhoneNumber(
    std::unique_ptr<autofill::AutofillProfile> profile) {
  selected_phone_number_ = std::move(profile);
}

}  // namespace autofill_assistant
