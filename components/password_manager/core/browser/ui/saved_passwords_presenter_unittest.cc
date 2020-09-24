// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"

#include "base/memory/scoped_refptr.h"
#include "base/scoped_observer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/password_form.h"
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

  const base::string16 new_password = base::ASCIIToUTF16("new_password");
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
  EXPECT_CALL(observer, OnEdited).Times(0);
  EXPECT_CALL(observer, OnSavedPasswordsChanged).Times(0);
  EXPECT_FALSE(presenter().EditPassword(form, new_password));
  RunUntilIdle();

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
  profile_store_form.username_value = base::ASCIIToUTF16("profile@gmail.com");
  profile_store_form.password_value = base::ASCIIToUTF16("profile_pass");
  profile_store_form.in_store = PasswordForm::Store::kProfileStore;

  PasswordForm account_store_form1;
  account_store_form1.username_value = base::ASCIIToUTF16("account@gmail.com");
  account_store_form1.password_value = base::ASCIIToUTF16("account_pass");
  account_store_form1.in_store = PasswordForm::Store::kAccountStore;

  PasswordForm account_store_form2 = account_store_form1;
  account_store_form2.username_value = base::ASCIIToUTF16("account2@gmail.com");

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

}  // namespace password_manager
