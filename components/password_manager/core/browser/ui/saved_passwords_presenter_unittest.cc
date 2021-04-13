// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"

#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

struct MockSavedPasswordsPresenterObserver : SavedPasswordsPresenter::Observer {
  MOCK_METHOD(void, OnEdited, (const PasswordForm&), (override));
  MOCK_METHOD(void,
              OnSavedPasswordsChanged,
              (SavedPasswordsPresenter::SavedPasswordsView),
              (override));
};

using StrictMockSavedPasswordsPresenterObserver =
    ::testing::StrictMock<MockSavedPasswordsPresenterObserver>;

class SavedPasswordsPresenterTest : public ::testing::Test {
 protected:
  SavedPasswordsPresenterTest() { store_->Init(/*prefs=*/nullptr); }

  ~SavedPasswordsPresenterTest() override {
    store_->ShutdownOnUIThread();
    task_env_.RunUntilIdle();
  }

  TestPasswordStore& store() { return *store_; }
  SavedPasswordsPresenter& presenter() { return presenter_; }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }

 private:
  base::test::SingleThreadTaskEnvironment task_env_;
  scoped_refptr<TestPasswordStore> store_ =
      base::MakeRefCounted<TestPasswordStore>();
  SavedPasswordsPresenter presenter_{store_};
};

}  // namespace

// Tests whether adding and removing an observer works as expected.
TEST_F(SavedPasswordsPresenterTest, NotifyObservers) {
  PasswordForm form;

  StrictMockSavedPasswordsPresenterObserver observer;
  presenter().AddObserver(&observer);

  // Adding a credential should notify observers. Furthermore, the credential
  // should be present of the list that is passed along.
  EXPECT_CALL(observer, OnSavedPasswordsChanged(
                            ElementsAre(MatchesFormExceptStore(form))));
  store().AddLogin(form);
  RunUntilIdle();
  EXPECT_FALSE(store().IsEmpty());

  // Remove should notify, and observers should be passed an empty list.
  EXPECT_CALL(observer, OnSavedPasswordsChanged(IsEmpty()));
  store().RemoveLogin(form);
  RunUntilIdle();
  EXPECT_TRUE(store().IsEmpty());

  // After an observer is removed it should no longer receive notifications.
  presenter().RemoveObserver(&observer);
  EXPECT_CALL(observer, OnSavedPasswordsChanged).Times(0);
  store().AddLogin(form);
  RunUntilIdle();
  EXPECT_FALSE(store().IsEmpty());
}

// Tests whether adding federated credentials doesn't inform the observers.
TEST_F(SavedPasswordsPresenterTest, IgnoredCredentials) {
  PasswordForm federated_form;
  federated_form.federation_origin =
      url::Origin::Create(GURL("https://example.com"));

  StrictMockSavedPasswordsPresenterObserver observer;
  presenter().AddObserver(&observer);

  // Adding a credential should notify observers. However, since federated
  // credentials should be ignored it should not be passed a long.
  EXPECT_CALL(observer, OnSavedPasswordsChanged(IsEmpty()));
  store().AddLogin(federated_form);
  RunUntilIdle();

  PasswordForm blocked_form;
  blocked_form.blocked_by_user = true;
  EXPECT_CALL(observer, OnSavedPasswordsChanged(IsEmpty()));
  store().AddLogin(blocked_form);
  RunUntilIdle();

  presenter().RemoveObserver(&observer);
}

// Tests whether editing a password works and results in the right
// notifications.
TEST_F(SavedPasswordsPresenterTest, EditPassword) {
  PasswordForm form;

  StrictMockSavedPasswordsPresenterObserver observer;
  presenter().AddObserver(&observer);

  EXPECT_CALL(observer, OnSavedPasswordsChanged);
  store().AddLogin(form);
  RunUntilIdle();
  EXPECT_FALSE(store().IsEmpty());

  // When |form| is read back from the store, its |in_store| member will be set,
  // and SavedPasswordsPresenter::EditPassword() actually depends on that. So
  // set it here too.
  form.in_store = PasswordForm::Store::kProfileStore;

  const std::u16string new_password = u"new_password";
  PasswordForm updated = form;
  updated.password_value = new_password;

  // Verify that editing a password triggers the right notifications.
  EXPECT_CALL(observer, OnEdited(updated));
  EXPECT_CALL(observer, OnSavedPasswordsChanged(ElementsAre(updated)));
  EXPECT_TRUE(presenter().EditPassword(form, new_password));
  RunUntilIdle();
  EXPECT_THAT(store().stored_passwords(),
              ElementsAre(Pair(updated.signon_realm, ElementsAre(updated))));

  // Verify that editing a password that does not exist does not triggers
  // notifications.
  form.username_value = u"another_username";
  EXPECT_CALL(observer, OnEdited).Times(0);
  EXPECT_CALL(observer, OnSavedPasswordsChanged).Times(0);
  EXPECT_FALSE(presenter().EditPassword(form, new_password));
  RunUntilIdle();

  presenter().RemoveObserver(&observer);
}

TEST_F(SavedPasswordsPresenterTest, EditOnlyUsername) {
  PasswordForm form;
  form.signon_realm = "https://example.com";
  form.username_value = u"test@gmail.com";
  form.password_value = u"password";
  form.in_store = PasswordForm::Store::kProfileStore;

  StrictMockSavedPasswordsPresenterObserver observer;
  presenter().AddObserver(&observer);

  EXPECT_CALL(observer, OnSavedPasswordsChanged);
  store().AddLogin(form);
  RunUntilIdle();
  EXPECT_FALSE(store().IsEmpty());

  std::vector<PasswordForm> forms = {form};

  const std::u16string new_username = u"new_username";
  PasswordForm updated_username = form;
  updated_username.username_value = new_username;

  // Verify that editing a username triggers the right notifications.
  base::HistogramTester histogram_tester;

  EXPECT_CALL(observer, OnEdited(updated_username));
  EXPECT_CALL(observer, OnSavedPasswordsChanged(ElementsAre(updated_username)));
  EXPECT_TRUE(
      presenter().EditSavedPasswords(forms, new_username, form.password_value));
  RunUntilIdle();
  EXPECT_THAT(
      store().stored_passwords(),
      ElementsAre(Pair(form.signon_realm, ElementsAre(updated_username))));

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordEditUpdatedValues",
      metrics_util::PasswordEditUpdatedValues::kUsername, 1);

  presenter().RemoveObserver(&observer);
}

TEST_F(SavedPasswordsPresenterTest, EditOnlyPassword) {
  PasswordForm form;
  form.signon_realm = "https://example.com";
  form.username_value = u"test@gmail.com";
  form.password_value = u"password";
  form.in_store = PasswordForm::Store::kProfileStore;

  StrictMockSavedPasswordsPresenterObserver observer;
  presenter().AddObserver(&observer);

  EXPECT_CALL(observer, OnSavedPasswordsChanged);
  store().AddLogin(form);
  RunUntilIdle();
  EXPECT_FALSE(store().IsEmpty());

  std::vector<PasswordForm> forms = {form};

  const std::u16string new_password = u"new_password";
  PasswordForm updated_password = form;
  updated_password.password_value = new_password;

  base::HistogramTester histogram_tester;
  // Verify that editing a password triggers the right notifications.
  EXPECT_CALL(observer, OnEdited(updated_password));
  EXPECT_CALL(observer, OnSavedPasswordsChanged(ElementsAre(updated_password)));
  EXPECT_TRUE(
      presenter().EditSavedPasswords(forms, form.username_value, new_password));
  RunUntilIdle();
  EXPECT_THAT(
      store().stored_passwords(),
      ElementsAre(Pair(form.signon_realm, ElementsAre(updated_password))));
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordEditUpdatedValues",
      metrics_util::PasswordEditUpdatedValues::kPassword, 1);

  presenter().RemoveObserver(&observer);
}

TEST_F(SavedPasswordsPresenterTest, EditUsernameAndPassword) {
  PasswordForm form;
  form.signon_realm = "https://example.com";
  form.username_value = u"test@gmail.com";
  form.password_value = u"password";
  form.in_store = PasswordForm::Store::kProfileStore;

  StrictMockSavedPasswordsPresenterObserver observer;
  presenter().AddObserver(&observer);

  EXPECT_CALL(observer, OnSavedPasswordsChanged);
  store().AddLogin(form);
  RunUntilIdle();
  EXPECT_FALSE(store().IsEmpty());

  std::vector<PasswordForm> forms = {form};

  const std::u16string new_username = u"new_username";
  const std::u16string new_password = u"new_password";

  PasswordForm updated_both = form;
  updated_both.username_value = new_username;
  updated_both.password_value = new_password;

  base::HistogramTester histogram_tester;
  // Verify that editing username and password triggers the right notifications.
  EXPECT_CALL(observer, OnEdited(updated_both));
  EXPECT_CALL(observer, OnSavedPasswordsChanged(ElementsAre(updated_both)));
  EXPECT_TRUE(
      presenter().EditSavedPasswords(forms, new_username, new_password));
  RunUntilIdle();
  EXPECT_THAT(store().stored_passwords(),
              ElementsAre(Pair(form.signon_realm, ElementsAre(updated_both))));
  histogram_tester.ExpectBucketCount(
      "PasswordManager.PasswordEditUpdatedValues",
      metrics_util::PasswordEditUpdatedValues::kBoth, 1);

  presenter().RemoveObserver(&observer);
}

TEST_F(SavedPasswordsPresenterTest, EditPasswordFails) {
  PasswordForm form1;
  form1.signon_realm = "https://example.com";
  form1.username_value = u"test1@gmail.com";
  form1.password_value = u"password";
  form1.in_store = PasswordForm::Store::kProfileStore;

  PasswordForm form2;
  form2.signon_realm = "https://example.com";
  form2.username_value = u"test2@gmail.com";
  form2.password_value = u"password";
  form2.in_store = PasswordForm::Store::kProfileStore;

  store().AddLogin(form1);
  store().AddLogin(form2);
  RunUntilIdle();
  EXPECT_FALSE(store().IsEmpty());

  std::vector<PasswordForm> forms{form1};

  // Updating the form with the username which is already used for same website
  // fails.
  const std::u16string new_username = u"test2@gmail.com";
  EXPECT_FALSE(presenter().EditSavedPasswords(forms, new_username,
                                              form1.password_value));
  RunUntilIdle();
  EXPECT_THAT(store().stored_passwords(),
              ElementsAre(Pair(form1.signon_realm, ElementsAre(form1, form2))));

  // Updating the form with the empty password fails.
  EXPECT_FALSE(presenter().EditSavedPasswords(forms, form1.username_value,
                                              std::u16string()));
  RunUntilIdle();
  EXPECT_THAT(store().stored_passwords(),
              ElementsAre(Pair(form1.signon_realm, ElementsAre(form1, form2))));
}

TEST_F(SavedPasswordsPresenterTest, EditPasswordWithoutChanges) {
  PasswordForm form;
  form.signon_realm = "https://example.com";
  form.username_value = u"test1@gmail.com";
  form.password_value = u"password";
  form.in_store = PasswordForm::Store::kProfileStore;

  store().AddLogin(form);

  RunUntilIdle();
  StrictMockSavedPasswordsPresenterObserver observer;
  presenter().AddObserver(&observer);

  EXPECT_FALSE(store().IsEmpty());
  // Verify that editing a form without changing the username or password does
  // not triggers notifications.
  base::HistogramTester histogram_tester;
  EXPECT_CALL(observer, OnEdited).Times(0);
  EXPECT_CALL(observer, OnSavedPasswordsChanged).Times(0);
  std::vector<PasswordForm> forms = {form};
  EXPECT_TRUE(presenter().EditSavedPasswords(forms, form.username_value,
                                             form.password_value));
  RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      "PasswordManager.PasswordEditUpdatedValues",
      metrics_util::PasswordEditUpdatedValues::kNone, 1);

  presenter().RemoveObserver(&observer);
}

TEST_F(SavedPasswordsPresenterTest, EditUpdatesDuplicates) {
  PasswordForm form;
  form.signon_realm = "https://example.com";
  form.username_value = u"test1@gmail.com";
  form.password_value = u"password";
  form.in_store = PasswordForm::Store::kProfileStore;

  PasswordForm duplicate_form(form);
  duplicate_form.signon_realm = "https://m.example.com";

  store().AddLogin(form);
  store().AddLogin(duplicate_form);

  RunUntilIdle();
  ASSERT_FALSE(store().IsEmpty());

  StrictMockSavedPasswordsPresenterObserver observer;
  presenter().AddObserver(&observer);

  const std::u16string new_password = u"new_password";

  PasswordForm updated_form = form;
  updated_form.password_value = new_password;

  PasswordForm updated_duplicate_form = duplicate_form;
  updated_duplicate_form.password_value = new_password;

  EXPECT_CALL(observer, OnEdited(updated_form));
  EXPECT_CALL(observer, OnEdited(updated_duplicate_form));
  // The notification that the logins have changed arrives after both updates
  // are sent to the store and db. This means that there will be 2 requests
  // from the presenter to get the updated credentials, BUT they are both sent
  // after the writes.
  EXPECT_CALL(observer, OnSavedPasswordsChanged(
                            ElementsAre(updated_form, updated_duplicate_form)))
      .Times(2);
  EXPECT_TRUE(
      presenter().EditSavedPasswords(form, form.username_value, new_password));
  RunUntilIdle();
  EXPECT_THAT(store().stored_passwords(),
              ElementsAre(Pair(form.signon_realm, ElementsAre(updated_form)),
                          Pair(duplicate_form.signon_realm,
                               ElementsAre(updated_duplicate_form))));
  presenter().RemoveObserver(&observer);
}

namespace {

class SavedPasswordsPresenterWithTwoStoresTest : public ::testing::Test {
 protected:
  SavedPasswordsPresenterWithTwoStoresTest() {
    profile_store_->Init(/*prefs=*/nullptr);
    account_store_->Init(/*prefs=*/nullptr);
  }

  ~SavedPasswordsPresenterWithTwoStoresTest() override {
    account_store_->ShutdownOnUIThread();
    profile_store_->ShutdownOnUIThread();
    task_env_.RunUntilIdle();
  }

  TestPasswordStore& profile_store() { return *profile_store_; }
  TestPasswordStore& account_store() { return *account_store_; }
  SavedPasswordsPresenter& presenter() { return presenter_; }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }

 private:
  base::test::SingleThreadTaskEnvironment task_env_;
  scoped_refptr<TestPasswordStore> profile_store_ =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  scoped_refptr<TestPasswordStore> account_store_ =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(true));
  SavedPasswordsPresenter presenter_{profile_store_, account_store_};
};

}  // namespace

// Tests whether adding credentials to profile or account store notifies
// observers with credentials in both stores.
TEST_F(SavedPasswordsPresenterWithTwoStoresTest, AddCredentialsToBothStores) {
  PasswordForm profile_store_form;
  profile_store_form.username_value = u"profile@gmail.com";
  profile_store_form.password_value = u"profile_pass";
  profile_store_form.in_store = PasswordForm::Store::kProfileStore;

  PasswordForm account_store_form1;
  account_store_form1.username_value = u"account@gmail.com";
  account_store_form1.password_value = u"account_pass";
  account_store_form1.in_store = PasswordForm::Store::kAccountStore;

  PasswordForm account_store_form2 = account_store_form1;
  account_store_form2.username_value = u"account2@gmail.com";

  StrictMockSavedPasswordsPresenterObserver observer;
  presenter().AddObserver(&observer);

  EXPECT_CALL(observer, OnSavedPasswordsChanged(
                            UnorderedElementsAre(profile_store_form)));
  profile_store().AddLogin(profile_store_form);
  RunUntilIdle();

  EXPECT_CALL(observer, OnSavedPasswordsChanged(UnorderedElementsAre(
                            profile_store_form, account_store_form1)));
  account_store().AddLogin(account_store_form1);
  RunUntilIdle();

  EXPECT_CALL(observer, OnSavedPasswordsChanged(UnorderedElementsAre(
                            profile_store_form, account_store_form1,
                            account_store_form2)));
  account_store().AddLogin(account_store_form2);
  RunUntilIdle();

  EXPECT_CALL(observer, OnSavedPasswordsChanged(UnorderedElementsAre(
                            account_store_form1, account_store_form2)));
  profile_store().RemoveLogin(profile_store_form);
  RunUntilIdle();

  EXPECT_CALL(observer, OnSavedPasswordsChanged(UnorderedElementsAre(
                            profile_store_form, account_store_form1,
                            account_store_form2)));
  profile_store().AddLogin(profile_store_form);
  RunUntilIdle();

  presenter().RemoveObserver(&observer);
}

// This tests changing the username of a credentials stored in the profile store
// to be equal to a username of a credential stored in the account store for the
// same domain.
TEST_F(SavedPasswordsPresenterWithTwoStoresTest, EditUsername) {
  PasswordForm profile_store_form;
  profile_store_form.username_value = u"profile@gmail.com";
  profile_store_form.password_value = u"profile_pass";
  profile_store_form.in_store = PasswordForm::Store::kProfileStore;

  PasswordForm account_store_form;
  account_store_form.username_value = u"account@gmail.com";
  account_store_form.password_value = u"account_pass";
  account_store_form.in_store = PasswordForm::Store::kAccountStore;

  profile_store().AddLogin(profile_store_form);
  account_store().AddLogin(account_store_form);
  RunUntilIdle();

  EXPECT_THAT(profile_store().stored_passwords(),
              ElementsAre(Pair(profile_store_form.signon_realm,
                               ElementsAre(profile_store_form))));

  auto new_username = account_store_form.username_value;
  std::vector<PasswordForm> forms_to_edit{profile_store_form};
  EXPECT_TRUE(presenter().EditSavedPasswords(
      forms_to_edit, new_username, profile_store_form.password_value));
  RunUntilIdle();
  profile_store_form.username_value = new_username;
  EXPECT_THAT(profile_store().stored_passwords(),
              ElementsAre(Pair(profile_store_form.signon_realm,
                               ElementsAre(profile_store_form))));
}

// Tests that duplicates of credentials are removed only from the store that
// the initial credential belonged to.
TEST_F(SavedPasswordsPresenterWithTwoStoresTest, DeleteCredentialProfileStore) {
  PasswordForm profile_store_form;
  profile_store_form.signon_realm = "https://example.com";
  profile_store_form.username_value = u"example@gmail.com";
  profile_store_form.password_value = u"password";
  profile_store_form.in_store = PasswordForm::Store::kProfileStore;

  PasswordForm duplicate_profile_store_form = profile_store_form;
  duplicate_profile_store_form.signon_realm = "https://m.example.com";

  PasswordForm account_store_form;
  account_store_form.signon_realm = "https://example.com";
  account_store_form.username_value = u"example@gmail.com";
  account_store_form.password_value = u"password";
  account_store_form.in_store = PasswordForm::Store::kAccountStore;

  profile_store().AddLogin(profile_store_form);
  profile_store().AddLogin(duplicate_profile_store_form);
  account_store().AddLogin(account_store_form);
  RunUntilIdle();

  ASSERT_THAT(profile_store().stored_passwords(),
              ElementsAre(Pair(profile_store_form.signon_realm,
                               ElementsAre(profile_store_form)),
                          Pair(duplicate_profile_store_form.signon_realm,
                               ElementsAre(duplicate_profile_store_form))));
  ASSERT_THAT(account_store().stored_passwords(),
              ElementsAre(Pair(account_store_form.signon_realm,
                               ElementsAre(account_store_form))));

  presenter().RemovePassword(profile_store_form);
  RunUntilIdle();

  EXPECT_TRUE(profile_store().IsEmpty());
  EXPECT_THAT(account_store().stored_passwords(),
              ElementsAre(Pair(account_store_form.signon_realm,
                               ElementsAre(account_store_form))));
}

TEST_F(SavedPasswordsPresenterWithTwoStoresTest, DeleteCredentialAccountStore) {
  PasswordForm profile_store_form;
  profile_store_form.signon_realm = "https://example.com";
  profile_store_form.username_value = u"example@gmail.com";
  profile_store_form.password_value = u"password";
  profile_store_form.in_store = PasswordForm::Store::kProfileStore;

  PasswordForm account_store_form;
  account_store_form.signon_realm = "https://example.com";
  account_store_form.username_value = u"example@gmail.com";
  account_store_form.password_value = u"password";
  account_store_form.in_store = PasswordForm::Store::kAccountStore;

  PasswordForm duplicate_account_store_form = account_store_form;
  duplicate_account_store_form.signon_realm = "https://m.example.com";

  profile_store().AddLogin(profile_store_form);
  account_store().AddLogin(account_store_form);
  account_store().AddLogin(duplicate_account_store_form);
  RunUntilIdle();

  ASSERT_THAT(profile_store().stored_passwords(),
              ElementsAre(Pair(profile_store_form.signon_realm,
                               ElementsAre(profile_store_form))));
  ASSERT_THAT(account_store().stored_passwords(),
              ElementsAre(Pair(account_store_form.signon_realm,
                               ElementsAre(account_store_form)),
                          Pair(duplicate_account_store_form.signon_realm,
                               ElementsAre(duplicate_account_store_form))));

  presenter().RemovePassword(account_store_form);
  RunUntilIdle();

  EXPECT_THAT(profile_store().stored_passwords(),
              ElementsAre(Pair(profile_store_form.signon_realm,
                               ElementsAre(profile_store_form))));
  EXPECT_TRUE(account_store().IsEmpty());
}

TEST_F(SavedPasswordsPresenterWithTwoStoresTest,
       ReturnsUsernamesForRealmFromSameStore) {
  PasswordForm form;
  form.signon_realm = "https://example.com";
  form.username_value = u"test1@gmail.com";
  form.password_value = u"password";
  form.in_store = PasswordForm::Store::kProfileStore;

  PasswordForm other_form;
  other_form = form;
  other_form.username_value = u"test2@gmail.com";

  PasswordForm account_store_form = other_form;
  account_store_form.username_value = u"test3@gmail.com";
  account_store_form.in_store = PasswordForm::Store::kAccountStore;

  profile_store().AddLogin(form);
  profile_store().AddLogin(other_form);

  account_store().AddLogin(account_store_form);

  RunUntilIdle();

  ASSERT_THAT(
      profile_store().stored_passwords(),
      ElementsAre(Pair(form.signon_realm, ElementsAre(form, other_form))));

  ASSERT_THAT(account_store().stored_passwords(),
              ElementsAre(Pair(account_store_form.signon_realm,
                               ElementsAre(account_store_form))));

  EXPECT_THAT(presenter().GetUsernamesForRealm(
                  form.signon_realm, /*is_using_account_store=*/false),
              ElementsAre(form.username_value, other_form.username_value));

  EXPECT_THAT(presenter().GetUsernamesForRealm(account_store_form.signon_realm,
                                               /*is_using_account_store=*/true),
              ElementsAre(account_store_form.username_value));
}

}  // namespace password_manager
