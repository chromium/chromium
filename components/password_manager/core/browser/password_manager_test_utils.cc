// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_test_utils.h"

#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/password_manager/core/browser/hash_password_manager.h"
#include "components/password_manager/core/browser/password_form.h"

namespace password_manager {

std::unique_ptr<PasswordForm> PasswordFormFromData(
    const PasswordFormData& form_data) {
  auto form = std::make_unique<PasswordForm>();
  form->scheme = form_data.scheme;
  form->date_last_used =
      base::Time::FromSecondsSinceUnixEpoch(form_data.last_usage_time);
  form->date_created =
      base::Time::FromSecondsSinceUnixEpoch(form_data.creation_time);
  if (form_data.signon_realm) {
    form->signon_realm = std::string(form_data.signon_realm);
  }
  if (form_data.origin) {
    form->url = GURL(form_data.origin);
  }
  if (form_data.action) {
    form->action = GURL(form_data.action);
  }
  if (form_data.submit_element) {
    form->submit_element = form_data.submit_element;
  }
  if (form_data.username_element) {
    form->username_element = form_data.username_element;
  }
  if (form_data.password_element) {
    form->password_element = form_data.password_element;
  }
  if (form_data.username_value) {
    form->username_value = form_data.username_value;
  }
  if (form_data.password_value) {
    form->password_value = form_data.password_value;
  }
  return form;
}

std::unique_ptr<PasswordForm> FillPasswordFormWithData(
    const PasswordFormData& form_data,
    bool is_account_store,
    bool use_federated_login) {
  auto form = PasswordFormFromData(form_data);
  if (form_data.username_value) {
    form->display_name = form->username_value;
  } else {
    form->blocked_by_user = true;
  }
  form->icon_url = GURL("https://accounts.google.com/Icon");
  if (use_federated_login) {
    form->password_value.clear();
    form->federation_origin =
        url::SchemeHostPort(GURL("https://accounts.google.com/login"));
    if (!affiliations::IsValidAndroidFacetURI(form->signon_realm)) {
      form->signon_realm =
          "federation://" + form->url.host() + "/accounts.google.com";
      form->type = PasswordForm::Type::kApi;
    }
  }
  if (is_account_store) {
    form->in_store = PasswordForm::Store::kAccountStore;
  } else {
    form->in_store = PasswordForm::Store::kProfileStore;
  }
  form->password_issues = base::flat_map<InsecureType, InsecurityMetadata>();
  return form;
}

PasswordForm CreateEntry(const std::string& username,
                         const std::string& password,
                         const GURL& origin_url,
                         PasswordForm::MatchType match_type) {
  PasswordForm form;
  form.username_value = base::ASCIIToUTF16(username);
  form.password_value = base::ASCIIToUTF16(password);
  form.url = origin_url;
  form.signon_realm = origin_url.GetWithEmptyPath().spec();
  form.match_type = match_type;
  return form;
}

std::unique_ptr<PasswordForm> CreateUniquePtrEntry(
    const std::string& username,
    const std::string& password,
    const GURL& origin_url,
    PasswordForm::MatchType match_type) {
  return std::make_unique<PasswordForm>(
      CreateEntry(username, password, origin_url, match_type));
}

bool ContainsEqualPasswordFormsUnordered(
    const std::vector<std::unique_ptr<PasswordForm>>& expectations,
    const std::vector<std::unique_ptr<PasswordForm>>& actual_values,
    std::ostream* mismatch_output) {
  std::vector<::testing::Matcher<std::unique_ptr<PasswordForm>>>
      expected_matchers;
  for (const auto& form : expectations) {
    expected_matchers.push_back(
        testing::Pointee(EqualsIgnorePrimaryKey(*form)));
  }
  auto m = testing::UnorderedElementsAreArray(expected_matchers);
  testing::StringMatchResultListener listener;
  bool matches = testing::ExplainMatchResult(m, actual_values, &listener);
  if (mismatch_output) {
    *mismatch_output << listener.str();
  }
  return matches;
}

std::vector<::testing::Matcher<PasswordForm>> FormsIgnoringPrimaryKey(
    const std::vector<PasswordForm>& forms) {
  std::vector<::testing::Matcher<PasswordForm>> result;
  for (const auto& form : forms) {
    result.push_back(EqualsIgnorePrimaryKey(form));
  }
  return result;
}

MockPasswordStoreObserver::MockPasswordStoreObserver() = default;

MockPasswordStoreObserver::~MockPasswordStoreObserver() = default;

PasswordStoreWaiter::PasswordStoreWaiter(PasswordStoreInterface* store) {
  password_store_observer_.Observe(store);
}

PasswordStoreWaiter::~PasswordStoreWaiter() = default;

void PasswordStoreWaiter::WaitOrReturn() {
  run_loop_.Run();
}

void PasswordStoreWaiter::OnLoginsChanged(
    PasswordStoreInterface* store,
    const PasswordStoreChangeList& changes) {
  run_loop_.Quit();
}

MockPasswordReuseDetectorConsumer::MockPasswordReuseDetectorConsumer() =
    default;

MockPasswordReuseDetectorConsumer::~MockPasswordReuseDetectorConsumer() =
    default;

base::WeakPtr<PasswordReuseDetectorConsumer>
MockPasswordReuseDetectorConsumer::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

PasswordHashDataMatcher::PasswordHashDataMatcher(
    std::optional<PasswordHashData> expected)
    : expected_(expected) {}

PasswordHashDataMatcher::~PasswordHashDataMatcher() = default;

bool PasswordHashDataMatcher::MatchAndExplain(
    std::optional<PasswordHashData> hash_data,
    ::testing::MatchResultListener* listener) const {
  if (expected_ == std::nullopt) {
    return hash_data == std::nullopt;
  }

  if (hash_data == std::nullopt) {
    return false;
  }

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

::testing::Matcher<std::optional<PasswordHashData>> Matches(
    std::optional<PasswordHashData> expected) {
  return ::testing::MakeMatcher(new PasswordHashDataMatcher(expected));
}

}  // namespace password_manager
