// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/user_model.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "third_party/re2/src/re2/re2.h"

namespace autofill_assistant {

namespace {

// Matches an ASCII identifier (w+) which ends in [<string>], e.g.,
// test_identifier[sub_identifier[2]].
static const char* const kExtractArraySubidentifierRegex = R"(^(\w+)\[(.+)\]$)";

// Simple wrapper around value_util::GetNthValue to unwrap the base::optional
// |value|.
base::Optional<ValueProto> GetNthValue(const base::Optional<ValueProto>& value,
                                       int index) {
  if (!value.has_value()) {
    return base::nullopt;
  }

  return GetNthValue(*value, index);
}

// Same as above, but expects |index_value| to point to a single integer value
// specifying the index to retrieve.
base::Optional<ValueProto> GetNthValue(
    const base::Optional<ValueProto>& value,
    const base::Optional<ValueProto>& index_value) {
  if (!value.has_value() || !index_value.has_value()) {
    return base::nullopt;
  }
  if (!AreAllValuesOfSize({*index_value}, 1) ||
      !AreAllValuesOfType({*index_value}, ValueProto::kInts)) {
    return base::nullopt;
  }

  return GetNthValue(*value, index_value->ints().values().at(0));
}

}  // namespace

UserModel::Observer::Observer() = default;
UserModel::Observer::~Observer() = default;

UserModel::UserModel() = default;
UserModel::~UserModel() = default;

base::WeakPtr<UserModel> UserModel::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void UserModel::SetValue(const std::string& identifier,
                         const ValueProto& value,
                         bool force_notification) {
  auto result = values_.emplace(identifier, value);
  if (!force_notification && !result.second && result.first->second == value &&
      value.is_client_side_only() ==
          result.first->second.is_client_side_only()) {
    return;
  } else if (!result.second) {
    result.first->second = value;
  }

  for (auto& observer : observers_) {
    observer.OnValueChanged(identifier, value);
  }
}

base::Optional<ValueProto> UserModel::GetValue(
    const std::string& identifier) const {
  auto it = values_.find(identifier);
  if (it != values_.end()) {
    return it->second;
  } else if (base::EndsWith(identifier, "]", base::CompareCase::SENSITIVE)) {
    std::string identifier_without_suffix;
    std::string subidentifier;
    if (re2::RE2::FullMatch(identifier, kExtractArraySubidentifierRegex,
                            &identifier_without_suffix, &subidentifier)) {
      int index;
      if (base::StringToInt(subidentifier, &index)) {
        // The case 'identifier[n]'.
        return GetNthValue(GetValue(identifier_without_suffix), index);
      } else {
        // The case 'identifier[subidentifier]'.
        return GetNthValue(GetValue(identifier_without_suffix),
                           GetValue(subidentifier));
      }
    }
  }
  return base::nullopt;
}

base::Optional<ValueProto> UserModel::GetValue(
    const ValueReferenceProto& reference) const {
  switch (reference.kind_case()) {
    case ValueReferenceProto::kValue:
      return reference.value();
    case ValueReferenceProto::kModelIdentifier:
      return GetValue(reference.model_identifier());
    case ValueReferenceProto::KIND_NOT_SET:
      return base::nullopt;
  }
}

void UserModel::MergeWithProto(const ModelProto& another,
                               bool force_notifications) {
  for (const auto& another_value : another.values()) {
    if (another_value.value() == ValueProto()) {
      // std::map::emplace does not overwrite existing values.
      if (values_.emplace(another_value.identifier(), another_value.value())
              .second ||
          force_notifications) {
        for (auto& observer : observers_) {
          observer.OnValueChanged(another_value.identifier(),
                                  another_value.value());
        }
      }
      continue;
    }
    SetValue(another_value.identifier(), another_value.value(),
             force_notifications);
  }
}

void UserModel::UpdateProto(ModelProto* model_proto) const {
  for (auto& model_value : *model_proto->mutable_values()) {
    auto it = values_.find(model_value.identifier());
    if (it != values_.end()) {
      *model_value.mutable_value() = (it->second);
    }
  }
}

void UserModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void UserModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void UserModel::SetAutofillCreditCards(
    std::unique_ptr<std::vector<std::unique_ptr<autofill::CreditCard>>>
        credit_cards) {
  credit_cards_.clear();
  for (auto& credit_card : *credit_cards) {
    credit_cards_[credit_card->guid()] = std::move(credit_card);
  }
}

void UserModel::SetAutofillProfiles(
    std::unique_ptr<std::vector<std::unique_ptr<autofill::AutofillProfile>>>
        profiles) {
  profiles_.clear();
  for (auto& profile : *profiles) {
    profiles_[profile->guid()] = std::move(profile);
  }
}

void UserModel::SetCurrentURL(GURL current_url) {
  current_url_ = current_url;
}

const autofill::CreditCard* UserModel::GetCreditCard(
    const std::string& guid) const {
  auto it = credit_cards_.find(guid);
  if (it == credit_cards_.end()) {
    return nullptr;
  }
  return it->second.get();
}

const autofill::AutofillProfile* UserModel::GetProfile(
    const std::string& guid) const {
  auto it = profiles_.find(guid);
  if (it == profiles_.end()) {
    return nullptr;
  }
  return it->second.get();
}

GURL UserModel::GetCurrentURL() const {
  return current_url_;
}

}  // namespace autofill_assistant
