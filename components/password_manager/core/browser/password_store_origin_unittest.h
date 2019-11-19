// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_ORIGIN_UNITTEST_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_ORIGIN_UNITTEST_H_

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using autofill::PasswordForm;
using password_manager::PasswordStore;
using testing::_;
using testing::ElementsAre;

bool matchesOrigin(const url::Origin& origin, const GURL& url) {
  return origin.IsSameOriginWith(url::Origin::Create(url));
}

namespace password_manager {

PasswordFormData CreateTestPasswordFormDataByOrigin(const char* origin_url) {
  PasswordFormData data = {PasswordForm::Scheme::kHtml,
                           origin_url,
                           origin_url,
                           "login_element",
                           L"submit_element",
                           L"username_element",
                           L"password_element",
                           L"username_value",
                           L"password_value",
                           true,
                           1};
  return data;
}

// Collection of origin-related testcases common to all platform-specific
// stores.
// The template type T represents a test delegate that must implement the
// following methods:
//    // Returns a pointer to a fully initialized store for polymorphic usage.
//    PasswordStore* store();
//
//    // Finishes all asnychronous processing on the store.
//    void FinishAsyncProcessing();
template <typename T>
class PasswordStoreOriginTest : public testing::Test {
 protected:
  T delegate_;
};

TYPED_TEST_SUITE_P(PasswordStoreOriginTest);

TYPED_TEST_P(PasswordStoreOriginTest,
             RemoveLoginsByURLAndTimeImpl_AllFittingOriginAndTime) {
  const char origin_url[] = "http://foo.example.com/";
  std::unique_ptr<PasswordForm> form =
      FillPasswordFormWithData(CreateTestPasswordFormDataByOrigin(origin_url));
  this->delegate_.store()->AddLogin(*form);
  this->delegate_.FinishAsyncProcessing();

  MockPasswordStoreObserver observer;
  this->delegate_.store()->AddObserver(&observer);

  const url::Origin origin = url::Origin::Create((GURL(origin_url)));
  base::Callback<bool(const GURL&)> filter = base::Bind(&matchesOrigin, origin);
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnLoginsChanged(ElementsAre(PasswordStoreChange(
                            PasswordStoreChange::REMOVE, *form))));
  this->delegate_.store()->RemoveLoginsByURLAndTime(
      filter, base::Time(), base::Time::Max(), run_loop.QuitClosure());
  run_loop.Run();

  this->delegate_.store()->RemoveObserver(&observer);
}

TYPED_TEST_P(PasswordStoreOriginTest,
             RemoveLoginsByURLAndTimeImpl_SomeFittingOriginAndTime) {
  const char fitting_url[] = "http://foo.example.com/";
  std::unique_ptr<PasswordForm> form =
      FillPasswordFormWithData(CreateTestPasswordFormDataByOrigin(fitting_url));
  this->delegate_.store()->AddLogin(*form);

  const char nonfitting_url[] = "http://bar.example.com/";
  this->delegate_.store()->AddLogin(*FillPasswordFormWithData(
      CreateTestPasswordFormDataByOrigin(nonfitting_url)));

  this->delegate_.FinishAsyncProcessing();

  MockPasswordStoreObserver observer;
  this->delegate_.store()->AddObserver(&observer);

  const url::Origin fitting_origin = url::Origin::Create((GURL(fitting_url)));
  base::Callback<bool(const GURL&)> filter =
      base::Bind(&matchesOrigin, fitting_origin);
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnLoginsChanged(ElementsAre(PasswordStoreChange(
                            PasswordStoreChange::REMOVE, *form))));
  this->delegate_.store()->RemoveLoginsByURLAndTime(
      filter, base::Time(), base::Time::Max(), run_loop.QuitClosure());
  run_loop.Run();

  this->delegate_.store()->RemoveObserver(&observer);
}

TYPED_TEST_P(PasswordStoreOriginTest,
             RemoveLoginsByURLAndTimeImpl_NonMatchingOrigin) {
  const char origin_url[] = "http://foo.example.com/";
  std::unique_ptr<autofill::PasswordForm> form =
      FillPasswordFormWithData(CreateTestPasswordFormDataByOrigin(origin_url));
  this->delegate_.store()->AddLogin(*form);
  this->delegate_.FinishAsyncProcessing();

  MockPasswordStoreObserver observer;
  this->delegate_.store()->AddObserver(&observer);

  const url::Origin other_origin =
      url::Origin::Create(GURL("http://bar.example.com/"));
  base::Callback<bool(const GURL&)> filter =
      base::Bind(&matchesOrigin, other_origin);
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnLoginsChanged(_)).Times(0);
  this->delegate_.store()->RemoveLoginsByURLAndTime(
      filter, base::Time(), base::Time::Max(), run_loop.QuitClosure());
  run_loop.Run();

  this->delegate_.store()->RemoveObserver(&observer);
}

TYPED_TEST_P(PasswordStoreOriginTest,
             RemoveLoginsByURLAndTimeImpl_NotWithinTimeInterval) {
  const char origin_url[] = "http://foo.example.com/";
  std::unique_ptr<autofill::PasswordForm> form =
      FillPasswordFormWithData(CreateTestPasswordFormDataByOrigin(origin_url));
  this->delegate_.store()->AddLogin(*form);
  this->delegate_.FinishAsyncProcessing();

  MockPasswordStoreObserver observer;
  this->delegate_.store()->AddObserver(&observer);

  const url::Origin origin = url::Origin::Create((GURL(origin_url)));
  base::Callback<bool(const GURL&)> filter = base::Bind(&matchesOrigin, origin);
  base::Time time_after_creation_date =
      form->date_created + base::TimeDelta::FromDays(1);
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnLoginsChanged(_)).Times(0);
  this->delegate_.store()->RemoveLoginsByURLAndTime(
      filter, time_after_creation_date, base::Time::Max(),
      run_loop.QuitClosure());
  run_loop.Run();

  this->delegate_.store()->RemoveObserver(&observer);
}

REGISTER_TYPED_TEST_SUITE_P(
    PasswordStoreOriginTest,
    RemoveLoginsByURLAndTimeImpl_AllFittingOriginAndTime,
    RemoveLoginsByURLAndTimeImpl_SomeFittingOriginAndTime,
    RemoveLoginsByURLAndTimeImpl_NonMatchingOrigin,
    RemoveLoginsByURLAndTimeImpl_NotWithinTimeInterval);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_ORIGIN_UNITTEST_H_
