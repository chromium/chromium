// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_saver_impl.h"

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using autofill::FormFieldData;
using base::ASCIIToUTF16;
using base::StringPiece;
using testing::_;
using testing::DoAll;
using testing::SaveArg;
using testing::StrictMock;

namespace password_manager {

namespace {

// Creates a dummy observed form with some basic arbitrary values.
PasswordForm CreateObserved() {
  PasswordForm form;
  form.url = GURL("https://example.in");
  form.signon_realm = form.url.spec();
  form.action = GURL("https://login.example.org");
  return form;
}

// Creates a dummy pending (for saving) form with some basic arbitrary values
// and |username| and |password| values as specified.
PasswordForm CreatePending(StringPiece username, StringPiece password) {
  PasswordForm form = CreateObserved();
  form.username_value = ASCIIToUTF16(username);
  form.password_value = ASCIIToUTF16(password);
  return form;
}

MATCHER_P(FormWithSomeDate, expected, "") {
  PasswordForm expected_with_date = expected;
  expected_with_date.date_created = arg.date_created;
  return arg == expected_with_date && arg.date_created != base::Time();
}

enum class SaveOperation {
  kSave,
  kUpdate,
  kReplaceUpdate,
};

}  // namespace

class FormSaverImplTest : public testing::Test {
 public:
  FormSaverImplTest()
      : mock_store_(new StrictMock<MockPasswordStore>()),
        form_saver_(mock_store_.get()) {}

  ~FormSaverImplTest() override { mock_store_->ShutdownOnUIThread(); }

 protected:
  // For the MockPasswordStore.
  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<StrictMock<MockPasswordStore>> mock_store_;
  FormSaverImpl form_saver_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FormSaverImplTest);
};

class FormSaverImplSaveTest
    : public FormSaverImplTest,
      public ::testing::WithParamInterface<SaveOperation> {
 protected:
  // Either saves, updates or replaces |pending| according to the test param.
  void SaveCredential(PasswordForm pending,
                      const std::vector<const PasswordForm*>& matches,
                      const base::string16& old_password);
};

void FormSaverImplSaveTest::SaveCredential(
    PasswordForm pending,
    const std::vector<const PasswordForm*>& matches,
    const base::string16& old_password) {
  switch (GetParam()) {
    case SaveOperation::kSave:
      EXPECT_CALL(*mock_store_, AddLogin(pending));
      return form_saver_.Save(std::move(pending), matches, old_password);
    case SaveOperation::kUpdate:
      EXPECT_CALL(*mock_store_, UpdateLogin(pending));
      return form_saver_.Update(std::move(pending), matches, old_password);
    case SaveOperation::kReplaceUpdate: {
      PasswordForm old_key = CreatePending("some_other_username", "1234");
      EXPECT_CALL(*mock_store_, UpdateLoginWithPrimaryKey(pending, old_key));
      return form_saver_.UpdateReplace(std::move(pending), matches,
                                       old_password, old_key);
    }
  }
}

// Pushes the credential to the store without any matches.
TEST_P(FormSaverImplSaveTest, Write_EmptyStore) {
  PasswordForm pending = CreatePending("nameofuser", "wordToP4a55");

  SaveCredential(pending, {} /* matches */,
                 base::string16() /* old_password */);
}

// Pushes the credential to the store with |matches| containing the pending
// credential.
TEST_P(FormSaverImplSaveTest, Write_EmptyStoreWithPending) {
  PasswordForm pending = CreatePending("nameofuser", "wordToP4a55");

  SaveCredential(pending, {&pending} /* matches */, pending.password_value);
}

// Pushes the credential to the store with |matches| containing the pending
// credential with an old password.
TEST_P(FormSaverImplSaveTest, Write_EmptyStoreWithPendingOldPassword) {
  PasswordForm pending = CreatePending("nameofuser", "old_password");

  SaveCredential(CreatePending("nameofuser", "new_password"),
                 {&pending} /* matches */, pending.password_value);
}

// Check that storing credentials with a non-empty username results in deleting
// credentials with the same password but empty username, if present in matches.
TEST_P(FormSaverImplSaveTest, Write_AndDeleteEmptyUsernameCredentials) {
  PasswordForm pending = CreatePending("nameofuser", "wordToP4a55");

  PasswordForm non_empty_username = pending;
  non_empty_username.username_value = ASCIIToUTF16("othername");

  PasswordForm no_username = pending;
  no_username.username_value.clear();
  const std::vector<const PasswordForm*> matches = {&non_empty_username,
                                                    &no_username};

  EXPECT_CALL(*mock_store_, RemoveLogin(no_username));
  SaveCredential(pending, matches, base::string16());
}

// Check that storing credentials with a non-empty username does not result in
// deleting credentials with a different password, even if they have no
// username.
TEST_P(FormSaverImplSaveTest,
       Write_AndDoNotDeleteEmptyUsernameCredentialsWithDifferentPassword) {
  PasswordForm pending = CreatePending("nameofuser", "wordToP4a55");

  PasswordForm no_username = pending;
  no_username.username_value.clear();
  no_username.password_value = ASCIIToUTF16("abcd");

  EXPECT_CALL(*mock_store_, RemoveLogin(_)).Times(0);
  SaveCredential(pending, {&no_username}, base::string16());
}

// Check that if a credential without username is saved, and another credential
// with the same password (and a non-empty username) is present in best matches,
// nothing is deleted.
TEST_P(FormSaverImplSaveTest, Write_EmptyUsernameWillNotCauseDeletion) {
  PasswordForm pending = CreatePending("", "wordToP4a55");

  PasswordForm with_username = pending;
  with_username.username_value = ASCIIToUTF16("nameofuser");

  EXPECT_CALL(*mock_store_, RemoveLogin(_)).Times(0);
  SaveCredential(pending, {&with_username}, base::string16());
}

// Check that PSL-matched credentials in matches are exempt from deletion,
// even if they have an empty username and the same password as the pending
// credential.
TEST_P(FormSaverImplSaveTest, Write_AndDoNotDeleteEmptyUsernamePSLCredentials) {
  PasswordForm pending = CreatePending("nameofuser", "wordToP4a55");

  PasswordForm stored = pending;
  PasswordForm no_username_psl = pending;
  no_username_psl.username_value.clear();
  no_username_psl.is_public_suffix_match = true;
  const std::vector<const PasswordForm*> matches = {&stored, &no_username_psl};

  EXPECT_CALL(*mock_store_, RemoveLogin(_)).Times(0);
  SaveCredential(pending, matches, base::string16());
}

// Check that on storing a credential, other credentials with the same password
// are not removed, as long as they have a non-empty username.
TEST_P(FormSaverImplSaveTest, Write_AndDoNotDeleteNonEmptyUsernameCredentials) {
  PasswordForm pending = CreatePending("nameofuser", "wordToP4a55");

  PasswordForm other_username = pending;
  other_username.username_value = ASCIIToUTF16("other username");

  EXPECT_CALL(*mock_store_, RemoveLogin(_)).Times(0);
  SaveCredential(pending, {&other_username}, base::string16());
}

// Stores a credential and makes sure that its duplicate is updated.
TEST_P(FormSaverImplSaveTest, Write_AndUpdatePasswordValuesOnExactMatch) {
  constexpr char kOldPassword[] = "old_password";
  constexpr char kNewPassword[] = "new_password";

  PasswordForm duplicate = CreatePending("nameofuser", kOldPassword);
  duplicate.url = GURL("https://example.in/somePath");

  PasswordForm expected_update = duplicate;
  expected_update.password_value = ASCIIToUTF16(kNewPassword);

  EXPECT_CALL(*mock_store_, UpdateLogin(expected_update));
  SaveCredential(CreatePending("nameofuser", kNewPassword), {&duplicate},
                 ASCIIToUTF16(kOldPassword));
}

// Stores a credential and makes sure that its PSL duplicate is updated.
TEST_P(FormSaverImplSaveTest, Write_AndUpdatePasswordValuesOnPSLMatch) {
  constexpr char kOldPassword[] = "old_password";
  constexpr char kNewPassword[] = "new_password";

  PasswordForm duplicate = CreatePending("nameofuser", kOldPassword);
  duplicate.url = GURL("https://www.example.in");
  duplicate.signon_realm = duplicate.url.spec();
  duplicate.is_public_suffix_match = true;

  PasswordForm expected_update = duplicate;
  expected_update.password_value = ASCIIToUTF16(kNewPassword);
  EXPECT_CALL(*mock_store_, UpdateLogin(expected_update));
  SaveCredential(CreatePending("nameofuser", kNewPassword), {&duplicate},
                 ASCIIToUTF16(kOldPassword));
}

// Stores a credential and makes sure that not exact matches are not updated.
TEST_P(FormSaverImplSaveTest, Write_AndUpdatePasswordValues_IgnoreNonMatches) {
  constexpr char kOldPassword[] = "old_password";
  constexpr char kNewPassword[] = "new_password";
  PasswordForm pending = CreatePending("nameofuser", kOldPassword);

  PasswordForm different_username = pending;
  different_username.username_value = ASCIIToUTF16("someuser");

  PasswordForm different_password = pending;
  different_password.password_value = ASCIIToUTF16("some_password");

  PasswordForm empty_username = pending;
  empty_username.username_value.clear();
  const std::vector<const PasswordForm*> matches = {
      &different_username, &different_password, &empty_username};

  pending.password_value = ASCIIToUTF16(kNewPassword);
  EXPECT_CALL(*mock_store_, UpdateLogin(_)).Times(0);
  EXPECT_CALL(*mock_store_, UpdateLoginWithPrimaryKey(_, _)).Times(0);
  SaveCredential(pending, matches, ASCIIToUTF16(kOldPassword));
}

// Check that on saving the pending form |form_data| is sanitized.
TEST_P(FormSaverImplSaveTest, FormDataSanitized) {
  PasswordForm pending = CreatePending("nameofuser", "wordToP4a55");
  FormFieldData field;
  field.name = ASCIIToUTF16("name");
  field.form_control_type = "password";
  field.value = ASCIIToUTF16("value");
  field.label = ASCIIToUTF16("label");
  field.placeholder = ASCIIToUTF16("placeholder");
  field.id_attribute = ASCIIToUTF16("id");
  field.name_attribute = field.name;
  field.css_classes = ASCIIToUTF16("css_classes");
  pending.form_data.fields.push_back(field);

  PasswordForm saved;
  switch (GetParam()) {
    case SaveOperation::kSave:
      EXPECT_CALL(*mock_store_, AddLogin(_)).WillOnce(SaveArg<0>(&saved));
      return form_saver_.Save(std::move(pending), {}, ASCIIToUTF16(""));
    case SaveOperation::kUpdate:
      EXPECT_CALL(*mock_store_, UpdateLogin(_)).WillOnce(SaveArg<0>(&saved));
      return form_saver_.Update(std::move(pending), {}, ASCIIToUTF16(""));
    case SaveOperation::kReplaceUpdate: {
      PasswordForm old_key = CreatePending("some_other_username", "1234");
      EXPECT_CALL(*mock_store_, UpdateLoginWithPrimaryKey(_, old_key))
          .WillOnce(SaveArg<0>(&saved));
      return form_saver_.UpdateReplace(std::move(pending), {}, ASCIIToUTF16(""),
                                       old_key);
    }
  }

  ASSERT_EQ(1u, saved.form_data.fields.size());
  const FormFieldData& saved_field = saved.form_data.fields[0];
  EXPECT_EQ(ASCIIToUTF16("name"), saved_field.name);
  EXPECT_EQ("password", saved_field.form_control_type);
  EXPECT_TRUE(saved_field.value.empty());
  EXPECT_TRUE(saved_field.label.empty());
  EXPECT_TRUE(saved_field.placeholder.empty());
  EXPECT_TRUE(saved_field.id_attribute.empty());
  EXPECT_TRUE(saved_field.name_attribute.empty());
  EXPECT_TRUE(saved_field.css_classes.empty());
}

INSTANTIATE_TEST_SUITE_P(All,
                         FormSaverImplSaveTest,
                         ::testing::Values(SaveOperation::kSave,
                                           SaveOperation::kUpdate,
                                           SaveOperation::kReplaceUpdate));

// Check that blacklisting an observed form sets the right properties and calls
// the PasswordStore.
TEST_F(FormSaverImplTest, PermanentlyBlacklist) {
  PasswordForm observed = CreateObserved();
  observed.blocked_by_user = false;
  observed.username_value = ASCIIToUTF16("user1");
  observed.username_element = ASCIIToUTF16("user");
  observed.password_value = ASCIIToUTF16("12345");
  observed.password_element = ASCIIToUTF16("password");
  observed.all_possible_usernames = {
      {ASCIIToUTF16("user2"), ASCIIToUTF16("field")}};
  observed.url = GURL("https://www.example.com/foobar");

  PasswordForm blacklisted =
      password_manager_util::MakeNormalizedBlacklistedForm(
          PasswordStore::FormDigest(observed));

  EXPECT_CALL(*mock_store_, AddLogin(FormWithSomeDate(blacklisted)));
  PasswordForm result =
      form_saver_.PermanentlyBlacklist(PasswordStore::FormDigest(observed));
  EXPECT_THAT(result, FormWithSomeDate(blacklisted));
}

// Check that Remove() method is relayed properly.
TEST_F(FormSaverImplTest, Remove) {
  PasswordForm form = CreatePending("nameofuser", "wordToP4a55");

  EXPECT_CALL(*mock_store_, RemoveLogin(form));
  form_saver_.Remove(form);
}

}  // namespace password_manager
