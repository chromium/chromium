// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_manager_presenter.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ui/passwords/password_ui_view.h"
#include "chrome/browser/ui/passwords/password_ui_view_mock.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using testing::_;
using testing::Each;
using testing::Eq;
using testing::Property;

namespace {

MATCHER(IsNotBlacklisted, "") {
  return !arg->blacklisted_by_user;
}

}  // namespace

class PasswordManagerPresenterTest : public testing::Test {
 protected:
  PasswordManagerPresenterTest() {
    store_ = base::WrapRefCounted(static_cast<password_manager::PasswordStore*>(
        PasswordStoreFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                &profile_,
                base::BindRepeating(&password_manager::BuildPasswordStore<
                                    content::BrowserContext,
                                    password_manager::TestPasswordStore>))
            .get()));
  }

  ~PasswordManagerPresenterTest() override {
    store_->ShutdownOnUIThread();
    scoped_task_environment_.RunUntilIdle();
  }

  void AddPasswordEntry(const GURL& origin,
                        const std::string& user_name,
                        const std::string& password) {
    autofill::PasswordForm form;
    form.origin = origin;
    form.username_element = base::ASCIIToUTF16("Email");
    form.username_value = base::ASCIIToUTF16(user_name);
    form.password_element = base::ASCIIToUTF16("Passwd");
    form.password_value = base::ASCIIToUTF16(password);
    store_->AddLogin(form);
  }

  void AddPasswordException(const GURL& origin) {
    autofill::PasswordForm form;
    form.origin = origin;
    form.blacklisted_by_user = true;
    store_->AddLogin(form);
  }

  void UpdatePasswordLists() {
    mock_controller_.GetPasswordManagerPresenter()->UpdatePasswordLists();
    scoped_task_environment_.RunUntilIdle();
  }

  MockPasswordUIView& GetUIController() { return mock_controller_; }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  content::TestBrowserThreadBundle thread_bundle_{
      content::TestBrowserThreadBundle::PLAIN_MAINLOOP};
  TestingProfile profile_;
  MockPasswordUIView mock_controller_{&profile_};
  scoped_refptr<password_manager::PasswordStore> store_;

  DISALLOW_COPY_AND_ASSIGN(PasswordManagerPresenterTest);
};

namespace {

TEST_F(PasswordManagerPresenterTest, UIControllerIsCalled) {
  EXPECT_CALL(GetUIController(),
              SetPasswordList(Property(
                  &std::vector<std::unique_ptr<autofill::PasswordForm>>::size,
                  Eq(0u))));
  EXPECT_CALL(GetUIController(),
              SetPasswordExceptionList(Property(
                  &std::vector<std::unique_ptr<autofill::PasswordForm>>::size,
                  Eq(0u))));
  UpdatePasswordLists();

  GURL pass_origin("http://abc1.com");
  AddPasswordEntry(pass_origin, "test@gmail.com", "test");
  EXPECT_CALL(GetUIController(),
              SetPasswordList(Property(
                  &std::vector<std::unique_ptr<autofill::PasswordForm>>::size,
                  Eq(1u))));
  EXPECT_CALL(GetUIController(),
              SetPasswordExceptionList(Property(
                  &std::vector<std::unique_ptr<autofill::PasswordForm>>::size,
                  Eq(0u))));
  UpdatePasswordLists();

  GURL except_origin("http://abc2.com");
  AddPasswordException(except_origin);
  EXPECT_CALL(GetUIController(),
              SetPasswordList(Property(
                  &std::vector<std::unique_ptr<autofill::PasswordForm>>::size,
                  Eq(1u))));
  EXPECT_CALL(GetUIController(),
              SetPasswordExceptionList(Property(
                  &std::vector<std::unique_ptr<autofill::PasswordForm>>::size,
                  Eq(1u))));
  UpdatePasswordLists();

  GURL pass_origin2("http://example.com");
  AddPasswordEntry(pass_origin2, "test@gmail.com", "test");
  EXPECT_CALL(GetUIController(),
              SetPasswordList(Property(
                  &std::vector<std::unique_ptr<autofill::PasswordForm>>::size,
                  Eq(2u))));
  EXPECT_CALL(GetUIController(),
              SetPasswordExceptionList(Property(
                  &std::vector<std::unique_ptr<autofill::PasswordForm>>::size,
                  Eq(1u))));
  UpdatePasswordLists();
}

// Check that only stored passwords, not blacklisted entries, are provided for
// exporting.
TEST_F(PasswordManagerPresenterTest, BlacklistedPasswordsNotExported) {
  AddPasswordEntry(GURL("http://abc1.com"), "test@gmail.com", "test");
  AddPasswordException(GURL("http://abc2.com"));
  EXPECT_CALL(GetUIController(),
              SetPasswordList(Property(
                  &std::vector<std::unique_ptr<autofill::PasswordForm>>::size,
                  Eq(1u))));
  EXPECT_CALL(GetUIController(),
              SetPasswordExceptionList(Property(
                  &std::vector<std::unique_ptr<autofill::PasswordForm>>::size,
                  Eq(1u))));
  UpdatePasswordLists();

  std::vector<std::unique_ptr<autofill::PasswordForm>> passwords_for_export =
      GetUIController().GetPasswordManagerPresenter()->GetAllPasswords();
  EXPECT_EQ(1u, passwords_for_export.size());
  EXPECT_THAT(passwords_for_export, Each(IsNotBlacklisted()));
}

// Check that stored passwords are provided for exporting even if there is a
// blacklist entry for the same origin. This is needed to keep the user in
// control of all of their stored passwords.
TEST_F(PasswordManagerPresenterTest, BlacklistDoesNotPreventExporting) {
  const GURL kSameOrigin("https://abc.com");
  AddPasswordEntry(kSameOrigin, "test@gmail.com", "test");
  AddPasswordException(kSameOrigin);
  EXPECT_CALL(GetUIController(),
              SetPasswordList(Property(
                  &std::vector<std::unique_ptr<autofill::PasswordForm>>::size,
                  Eq(1u))));
  EXPECT_CALL(GetUIController(),
              SetPasswordExceptionList(Property(
                  &std::vector<std::unique_ptr<autofill::PasswordForm>>::size,
                  Eq(1u))));
  UpdatePasswordLists();

  std::vector<std::unique_ptr<autofill::PasswordForm>> passwords_for_export =
      GetUIController().GetPasswordManagerPresenter()->GetAllPasswords();
  ASSERT_EQ(1u, passwords_for_export.size());
  EXPECT_EQ(kSameOrigin, passwords_for_export[0]->origin);
}

}  // namespace
