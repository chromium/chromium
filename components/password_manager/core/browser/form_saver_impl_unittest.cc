// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_saver_impl.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using autofill::FormFieldData;
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
PasswordForm CreatePending(
    std::u16string_view username,
    std::u16string_view password,
    PasswordForm::MatchType match_type = PasswordForm::MatchType::kExact) {
  PasswordForm form = CreateObserved();
  form.username_value = std::u16string(username);
  form.password_value = std::u16string(password);
  form.match_type = match_type;
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
      : mock_store_(new StrictMock<MockPasswordStoreInterface>()),
        form_saver_(mock_store_.get()) {}

  FormSaverImplTest(const FormSaverImplTest&) = delete;
  FormSaverImplTest& operator=(const FormSaverImplTest&) = delete;

  ~FormSaverImplTest() override { mock_store_->ShutdownOnUIThread(); }

 protected:
  // For the MockPasswordStore.
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<StrictMock<MockPasswordStoreInterface>> mock_store_;
  FormSaverImpl form_saver_;
};

class FormSaverImplSaveTest
    : public FormSaverImplTest,
      public ::testing::WithParamInterface<SaveOperation> {
 protected:
  // Either saves, updates or replaces |pending| according to the test param.
  void SaveCredential(
      PasswordForm pending,
      const std::vector<raw_ptr<const PasswordForm, VectorExperimental>>&
          matches,
      const std::u16string& old_password);
};

void FormSaverImplSaveTest::SaveCredential(
    PasswordForm pending,
    const std::vector<raw_ptr<const PasswordForm, VectorExperimental>>& matches,
    const std::u16string& old_password) {
  PasswordForm expected = pending;
  switch (GetParam()) {
    case SaveOperation::kSave:
      expected.date_password_modified = base::Time::Now();
      EXPECT_CALL(*mock_store_, AddLogin(expected, _));
      return form_saver_.Save(std::move(pending), matches, old_password);
    case SaveOperation::kUpdate:
      if (old_password != pending.password_value) {
        expected.date_password_modified = base::Time::Now();
      }
      EXPECT_CALL(*mock_store_, UpdateLogin(expected, _));
      return form_saver_.Update(std::move(pending), matches, old_password);
    case SaveOperation::kReplaceUpdate: {
      PasswordForm old_key = CreatePending(u"some_other_username", u"1234");
      expected.date_password_modified = base::Time::Now();
      EXPECT_CALL(*mock_store_,
                  UpdateLoginWithPrimaryKey(expected, old_key, _));
      return form_saver_.UpdateReplace(std::move(pending), matches,
                                       old_password, old_key);
    }
  }
}

// Pushes the credential to the store without any matches.
TEST_P(FormSaverImplSaveTest, Write_EmptyStore) {
  PasswordForm pending = CreatePending(u"nameofuser", u"wordToP4a55");

  SaveCredential(pending, {} /* matches */,
                 std::u16string() /* old_password */);
}

// Pushes the credential to the store with |matches| containing the pending
// credential.
TEST_P(FormSaverImplSaveTest, Write_EmptyStoreWithPending) {
  PasswordForm pending = CreatePending(u"nameofuser", u"wordToP4a55");

  SaveCredential(pending, {&pending} /* matches */, pending.password_value);
}

// Pushes the credential to the store with |matches| containing the pending
// credential with an old password.
TEST_P(FormSaverImplSaveTest, Write_EmptyStoreWithPendingOldPassword) {
  PasswordForm pending = CreatePending(u"nameofuser", u"old_password");

  SaveCredential(CreatePending(u"nameofuser", u"new_password"),
                 {&pending} /* matches */, pending.password_value);
}

// Check that storing credentials with a non-empty username results in deleting
// credentials with the same password but empty username, if present in matches.
TEST_P(FormSaverImplSaveTest, Write_AndDeleteEmptyUsernameCredentials) {
  PasswordForm pending = CreatePending(u"nameofuser", u"wordToP4a55");

  PasswordForm non_empty_username = pending;
  non_empty_username.username_value = u"othername";

  PasswordForm no_username = pending;
  no_username.username_value.clear();
  const std::vector<raw_ptr<const PasswordForm, VectorExperimental>> matches = {
      &non_empty_username, &no_username};

  EXPECT_CALL(*mock_store_, RemoveLogin(_, no_username));
  SaveCredential(pending, matches, std::u16string());
}

// Check that storing credentials with a non-empty username does not result in
// deleting credentials with a different password, even if they have no
// username.
TEST_P(FormSaverImplSaveTest,
       Write_AndDoNotDeleteEmptyUsernameCredentialsWithDifferentPassword) {
  PasswordForm pending = CreatePending(u"nameofuser", u"wordToP4a55");

  PasswordForm no_username = pending;
  no_username.username_value.clear();
  no_username.password_value = u"abcd";

  EXPECT_CALL(*mock_store_, RemoveLogin).Times(0);
  SaveCredential(pending, {&no_username}, std::u16string());
}

// Check that if a credential without username is saved, and another credential
// with the same password (and a non-empty username) is present in best matches,
// nothing is deleted.
TEST_P(FormSaverImplSaveTest, Write_EmptyUsernameWillNotCauseDeletion) {
  PasswordForm pending = CreatePending(u"", u"wordToP4a55");

  PasswordForm with_username = pending;
  with_username.username_value = u"nameofuser";

  EXPECT_CALL(*mock_store_, RemoveLogin).Times(0);
  SaveCredential(pending, {&with_username}, std::u16string());
}

// Check that PSL-matched credentials in matches are exempt from deletion,
// even if they have an empty username and the same password as the pending
// credential.
TEST_P(FormSaverImplSaveTest, Write_AndDoNotDeleteEmptyUsernamePSLCredentials) {
  PasswordForm pending = CreatePending(u"nameofuser", u"wordToP4a55");

  PasswordForm stored = pending;
  PasswordForm no_username_psl = pending;
  no_username_psl.username_value.clear();
  no_username_psl.match_type = PasswordForm::MatchType::kPSL;
  const std::vector<raw_ptr<const PasswordForm, VectorExperimental>> matches = {
      &stored, &no_username_psl};

  EXPECT_CALL(*mock_store_, RemoveLogin).Times(0);
  SaveCredential(pending, matches, std::u16string());
}

// Check that on storing a credential, other credentials with the same password
// are not removed, as long as they have a non-empty username.
TEST_P(FormSaverImplSaveTest, Write_AndDoNotDeleteNonEmptyUsernameCredentials) {
  PasswordForm pending = CreatePending(u"nameofuser", u"wordToP4a55");

  PasswordForm other_username = pending;
  other_username.username_value = u"other username";

  EXPECT_CALL(*mock_store_, RemoveLogin).Times(0);
  SaveCredential(pending, {&other_username}, std::u16string());
}

// Stores a credential and makes sure that its duplicate is updated.
TEST_P(FormSaverImplSaveTest, Write_AndUpdatePasswordValuesOnExactMatch) {
  constexpr char16_t kOldPassword[] = u"old_password";
  constexpr char16_t kNewPassword[] = u"new_password";

  PasswordForm duplicate = CreatePending(u"nameofuser", kOldPassword);
  duplicate.url = GURL("https://example.in/somePath");

  PasswordForm expected_update = duplicate;
  expected_update.password_value = kNewPassword;
  expected_update.date_password_modified = base::Time::Now();

  EXPECT_CALL(*mock_store_, UpdateLogin(expected_update, _));
  SaveCredential(CreatePending(u"nameofuser", kNewPassword), {&duplicate},
                 kOldPassword);
}

// Stores a credential and makes sure that its PSL duplicate is updated.
TEST_P(FormSaverImplSaveTest, Write_AndUpdatePasswordValuesOnPSLMatch) {
  constexpr char16_t kOldPassword[] = u"old_password";
  constexpr char16_t kNewPassword[] = u"new_password";

  PasswordForm duplicate =
      CreatePending(u"nameofuser", kOldPassword, PasswordForm::MatchType::kPSL);
  duplicate.url = GURL("https://www.example.in");
  duplicate.signon_realm = duplicate.url.spec();

  PasswordForm expected_update = duplicate;
  expected_update.password_value = kNewPassword;
  expected_update.date_password_modified = base::Time::Now();
  EXPECT_CALL(*mock_store_, UpdateLogin(expected_update, _));
  SaveCredential(CreatePending(u"nameofuser", kNewPassword), {&duplicate},
                 kOldPassword);
}

// Stores a credential and makes sure that not exact matches are not updated.
TEST_P(FormSaverImplSaveTest, Write_AndUpdatePasswordValues_IgnoreNonMatches) {
  constexpr char16_t kOldPassword[] = u"old_password";
  constexpr char16_t kNewPassword[] = u"new_password";
  PasswordForm pending = CreatePending(u"nameofuser", kOldPassword);

  PasswordForm different_username = pending;
  different_username.username_value = u"someuser";

  PasswordForm different_password = pending;
  different_password.password_value = u"some_password";

  PasswordForm empty_username = pending;
  empty_username.username_value.clear();
  const std::vector<raw_ptr<const PasswordForm, VectorExperimental>> matches = {
      &different_username, &different_password, &empty_username};

  pending.password_value = kNewPassword;
  EXPECT_CALL(*mock_store_, UpdateLogin).Times(0);
  EXPECT_CALL(*mock_store_, UpdateLoginWithPrimaryKey).Times(0);
  SaveCredential(pending, matches, kOldPassword);
}

// Check that on saving the pending form |form_data| is sanitized.
TEST_P(FormSaverImplSaveTest, FormDataSanitized) {
  PasswordForm pending = CreatePending(u"nameofuser", u"wordToP4a55");
  FormFieldData field;
  field.set_name(u"name");
  field.set_form_control_type(autofill::FormControlType::kInputPassword);
  field.set_value(u"value");
  field.set_label(u"label");
  field.set_placeholder(u"placeholder");
  field.set_id_attribute(u"id");
  field.set_name_attribute(field.name());
  field.set_css_classes(u"css_classes");
  pending.form_data.set_fields({field});

  PasswordForm saved;
  switch (GetParam()) {
    case SaveOperation::kSave:
      EXPECT_CALL(*mock_store_, AddLogin).WillOnce(SaveArg<0>(&saved));
      return form_saver_.Save(std::move(pending), {}, u"");
    case SaveOperation::kUpdate:
      EXPECT_CALL(*mock_store_, UpdateLogin).WillOnce(SaveArg<0>(&saved));
      return form_saver_.Update(std::move(pending), {}, u"");
    case SaveOperation::kReplaceUpdate: {
      PasswordForm old_key = CreatePending(u"some_other_username", u"1234");
      EXPECT_CALL(*mock_store_, UpdateLoginWithPrimaryKey(_, old_key, _))
          .WillOnce(SaveArg<0>(&saved));
      return form_saver_.UpdateReplace(std::move(pending), {}, u"", old_key);
    }
  }

  ASSERT_EQ(1u, saved.form_data.fields().size());
  const FormFieldData& saved_field = saved.form_data.fields()[0];
  EXPECT_EQ(u"name", saved_field.name());
  EXPECT_EQ(autofill::FormControlType::kInputPassword,
            saved_field.form_control_type());
  EXPECT_TRUE(saved_field.value().empty());
  EXPECT_TRUE(saved_field.label().empty());
  EXPECT_TRUE(saved_field.placeholder().empty());
  EXPECT_TRUE(saved_field.id_attribute().empty());
  EXPECT_TRUE(saved_field.name_attribute().empty());
  EXPECT_TRUE(saved_field.css_classes().empty());
}

INSTANTIATE_TEST_SUITE_P(All,
                         FormSaverImplSaveTest,
                         ::testing::Values(SaveOperation::kSave,
                                           SaveOperation::kUpdate,
                                           SaveOperation::kReplaceUpdate));

// Check that blocklisting an observed form sets the right properties and calls
// the PasswordStore.
TEST_F(FormSaverImplTest, Blocklist) {
  PasswordForm observed = CreateObserved();
  observed.blocked_by_user = false;
  observed.username_value = u"user1";
  observed.username_element = u"user";
  observed.password_value = u"12345";
  observed.password_element = u"password";
  observed.all_alternative_usernames = {{AlternativeElement::Value(u"user2"),
                                         autofill::FieldRendererId(1),
                                         AlternativeElement::Name(u"field")}};
  observed.url = GURL("https://www.example.com/foobar");

  PasswordForm blocklisted =
      password_manager_util::MakeNormalizedBlocklistedForm(
          PasswordFormDigest(observed));

  EXPECT_CALL(*mock_store_, AddLogin(FormWithSomeDate(blocklisted), _));
  PasswordForm result = form_saver_.Blocklist(PasswordFormDigest(observed));
  EXPECT_THAT(result, FormWithSomeDate(blocklisted));
}

// Check that Remove() method is relayed properly.
TEST_F(FormSaverImplTest, Remove) {
  PasswordForm form = CreatePending(u"nameofuser", u"wordToP4a55");

  EXPECT_CALL(*mock_store_, RemoveLogin(_, form));
  form_saver_.Remove(form);
}

}  // namespace password_manager
