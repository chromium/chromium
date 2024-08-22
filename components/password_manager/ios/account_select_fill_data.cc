// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/ios/account_select_fill_data.h"

#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/core/browser/features/password_features.h"

using autofill::FieldRendererId;
using autofill::FormRendererId;

namespace password_manager {

namespace {

// Returns true if credentials are eligible. For example, credentials are
// ineglible when there are only credentials with an empty username available
// for a single username form.
bool AreCredentialsEligibleForFilling(
    const FormInfo* form_info,
    const std::vector<Credential>& credentials) {
  // Check that this is only called when `form_info` is available.
  CHECK(form_info);

  const bool is_single_username = form_info && form_info->username_element_id &&
                                  !form_info->password_element_id;

  const auto has_empty_username = [](const Credential& c) {
    return c.username.empty();
  };
  return !(is_single_username &&
           base::ranges::all_of(credentials, has_empty_username));
}

}  // namespace

FillData::FillData() = default;
FillData::~FillData() = default;
FillData::FillData(const FillData& other) = default;

FormInfo::FormInfo() = default;
FormInfo::~FormInfo() = default;
FormInfo::FormInfo(const FormInfo&) = default;

Credential::Credential(const std::u16string& username,
                       const std::u16string& password,
                       const std::string& realm)
    : username(username), password(password), realm(realm) {}
Credential::~Credential() = default;

AccountSelectFillData::AccountSelectFillData() = default;
AccountSelectFillData::~AccountSelectFillData() = default;

void AccountSelectFillData::Add(const autofill::PasswordFormFillData& form_data,
                                bool always_populate_realm) {
  auto iter_ok = forms_.insert(
      std::make_pair(form_data.form_renderer_id.value(), FormInfo()));
  FormInfo& form_info = iter_ok.first->second;
  form_info.origin = form_data.url;
  form_info.form_id = form_data.form_renderer_id;
  form_info.username_element_id = form_data.username_element_renderer_id;
  form_info.password_element_id = form_data.password_element_renderer_id;

  // Suggested credentials don't depend on a clicked form. It's better to use
  // the latest known credentials, since credentials can be updated between
  // loading of different forms.
  credentials_.clear();

  credentials_.push_back(
      {form_data.preferred_login.username_value,
       form_data.preferred_login.password_value,
       always_populate_realm && form_data.preferred_login.realm.empty()
           ? form_data.url.spec()
           : form_data.preferred_login.realm});

  for (const auto& username_password_and_realm : form_data.additional_logins) {
    const std::u16string& username = username_password_and_realm.username_value;
    const std::u16string& password = username_password_and_realm.password_value;
    const std::string& realm = username_password_and_realm.realm;
    if (always_populate_realm && realm.empty()) {
      credentials_.push_back({username, password, form_data.url.spec()});
    } else {
      credentials_.push_back({username, password, realm});
    }
  }
}

void AccountSelectFillData::Reset() {
  forms_.clear();
  credentials_.clear();
  last_requested_form_ = nullptr;
}

void AccountSelectFillData::ResetCache() {
  credentials_.clear();
}

bool AccountSelectFillData::Empty() const {
  return credentials_.empty();
}

bool AccountSelectFillData::IsSuggestionsAvailable(
    FormRendererId form_identifier,
    FieldRendererId field_identifier,
    bool is_password_field) const {
  const FormInfo* form_info =
      GetFormInfo(form_identifier, field_identifier, is_password_field);
  return form_info && AreCredentialsEligibleForFilling(form_info, credentials_);
}

std::vector<UsernameAndRealm> AccountSelectFillData::RetrieveSuggestions(
    FormRendererId form_identifier,
    FieldRendererId field_identifier,
    bool is_password_field) {
  last_requested_form_ =
      GetFormInfo(form_identifier, field_identifier, is_password_field);
  CHECK(last_requested_form_);

  if (!AreCredentialsEligibleForFilling(last_requested_form_, credentials_)) {
    return {};
  }

  last_requested_password_field_id_ =
      is_password_field ? field_identifier : FieldRendererId();
  std::vector<UsernameAndRealm> result;
  for (const Credential& credential : credentials_)
    result.push_back({credential.username, credential.realm});

  return result;
}

std::unique_ptr<FillData> AccountSelectFillData::GetFillData(
    const std::u16string& username) const {
  if (!last_requested_form_) {
    DUMP_WILL_BE_NOTREACHED();
    return nullptr;
  }

  auto it = base::ranges::find(credentials_, username, &Credential::username);
  if (it == credentials_.end())
    return nullptr;
  const Credential& credential = *it;
  auto result = std::make_unique<FillData>();
  result->origin = last_requested_form_->origin;
  result->form_id = last_requested_form_->form_id;
  result->username_element_id = last_requested_form_->username_element_id;
  result->username_value = credential.username;
  result->password_element_id = last_requested_password_field_id_.is_null()
                                    ? last_requested_form_->password_element_id
                                    : last_requested_password_field_id_;
  result->password_value = credential.password;
  return result;
}

const FormInfo* AccountSelectFillData::GetFormInfo(
    FormRendererId form_identifier,
    FieldRendererId field_identifier,
    bool is_password_field) const {
  auto it = forms_.find(form_identifier);
  if (it == forms_.end())
    return nullptr;
  return is_password_field || it->second.username_element_id == field_identifier
             ? &it->second
             : nullptr;
}

}  // namespace  password_manager
