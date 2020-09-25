// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_test_utils.h"

#include <algorithm>
#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/hash_password_manager.h"

namespace password_manager {

std::unique_ptr<PasswordForm> PasswordFormFromData(
    const PasswordFormData& form_data) {
  auto form = std::make_unique<PasswordForm>();
  form->scheme = form_data.scheme;
  form->date_last_used = base::Time::FromDoubleT(form_data.last_usage_time);
  form->date_created = base::Time::FromDoubleT(form_data.creation_time);
  if (form_data.signon_realm)
    form->signon_realm = std::string(form_data.signon_realm);
  if (form_data.origin)
    form->url = GURL(form_data.origin);
  if (form_data.action)
    form->action = GURL(form_data.action);
  if (form_data.submit_element)
    form->submit_element = base::WideToUTF16(form_data.submit_element);
  if (form_data.username_element)
    form->username_element = base::WideToUTF16(form_data.username_element);
  if (form_data.password_element)
    form->password_element = base::WideToUTF16(form_data.password_element);
  if (form_data.username_value)
    form->username_value = base::WideToUTF16(form_data.username_value);
  if (form_data.password_value)
    form->password_value = base::WideToUTF16(form_data.password_value);
  return form;
}

std::unique_ptr<PasswordForm> FillPasswordFormWithData(
    const PasswordFormData& form_data,
    bool use_federated_login) {
  auto form = PasswordFormFromData(form_data);
  form->date_synced = form->date_created + base::TimeDelta::FromDays(1);
  if (form_data.username_value)
    form->display_name = form->username_value;
  else
    form->blocked_by_user = true;
  form->icon_url = GURL("https://accounts.google.com/Icon");
  if (use_federated_login) {
    form->password_value.clear();
    form->federation_origin =
        url::Origin::Create(GURL("https://accounts.google.com/login"));
  }
  form->in_store = PasswordForm::Store::kProfileStore;
  return form;
}

std::unique_ptr<PasswordForm> CreateEntry(const std::string& username,
                                          const std::string& password,
                                          const GURL& origin_url,
                                          bool is_psl_match,
                                          bool is_affiliation_based_match) {
  auto form = std::make_unique<PasswordForm>();
  form->username_value = base::ASCIIToUTF16(username);
  form->password_value = base::ASCIIToUTF16(password);
  form->url = origin_url;
  form->is_public_suffix_match = is_psl_match;
  form->is_affiliation_based_match = is_affiliation_based_match;
  return form;
}

bool ContainsEqualPasswordFormsUnordered(
    const std::vector<std::unique_ptr<PasswordForm>>& expectations,
    const std::vector<std::unique_ptr<PasswordForm>>& actual_values,
    std::ostream* mismatch_output) {
  std::vector<PasswordForm*> remaining_expectations(expectations.size());
  std::transform(
      expectations.begin(), expectations.end(), remaining_expectations.begin(),
      [](const std::unique_ptr<PasswordForm>& form) { return form.get(); });

  bool had_mismatched_actual_form = false;
  for (const auto& actual : actual_values) {
    auto it_matching_expectation = std::find_if(
        remaining_expectations.begin(), remaining_expectations.end(),
        [&actual](const PasswordForm* expected) {
          return *expected == *actual;
        });
    if (it_matching_expectation != remaining_expectations.end()) {
      // Erase the matched expectation by moving the last element to its place.
      *it_matching_expectation = remaining_expectations.back();
      remaining_expectations.pop_back();
    } else {
      if (mismatch_output) {
        *mismatch_output << std::endl
                         << "Unmatched actual form:" << std::endl
                         << *actual;
      }
      had_mismatched_actual_form = true;
    }
  }

  if (mismatch_output) {
    for (const PasswordForm* remaining_expected_form : remaining_expectations) {
      *mismatch_output << std::endl
                       << "Unmatched expected form:" << std::endl
                       << *remaining_expected_form;
    }
  }

  return !had_mismatched_actual_form && remaining_expectations.empty();
}

MockPasswordStoreObserver::MockPasswordStoreObserver() = default;

MockPasswordStoreObserver::~MockPasswordStoreObserver() = default;

#if defined(PASSWORD_REUSE_DETECTION_ENABLED)
MockPasswordReuseDetectorConsumer::MockPasswordReuseDetectorConsumer() =
    default;

MockPasswordReuseDetectorConsumer::~MockPasswordReuseDetectorConsumer() =
    default;

PasswordHashDataMatcher::PasswordHashDataMatcher(
    base::Optional<PasswordHashData> expected)
    : expected_(expected) {}

bool PasswordHashDataMatcher::MatchAndExplain(
    base::Optional<PasswordHashData> hash_data,
    ::testing::MatchResultListener* listener) const {
  if (expected_ == base::nullopt)
    return hash_data == base::nullopt;

  if (hash_data == base::nullopt)
    return false;

  return expected_->username == hash_data->username &&
         expected_->length == hash_data->length &&
         expected_->is_gaia_password == hash_data->is_gaia_password;
}

void PasswordHashDataMatcher::DescribeTo(::std::ostream* os) const {
  *os << "matches password hash data for " << expected_->username;
}

void PasswordHashDataMatcher::DescribeNegationTo(::std::ostream* os) const {
  *os << "doesn't match password hash data for " << expected_->username;
}

::testing::Matcher<base::Optional<PasswordHashData>> Matches(
    base::Optional<PasswordHashData> expected) {
  return ::testing::MakeMatcher(new PasswordHashDataMatcher(expected));
}

#endif

}  // namespace password_manager
