// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/password_undo_helper.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

using password_manager::PasswordForm;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Pair;
using testing::UnorderedElementsAre;

PasswordForm CreatePasswordForm() {
  PasswordForm form;
  form.url = GURL("http://test.com/");
  form.signon_realm = "http://test.com/";
  form.username_value = u"username";
  form.password_value = u"password";
  form.in_store = PasswordForm::Store::kProfileStore;
  return form;
}

class PasswordUndoHelperTest : public testing::Test {
 protected:
  PasswordUndoHelperTest() {
    profile_store_->Init(/*prefs=*/nullptr,
                         /*affiliated_match_helper=*/nullptr);
    account_store_->Init(/*prefs=*/nullptr,
                         /*affiliated_match_helper=*/nullptr);
  }

  ~PasswordUndoHelperTest() override {
    profile_store_->ShutdownOnUIThread();
    account_store_->ShutdownOnUIThread();
  }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }

  password_manager::TestPasswordStore* ProfileStore() {
    return profile_store_.get();
  }
  password_manager::TestPasswordStore* AccountStore() {
    return account_store_.get();
  }

  PasswordUndoHelper& UndoHelper() { return undo_helper_; }

 private:
  base::test::SingleThreadTaskEnvironment task_env_;
  scoped_refptr<password_manager::TestPasswordStore> profile_store_ =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  scoped_refptr<password_manager::TestPasswordStore> account_store_ =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(true));
  PasswordUndoHelper undo_helper_{profile_store_.get(), account_store_.get()};
};

TEST_F(PasswordUndoHelperTest, UndoSingleForm) {
  PasswordForm form = CreatePasswordForm();
  ProfileStore()->AddLogin(form);

  RunUntilIdle();

  ASSERT_THAT(ProfileStore()->stored_passwords(),
              ElementsAre(Pair(form.signon_realm, ElementsAre(form))));

  // Remove form
  UndoHelper().StartGroupingActions();
  ProfileStore()->RemoveLogin(FROM_HERE, form);
  UndoHelper().PasswordRemoved(form);
  UndoHelper().EndGroupingActions();
  RunUntilIdle();

  EXPECT_THAT(ProfileStore()->stored_passwords(), IsEmpty());

  UndoHelper().Undo();
  RunUntilIdle();
  EXPECT_THAT(ProfileStore()->stored_passwords(),
              ElementsAre(Pair(form.signon_realm, ElementsAre(form))));
}

// Tests that all removed forms are back after undo.
TEST_F(PasswordUndoHelperTest, UndoMultipleForms) {
  PasswordForm form_1 = CreatePasswordForm();
  PasswordForm form_2 = form_1;
  form_2.signon_realm = "https://google.com/";
  PasswordForm form_1_duplicate = form_1;
  form_1_duplicate.username_element = u"element";

  ProfileStore()->AddLogin(form_1);
  ProfileStore()->AddLogin(form_2);
  ProfileStore()->AddLogin(form_1_duplicate);

  RunUntilIdle();

  ASSERT_THAT(
      ProfileStore()->stored_passwords(),
      UnorderedElementsAre(Pair(form_1.signon_realm,
                                UnorderedElementsAre(form_1, form_1_duplicate)),
                           Pair(form_2.signon_realm, ElementsAre(form_2))));

  // Remove all forms
  UndoHelper().StartGroupingActions();
  ProfileStore()->RemoveLogin(FROM_HERE, form_1);
  ProfileStore()->RemoveLogin(FROM_HERE, form_2);
  ProfileStore()->RemoveLogin(FROM_HERE, form_1_duplicate);
  UndoHelper().PasswordRemoved(form_1);
  UndoHelper().PasswordRemoved(form_2);
  UndoHelper().PasswordRemoved(form_1_duplicate);
  UndoHelper().EndGroupingActions();
  RunUntilIdle();

  EXPECT_THAT(ProfileStore()->stored_passwords(), IsEmpty());
  // Undo forms removal.
  UndoHelper().Undo();
  RunUntilIdle();

  EXPECT_THAT(
      ProfileStore()->stored_passwords(),
      UnorderedElementsAre(Pair(form_1.signon_realm,
                                UnorderedElementsAre(form_1, form_1_duplicate)),
                           Pair(form_2.signon_realm, ElementsAre(form_2))));
}

TEST_F(PasswordUndoHelperTest, UndoFormsMultipleStores) {
  PasswordForm profile_form = CreatePasswordForm();
  PasswordForm account_form = CreatePasswordForm();
  account_form.in_store = password_manager::PasswordForm::Store::kAccountStore;

  ProfileStore()->AddLogin(profile_form);
  AccountStore()->AddLogin(account_form);
  RunUntilIdle();

  ASSERT_THAT(
      ProfileStore()->stored_passwords(),
      ElementsAre(Pair(profile_form.signon_realm, ElementsAre(profile_form))));
  ASSERT_THAT(
      AccountStore()->stored_passwords(),
      ElementsAre(Pair(account_form.signon_realm, ElementsAre(account_form))));

  // Remove forms
  UndoHelper().StartGroupingActions();
  ProfileStore()->RemoveLogin(FROM_HERE, profile_form);
  AccountStore()->RemoveLogin(FROM_HERE, account_form);
  UndoHelper().PasswordRemoved(profile_form);
  UndoHelper().PasswordRemoved(account_form);
  UndoHelper().EndGroupingActions();
  RunUntilIdle();

  EXPECT_THAT(ProfileStore()->stored_passwords(), IsEmpty());
  EXPECT_THAT(AccountStore()->stored_passwords(), IsEmpty());

  UndoHelper().Undo();
  RunUntilIdle();
  EXPECT_THAT(
      ProfileStore()->stored_passwords(),
      ElementsAre(Pair(profile_form.signon_realm, ElementsAre(profile_form))));
  EXPECT_THAT(
      AccountStore()->stored_passwords(),
      ElementsAre(Pair(account_form.signon_realm, ElementsAre(account_form))));
}

}  // namespace

}  // namespace password_manager
