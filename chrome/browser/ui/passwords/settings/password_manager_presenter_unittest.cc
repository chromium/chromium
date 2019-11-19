// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/settings/password_manager_presenter.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ui/passwords/settings/password_ui_view.h"
#include "chrome/browser/ui/passwords/settings/password_ui_view_mock.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/password_list_sorter.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Each;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Pair;
using testing::SizeIs;
using testing::UnorderedElementsAre;

namespace {

constexpr char kExampleCom[] = "https://example.com/";
constexpr char kExampleOrg[] = "https://example.org/";
constexpr char kNewPass[] = "new_pass";
constexpr char kNewUser[] = "new_user";
constexpr char kPassword[] = "pass";
constexpr char kPassword2[] = "pass2";
constexpr char kUsername[] = "user";
constexpr char kUsername2[] = "user2";

MATCHER(IsNotBlacklisted, "") {
  return !arg->blacklisted_by_user;
}

std::vector<std::pair<std::string, std::string>> GetUsernamesAndPasswords(
    const std::vector<autofill::PasswordForm>& forms) {
  std::vector<std::pair<std::string, std::string>> result;
  result.reserve(forms.size());
  for (const auto& form : forms) {
    result.emplace_back(base::UTF16ToUTF8(form.username_value),
                        base::UTF16ToUTF8(form.password_value));
  }

  return result;
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
    task_environment_.RunUntilIdle();
  }

  void AddPasswordEntry(const GURL& origin,
                        base::StringPiece username,
                        base::StringPiece password) {
    autofill::PasswordForm form;
    form.origin = origin;
    form.signon_realm = origin.GetOrigin().spec();
    form.username_element = base::ASCIIToUTF16("Email");
    form.username_value = base::ASCIIToUTF16(username);
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

  void ChangeSavedPasswordBySortKey(
      base::StringPiece origin,
      base::StringPiece old_username,
      base::StringPiece old_password,
      base::StringPiece new_username,
      base::Optional<base::StringPiece> new_password) {
    autofill::PasswordForm temp_form;
    temp_form.origin = GURL(origin);
    temp_form.signon_realm = temp_form.origin.GetOrigin().spec();
    temp_form.username_element = base::ASCIIToUTF16("username");
    temp_form.password_element = base::ASCIIToUTF16("password");
    temp_form.username_value = base::ASCIIToUTF16(old_username);
    temp_form.password_value = base::ASCIIToUTF16(old_password);

    mock_controller_.GetPasswordManagerPresenter()->ChangeSavedPassword(
        password_manager::CreateSortKey(temp_form),
        base::ASCIIToUTF16(new_username),
        new_password ? base::make_optional(base::ASCIIToUTF16(*new_password))
                     : base::nullopt);
    // The password store posts mutation tasks to a background thread, thus we
    // need to spin the message loop here.
    task_environment_.RunUntilIdle();
  }

  void UpdatePasswordLists() {
    mock_controller_.GetPasswordManagerPresenter()->UpdatePasswordLists();
    task_environment_.RunUntilIdle();
  }

  MockPasswordUIView& GetUIController() { return mock_controller_; }

  const std::vector<autofill::PasswordForm>& GetStoredPasswordsForRealm(
      base::StringPiece signon_realm) {
    const auto& stored_passwords =
        static_cast<const password_manager::TestPasswordStore&>(*store_)
            .stored_passwords();
    auto for_realm_it = stored_passwords.find(signon_realm);
    EXPECT_NE(stored_passwords.end(), for_realm_it);
    return for_realm_it->second;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  MockPasswordUIView mock_controller_{&profile_};
  scoped_refptr<password_manager::PasswordStore> store_;

  DISALLOW_COPY_AND_ASSIGN(PasswordManagerPresenterTest);
};

namespace {

TEST_F(PasswordManagerPresenterTest,
       ChangeSavedPasswordBySortKey_RejectEmptyPassword) {
  AddPasswordEntry(GURL(kExampleCom), kUsername, kPassword);
  EXPECT_CALL(GetUIController(), SetPasswordList(SizeIs(1)));
  EXPECT_CALL(GetUIController(), SetPasswordExceptionList(IsEmpty()));
  UpdatePasswordLists();
  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleCom)),
              ElementsAre(Pair(kUsername, kPassword)));
  testing::Mock::VerifyAndClearExpectations(&GetUIController());

  ChangeSavedPasswordBySortKey(kExampleCom, kUsername, kPassword, kNewUser, "");
  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleCom)),
              ElementsAre(Pair(kUsername, kPassword)));
}

TEST_F(PasswordManagerPresenterTest,
       ChangeSavedPasswordBySortKey_ChangeUsername) {
  AddPasswordEntry(GURL(kExampleCom), kUsername, kPassword);
  EXPECT_CALL(GetUIController(), SetPasswordList(SizeIs(1)));
  EXPECT_CALL(GetUIController(), SetPasswordExceptionList(IsEmpty()));
  UpdatePasswordLists();
  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleCom)),
              ElementsAre(Pair(kUsername, kPassword)));
  testing::Mock::VerifyAndClearExpectations(&GetUIController());

  ChangeSavedPasswordBySortKey(kExampleCom, kUsername, kPassword, kNewUser,
                               base::nullopt);
  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleCom)),
              ElementsAre(Pair(kNewUser, kPassword)));
}

TEST_F(PasswordManagerPresenterTest,
       ChangeSavedPasswordBySortKey_ChangeUsernameAndPassword) {
  AddPasswordEntry(GURL(kExampleCom), kUsername, kPassword);
  EXPECT_CALL(GetUIController(), SetPasswordList(SizeIs(1)));
  EXPECT_CALL(GetUIController(), SetPasswordExceptionList(IsEmpty()));
  UpdatePasswordLists();
  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleCom)),
              ElementsAre(Pair(kUsername, kPassword)));
  testing::Mock::VerifyAndClearExpectations(&GetUIController());

  ChangeSavedPasswordBySortKey(kExampleCom, kUsername, kPassword, kNewUser,
                               kNewPass);
  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleCom)),
              ElementsAre(Pair(kNewUser, kNewPass)));
}

TEST_F(PasswordManagerPresenterTest,
       ChangeSavedPasswordBySortKey_RejectSameUsernameForSameRealm) {
  AddPasswordEntry(GURL(kExampleCom), kUsername, kPassword);
  AddPasswordEntry(GURL(kExampleCom), kUsername2, kPassword2);
  EXPECT_CALL(GetUIController(), SetPasswordList(SizeIs(2)));
  EXPECT_CALL(GetUIController(), SetPasswordExceptionList(IsEmpty()));
  UpdatePasswordLists();
  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleCom)),
              UnorderedElementsAre(Pair(kUsername, kPassword),
                                   Pair(kUsername2, kPassword2)));
  testing::Mock::VerifyAndClearExpectations(&GetUIController());

  ChangeSavedPasswordBySortKey(kExampleCom, kUsername, kPassword, kUsername2,
                               base::nullopt);
  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleCom)),
              UnorderedElementsAre(Pair(kUsername, kPassword),
                                   Pair(kUsername2, kPassword2)));
}

TEST_F(PasswordManagerPresenterTest,
       ChangeSavedPasswordBySortKey_DontRejectSameUsernameForDifferentRealm) {
  AddPasswordEntry(GURL(kExampleCom), kUsername, kPassword);
  AddPasswordEntry(GURL(kExampleOrg), kUsername2, kPassword2);
  EXPECT_CALL(GetUIController(), SetPasswordList(SizeIs(2)));
  EXPECT_CALL(GetUIController(), SetPasswordExceptionList(IsEmpty()));
  UpdatePasswordLists();
  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleCom)),
              ElementsAre(Pair(kUsername, kPassword)));
  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleOrg)),
              ElementsAre(Pair(kUsername2, kPassword2)));
  testing::Mock::VerifyAndClearExpectations(&GetUIController());

  ChangeSavedPasswordBySortKey(kExampleCom, kUsername, kPassword, kUsername2,
                               base::nullopt);
  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleCom)),
              ElementsAre(Pair(kUsername2, kPassword)));
  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleOrg)),
              ElementsAre(Pair(kUsername2, kPassword2)));
}

TEST_F(PasswordManagerPresenterTest,
       ChangeSavedPasswordBySortKey_UpdateDuplicates) {
  AddPasswordEntry(GURL(std::string(kExampleCom) + "pathA"), kUsername,
                   kPassword);
  AddPasswordEntry(GURL(std::string(kExampleCom) + "pathB"), kUsername,
                   kPassword);
  EXPECT_CALL(GetUIController(), SetPasswordList(SizeIs(1)));
  EXPECT_CALL(GetUIController(), SetPasswordExceptionList(IsEmpty()));
  UpdatePasswordLists();
  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleCom)),
              UnorderedElementsAre(Pair(kUsername, kPassword),
                                   Pair(kUsername, kPassword)));
  testing::Mock::VerifyAndClearExpectations(&GetUIController());

  ChangeSavedPasswordBySortKey(kExampleCom, kUsername, kPassword, kNewUser,
                               kNewPass);
  EXPECT_THAT(
      GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleCom)),
      UnorderedElementsAre(Pair(kNewUser, kNewPass), Pair(kNewUser, kNewPass)));
}

TEST_F(PasswordManagerPresenterTest,
       ChangeSavedPasswordBySortKey_EditUsernameForTheRightCredential) {
  AddPasswordEntry(GURL(kExampleCom), kUsername, kPassword);
  AddPasswordEntry(GURL(kExampleCom), kUsername2, kPassword);
  AddPasswordEntry(GURL(kExampleOrg), kUsername, kPassword);
  AddPasswordEntry(GURL(kExampleOrg), kUsername2, kPassword);
  EXPECT_CALL(GetUIController(), SetPasswordList(SizeIs(4)));
  EXPECT_CALL(GetUIController(), SetPasswordExceptionList(IsEmpty()));
  UpdatePasswordLists();
  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleCom)),
              UnorderedElementsAre(Pair(kUsername, kPassword),
                                   Pair(kUsername2, kPassword)));
  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleOrg)),
              UnorderedElementsAre(Pair(kUsername, kPassword),
                                   Pair(kUsername2, kPassword)));
  testing::Mock::VerifyAndClearExpectations(&GetUIController());

  ChangeSavedPasswordBySortKey(kExampleCom, kUsername, kPassword, kNewUser,
                               kPassword);
  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleCom)),
              UnorderedElementsAre(Pair(kNewUser, kPassword),
                                   Pair(kUsername2, kPassword)));
  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleOrg)),
              UnorderedElementsAre(Pair(kUsername, kPassword),
                                   Pair(kUsername2, kPassword)));
}

TEST_F(PasswordManagerPresenterTest,
       ChangeSavedPasswordBySortKey_EditPasswordForTheRightCredential) {
  AddPasswordEntry(GURL(kExampleCom), kUsername, kPassword);
  AddPasswordEntry(GURL(kExampleCom), kUsername2, kPassword);
  AddPasswordEntry(GURL(kExampleOrg), kUsername, kPassword);
  AddPasswordEntry(GURL(kExampleOrg), kUsername2, kPassword);
  EXPECT_CALL(GetUIController(), SetPasswordList(SizeIs(4)));
  EXPECT_CALL(GetUIController(), SetPasswordExceptionList(IsEmpty()));
  UpdatePasswordLists();
  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleCom)),
              UnorderedElementsAre(Pair(kUsername, kPassword),
                                   Pair(kUsername2, kPassword)));
  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleOrg)),
              UnorderedElementsAre(Pair(kUsername, kPassword),
                                   Pair(kUsername2, kPassword)));
  testing::Mock::VerifyAndClearExpectations(&GetUIController());

  ChangeSavedPasswordBySortKey(kExampleCom, kUsername, kPassword, kUsername,
                               kNewPass);
  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleCom)),
              UnorderedElementsAre(Pair(kUsername, kNewPass),
                                   Pair(kUsername2, kPassword)));
  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleOrg)),
              UnorderedElementsAre(Pair(kUsername, kPassword),
                                   Pair(kUsername2, kPassword)));
}

TEST_F(PasswordManagerPresenterTest, UIControllerIsCalled) {
  EXPECT_CALL(GetUIController(), SetPasswordList(IsEmpty()));
  EXPECT_CALL(GetUIController(), SetPasswordExceptionList(IsEmpty()));
  UpdatePasswordLists();

  GURL pass_origin("http://abc1.com");
  AddPasswordEntry(pass_origin, "test@gmail.com", "test");
  EXPECT_CALL(GetUIController(), SetPasswordList(SizeIs(1u)));
  EXPECT_CALL(GetUIController(), SetPasswordExceptionList(IsEmpty()));
  UpdatePasswordLists();

  GURL except_origin("http://abc2.com");
  AddPasswordException(except_origin);
  EXPECT_CALL(GetUIController(), SetPasswordList(SizeIs(1u)));
  EXPECT_CALL(GetUIController(), SetPasswordExceptionList(SizeIs(1u)));
  UpdatePasswordLists();

  GURL pass_origin2("http://example.com");
  AddPasswordEntry(pass_origin2, "test@gmail.com", "test");
  EXPECT_CALL(GetUIController(), SetPasswordList(SizeIs(2u)));
  EXPECT_CALL(GetUIController(), SetPasswordExceptionList(SizeIs(1u)));
  UpdatePasswordLists();
}

// Check that only stored passwords, not blacklisted entries, are provided for
// exporting.
TEST_F(PasswordManagerPresenterTest, BlacklistedPasswordsNotExported) {
  AddPasswordEntry(GURL("http://abc1.com"), "test@gmail.com", "test");
  AddPasswordException(GURL("http://abc2.com"));
  EXPECT_CALL(GetUIController(), SetPasswordList(SizeIs(1u)));
  EXPECT_CALL(GetUIController(), SetPasswordExceptionList(SizeIs(1u)));
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
  EXPECT_CALL(GetUIController(), SetPasswordList(SizeIs(1u)));
  EXPECT_CALL(GetUIController(), SetPasswordExceptionList(SizeIs(1u)));
  UpdatePasswordLists();

  std::vector<std::unique_ptr<autofill::PasswordForm>> passwords_for_export =
      GetUIController().GetPasswordManagerPresenter()->GetAllPasswords();
  ASSERT_EQ(1u, passwords_for_export.size());
  EXPECT_EQ(kSameOrigin, passwords_for_export[0]->origin);
}

}  // namespace
