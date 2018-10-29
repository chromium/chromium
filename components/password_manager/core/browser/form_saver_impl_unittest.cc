// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_saver_impl.h"

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using autofill::FormFieldData;
using autofill::PasswordForm;
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
  form.origin = GURL("https://example.in");
  form.signon_realm = form.origin.spec();
  form.action = GURL("https://login.example.org");
  return form;
}

// Creates a dummy pending (for saving) form with some basic arbitrary values
// and |username| and |password| values as specified.
PasswordForm CreatePending(StringPiece username, StringPiece password) {
  PasswordForm form = CreateObserved();
  form.username_value = ASCIIToUTF16(username);
  form.password_value = ASCIIToUTF16(password);
  form.preferred = true;
  return form;
}

}  // namespace

class FormSaverImplTest : public testing::Test {
 public:
  FormSaverImplTest()
      : mock_store_(new StrictMock<MockPasswordStore>()),
        form_saver_(mock_store_.get()) {}

  ~FormSaverImplTest() override { mock_store_->ShutdownOnUIThread(); }

 protected:
  base::MessageLoop message_loop_;  // For the MockPasswordStore.
  scoped_refptr<StrictMock<MockPasswordStore>> mock_store_;
  FormSaverImpl form_saver_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FormSaverImplTest);
};

// Check that blacklisting an observed form sets the right properties and calls
// the PasswordStore.
TEST_F(FormSaverImplTest, PermanentlyBlacklist) {
  PasswordForm observed = CreateObserved();
  PasswordForm saved;

  observed.blacklisted_by_user = false;
  observed.preferred = true;
  observed.username_value = ASCIIToUTF16("user1");
  observed.password_value = ASCIIToUTF16("12345");
  observed.other_possible_usernames = {
      {ASCIIToUTF16("user2"), ASCIIToUTF16("field")}};

  EXPECT_CALL(*mock_store_, AddLogin(_)).WillOnce(SaveArg<0>(&saved));
  form_saver_.PermanentlyBlacklist(&observed);
  EXPECT_TRUE(saved.blacklisted_by_user);
  EXPECT_FALSE(saved.preferred);
  EXPECT_EQ(base::string16(), saved.username_value);
  EXPECT_EQ(base::string16(), saved.password_value);
  EXPECT_TRUE(saved.other_possible_usernames.empty());
}

// Check that saving the pending form as new adds the credential to the store
// (rather than updating).
TEST_F(FormSaverImplTest, Save_AsNew) {
  PasswordForm pending = CreatePending("nameofuser", "wordToP4a55");
  PasswordForm saved;

  EXPECT_CALL(*mock_store_, AddLogin(_)).WillOnce(SaveArg<0>(&saved));
  EXPECT_CALL(*mock_store_, UpdateLogin(_)).Times(0);
  EXPECT_CALL(*mock_store_, UpdateLoginWithPrimaryKey(_, _)).Times(0);
  form_saver_.Save(pending, std::map<base::string16, const PasswordForm*>());
  EXPECT_EQ(ASCIIToUTF16("nameofuser"), saved.username_value);
  EXPECT_EQ(ASCIIToUTF16("wordToP4a55"), saved.password_value);
}

// Check that saving the pending form as not new updates the store with the
// credential.
TEST_F(FormSaverImplTest, Save_Update) {
  PasswordForm pending = CreatePending("nameofuser", "wordToP4a55");
  PasswordForm saved;

  EXPECT_CALL(*mock_store_, AddLogin(_)).Times(0);
  EXPECT_CALL(*mock_store_, UpdateLogin(_)).WillOnce(SaveArg<0>(&saved));
  EXPECT_CALL(*mock_store_, UpdateLoginWithPrimaryKey(_, _)).Times(0);
  form_saver_.Update(pending, std::map<base::string16, const PasswordForm*>(),
                     nullptr, nullptr);
  EXPECT_EQ(ASCIIToUTF16("nameofuser"), saved.username_value);
  EXPECT_EQ(ASCIIToUTF16("wordToP4a55"), saved.password_value);
}

// Check that passing other credentials to update to the Save call results in
// the store being updated with those credentials in addition to the pending
// one.
TEST_F(FormSaverImplTest, Save_UpdateAlsoOtherCredentials) {
  PasswordForm pending = CreatePending("nameofuser", "wordToP4a55");
  PasswordForm related1 = pending;
  related1.origin = GURL("https://other.example.ca");
  related1.signon_realm = related1.origin.spec();
  PasswordForm related2 = pending;
  related2.origin = GURL("http://complete.example.net");
  related2.signon_realm = related2.origin.spec();
  std::vector<PasswordForm> credentials_to_update = {related1, related2};
  pending.password_value = ASCIIToUTF16("abcd");

  PasswordForm saved[3];

  EXPECT_CALL(*mock_store_, AddLogin(_)).Times(0);
  EXPECT_CALL(*mock_store_, UpdateLogin(_))
      .WillOnce(SaveArg<0>(&saved[0]))
      .WillOnce(SaveArg<0>(&saved[1]))
      .WillOnce(SaveArg<0>(&saved[2]));
  EXPECT_CALL(*mock_store_, UpdateLoginWithPrimaryKey(_, _)).Times(0);
  form_saver_.Update(pending, std::map<base::string16, const PasswordForm*>(),
                     &credentials_to_update, nullptr);
  std::set<GURL> different_origins;
  for (const PasswordForm& form : saved) {
    different_origins.insert(form.origin);
  }
  EXPECT_THAT(different_origins,
              testing::UnorderedElementsAre(pending.origin, related1.origin,
                                            related2.origin));
}

// Check that if the old primary key is supplied, the appropriate store method
// for update is used.
TEST_F(FormSaverImplTest, Save_UpdateWithPrimaryKey) {
  PasswordForm pending = CreatePending("nameofuser", "wordToP4a55");
  PasswordForm old_key = pending;
  old_key.username_value = ASCIIToUTF16("old username");
  PasswordForm saved_new;
  PasswordForm saved_old;

  EXPECT_CALL(*mock_store_, AddLogin(_)).Times(0);
  EXPECT_CALL(*mock_store_, UpdateLogin(_)).Times(0);
  EXPECT_CALL(*mock_store_, UpdateLoginWithPrimaryKey(_, _))
      .WillOnce(DoAll(SaveArg<0>(&saved_new), SaveArg<1>(&saved_old)));
  form_saver_.Update(pending, std::map<base::string16, const PasswordForm*>(),
                     nullptr, &old_key);
  EXPECT_EQ(ASCIIToUTF16("nameofuser"), saved_new.username_value);
  EXPECT_EQ(ASCIIToUTF16("wordToP4a55"), saved_new.password_value);
  EXPECT_EQ(ASCIIToUTF16("old username"), saved_old.username_value);
}

// Check that the "preferred" bit of best matches is updated accordingly in the
// store.
TEST_F(FormSaverImplTest, Save_AndUpdatePreferredLoginState) {
  PasswordForm pending = CreatePending("nameofuser", "wordToP4a55");
  pending.preferred = true;

  // |best_matches| will contain two forms: one non-PSL matched with a username
  // different from the pending one, and one PSL-matched with a username same
  // as the pending one, both marked as "preferred". FormSaver should ignore
  // the pending and PSL-matched one, but should update the non-PSL matched
  // form (with different username) to no longer be preferred.
  std::map<base::string16, const PasswordForm*> best_matches;
  PasswordForm other = pending;
  other.username_value = ASCIIToUTF16("othername");
  best_matches[other.username_value] = &other;
  PasswordForm psl_match = pending;
  psl_match.is_public_suffix_match = true;
  best_matches[psl_match.username_value] = &psl_match;

  PasswordForm saved;
  PasswordForm updated;

  EXPECT_CALL(*mock_store_, AddLogin(_)).WillOnce(SaveArg<0>(&saved));
  EXPECT_CALL(*mock_store_, UpdateLogin(_)).WillOnce(SaveArg<0>(&updated));
  EXPECT_CALL(*mock_store_, UpdateLoginWithPrimaryKey(_, _)).Times(0);
  form_saver_.Save(pending, best_matches);
  EXPECT_EQ(ASCIIToUTF16("nameofuser"), saved.username_value);
  EXPECT_EQ(ASCIIToUTF16("wordToP4a55"), saved.password_value);
  EXPECT_TRUE(saved.preferred);
  EXPECT_FALSE(saved.is_public_suffix_match);
  EXPECT_EQ(ASCIIToUTF16("othername"), updated.username_value);
  EXPECT_EQ(ASCIIToUTF16("wordToP4a55"), updated.password_value);
  EXPECT_FALSE(updated.preferred);
  EXPECT_FALSE(updated.is_public_suffix_match);
}

// Check that storing credentials with a non-empty username results in deleting
// credentials with the same password but no username, if present in best
// matches.
TEST_F(FormSaverImplTest, Save_AndDeleteEmptyUsernameCredentials) {
  PasswordForm pending = CreatePending("nameofuser", "wordToP4a55");

  std::map<base::string16, const PasswordForm*> best_matches;
  best_matches[pending.username_value] = &pending;
  PasswordForm no_username = pending;
  no_username.username_value.clear();
  no_username.preferred = false;
  best_matches[no_username.username_value] = &no_username;

  PasswordForm saved;
  PasswordForm removed;

  EXPECT_CALL(*mock_store_, AddLogin(_)).WillOnce(SaveArg<0>(&saved));
  EXPECT_CALL(*mock_store_, UpdateLogin(_)).Times(0);
  EXPECT_CALL(*mock_store_, UpdateLoginWithPrimaryKey(_, _)).Times(0);
  EXPECT_CALL(*mock_store_, RemoveLogin(_)).WillOnce(SaveArg<0>(&removed));
  form_saver_.Save(pending, best_matches);
  EXPECT_EQ(ASCIIToUTF16("nameofuser"), saved.username_value);
  EXPECT_EQ(ASCIIToUTF16("wordToP4a55"), saved.password_value);
  EXPECT_TRUE(removed.username_value.empty());
  EXPECT_EQ(ASCIIToUTF16("wordToP4a55"), removed.password_value);
}

// Check that storing credentials with a non-empty username does not result in
// deleting credentials with a different password, even if they have no
// username.
TEST_F(FormSaverImplTest,
       Save_AndDoNotDeleteEmptyUsernameCredentialsWithDifferentPassword) {
  PasswordForm pending = CreatePending("nameofuser", "wordToP4a55");

  std::map<base::string16, const PasswordForm*> best_matches;
  best_matches[pending.username_value] = &pending;
  PasswordForm no_username = pending;
  no_username.username_value.clear();
  no_username.preferred = false;
  no_username.password_value = ASCIIToUTF16("abcd");
  best_matches[no_username.username_value] = &no_username;

  PasswordForm saved;

  EXPECT_CALL(*mock_store_, AddLogin(_)).WillOnce(SaveArg<0>(&saved));
  EXPECT_CALL(*mock_store_, UpdateLogin(_)).Times(0);
  EXPECT_CALL(*mock_store_, UpdateLoginWithPrimaryKey(_, _)).Times(0);
  EXPECT_CALL(*mock_store_, RemoveLogin(_)).Times(0);
  form_saver_.Save(pending, best_matches);
  EXPECT_EQ(ASCIIToUTF16("nameofuser"), saved.username_value);
  EXPECT_EQ(ASCIIToUTF16("wordToP4a55"), saved.password_value);
}

// Check that if both "abc"/"pwd" and ""/"pwd" are both stored, and "abc"/"pwd"
// is updated to "abc"/"def", then ""/"pwd" is not deleted.
TEST_F(FormSaverImplTest,
       Save_DoNotDeleteUsernamelessOnUpdatingPasswordWithUsername) {
  PasswordForm pending = CreatePending("abc", "pwd");

  std::map<base::string16, const PasswordForm*> best_matches;
  best_matches[pending.username_value] = &pending;
  PasswordForm no_username = pending;
  no_username.username_value.clear();
  no_username.preferred = false;
  best_matches[no_username.username_value] = &no_username;

  pending.password_value = ASCIIToUTF16("def");

  PasswordForm saved;

  EXPECT_CALL(*mock_store_, AddLogin(_)).WillOnce(SaveArg<0>(&saved));
  EXPECT_CALL(*mock_store_, UpdateLogin(_)).Times(0);
  EXPECT_CALL(*mock_store_, UpdateLoginWithPrimaryKey(_, _)).Times(0);
  EXPECT_CALL(*mock_store_, RemoveLogin(_)).Times(0);
  form_saver_.Save(pending, best_matches);
  EXPECT_EQ(ASCIIToUTF16("abc"), saved.username_value);
  EXPECT_EQ(ASCIIToUTF16("def"), saved.password_value);
}

// Check that if a credential without username is saved, and another credential
// with the same password (and a non-empty username) is present in best matches,
// nothing is deleted.
TEST_F(FormSaverImplTest, Save_EmptyUsernameWillNotCauseDeletion) {
  PasswordForm pending = CreatePending("", "wordToP4a55");

  std::map<base::string16, const PasswordForm*> best_matches;
  PasswordForm with_username = pending;
  with_username.username_value = ASCIIToUTF16("nameofuser");
  with_username.preferred = false;
  best_matches[with_username.username_value] = &with_username;

  PasswordForm saved;

  EXPECT_CALL(*mock_store_, AddLogin(_)).WillOnce(SaveArg<0>(&saved));
  EXPECT_CALL(*mock_store_, UpdateLogin(_)).Times(0);
  EXPECT_CALL(*mock_store_, UpdateLoginWithPrimaryKey(_, _)).Times(0);
  EXPECT_CALL(*mock_store_, RemoveLogin(_)).Times(0);
  form_saver_.Save(pending, best_matches);
  EXPECT_TRUE(saved.username_value.empty());
  EXPECT_EQ(ASCIIToUTF16("wordToP4a55"), saved.password_value);
}

// Check that PSL-matched credentials in best matches are exempt from deletion,
// even if they have an empty username and the same password as the pending
// credential.
TEST_F(FormSaverImplTest, Save_AndDoNotDeleteEmptyUsernamePSLCredentials) {
  PasswordForm pending = CreatePending("nameofuser", "wordToP4a55");

  std::map<base::string16, const PasswordForm*> best_matches;
  best_matches[pending.username_value] = &pending;
  PasswordForm no_username_psl = pending;
  no_username_psl.username_value.clear();
  no_username_psl.is_public_suffix_match = true;
  best_matches[no_username_psl.username_value] = &no_username_psl;

  PasswordForm saved;

  EXPECT_CALL(*mock_store_, AddLogin(_)).WillOnce(SaveArg<0>(&saved));
  EXPECT_CALL(*mock_store_, UpdateLogin(_)).Times(0);
  EXPECT_CALL(*mock_store_, UpdateLoginWithPrimaryKey(_, _)).Times(0);
  EXPECT_CALL(*mock_store_, RemoveLogin(_)).Times(0);
  form_saver_.Save(pending, best_matches);
  EXPECT_EQ(ASCIIToUTF16("nameofuser"), saved.username_value);
  EXPECT_EQ(ASCIIToUTF16("wordToP4a55"), saved.password_value);
}

// Check that on storing a credential, other credentials with the same password
// are not removed, as long as they have a non-empty username.
TEST_F(FormSaverImplTest, Save_AndDoNotDeleteNonEmptyUsernameCredentials) {
  PasswordForm pending = CreatePending("nameofuser", "wordToP4a55");

  std::map<base::string16, const PasswordForm*> best_matches;
  best_matches[pending.username_value] = &pending;
  PasswordForm other_username = pending;
  other_username.username_value = ASCIIToUTF16("other username");
  other_username.preferred = false;
  best_matches[other_username.username_value] = &other_username;

  PasswordForm saved;

  EXPECT_CALL(*mock_store_, AddLogin(_)).WillOnce(SaveArg<0>(&saved));
  EXPECT_CALL(*mock_store_, UpdateLogin(_)).Times(0);
  EXPECT_CALL(*mock_store_, UpdateLoginWithPrimaryKey(_, _)).Times(0);
  EXPECT_CALL(*mock_store_, RemoveLogin(_)).Times(0);
  form_saver_.Save(pending, best_matches);
  EXPECT_EQ(ASCIIToUTF16("nameofuser"), saved.username_value);
  EXPECT_EQ(ASCIIToUTF16("wordToP4a55"), saved.password_value);
}

// Check that presaving a password for the first time results in adding it.
TEST_F(FormSaverImplTest, PresaveGeneratedPassword_New) {
  PasswordForm generated = CreatePending("nameofuser", "wordToP4a55");
  PasswordForm saved;

  EXPECT_CALL(*mock_store_, AddLogin(_)).WillOnce(SaveArg<0>(&saved));
  EXPECT_CALL(*mock_store_, UpdateLogin(_)).Times(0);
  EXPECT_CALL(*mock_store_, UpdateLoginWithPrimaryKey(_, _)).Times(0);
  form_saver_.PresaveGeneratedPassword(generated);
  EXPECT_EQ(ASCIIToUTF16("nameofuser"), saved.username_value);
  EXPECT_EQ(ASCIIToUTF16("wordToP4a55"), saved.password_value);
}

// Check that presaving a password for the second time results in updating it.
TEST_F(FormSaverImplTest, PresaveGeneratedPassword_Replace) {
  PasswordForm generated = CreatePending("nameofuser", "wordToP4a55");

  EXPECT_CALL(*mock_store_, AddLogin(_));
  form_saver_.PresaveGeneratedPassword(generated);

  generated.password_value = ASCIIToUTF16("newgenpwd");
  PasswordForm saved_new;
  PasswordForm saved_old;
  EXPECT_CALL(*mock_store_, AddLogin(_)).Times(0);
  EXPECT_CALL(*mock_store_, UpdateLogin(_)).Times(0);
  EXPECT_CALL(*mock_store_, UpdateLoginWithPrimaryKey(_, _))
      .WillOnce(DoAll(SaveArg<0>(&saved_new), SaveArg<1>(&saved_old)));
  form_saver_.PresaveGeneratedPassword(generated);
  EXPECT_EQ(ASCIIToUTF16("nameofuser"), saved_old.username_value);
  EXPECT_EQ(ASCIIToUTF16("wordToP4a55"), saved_old.password_value);
  EXPECT_EQ(ASCIIToUTF16("nameofuser"), saved_new.username_value);
  EXPECT_EQ(ASCIIToUTF16("newgenpwd"), saved_new.password_value);
}

// Check that presaving a password followed by a call to save a pending
// credential (as new) results in replacing the presaved password with the
// pending one.
TEST_F(FormSaverImplTest, PresaveGeneratedPassword_ThenSaveAsNew) {
  PasswordForm generated = CreatePending("generatedU", "generatedP");

  EXPECT_CALL(*mock_store_, AddLogin(_));
  form_saver_.PresaveGeneratedPassword(generated);

  PasswordForm pending = CreatePending("nameofuser", "wordToP4a55");
  PasswordForm saved_new;
  PasswordForm saved_old;
  EXPECT_CALL(*mock_store_, AddLogin(_)).Times(0);
  EXPECT_CALL(*mock_store_, UpdateLogin(_)).Times(0);
  EXPECT_CALL(*mock_store_, UpdateLoginWithPrimaryKey(_, _))
      .WillOnce(DoAll(SaveArg<0>(&saved_new), SaveArg<1>(&saved_old)));
  form_saver_.Save(pending, std::map<base::string16, const PasswordForm*>());
  EXPECT_EQ(ASCIIToUTF16("generatedU"), saved_old.username_value);
  EXPECT_EQ(ASCIIToUTF16("generatedP"), saved_old.password_value);
  EXPECT_EQ(ASCIIToUTF16("nameofuser"), saved_new.username_value);
  EXPECT_EQ(ASCIIToUTF16("wordToP4a55"), saved_new.password_value);
}

// Check that presaving a password followed by a call to save a pending
// credential (as update) results in replacing the presaved password with the
// pending one.
TEST_F(FormSaverImplTest, PresaveGeneratedPassword_ThenUpdate) {
  PasswordForm generated = CreatePending("generatedU", "generatedP");

  EXPECT_CALL(*mock_store_, AddLogin(_));
  form_saver_.PresaveGeneratedPassword(generated);

  PasswordForm pending = CreatePending("nameofuser", "wordToP4a55");
  PasswordForm saved_new;
  PasswordForm saved_old;
  EXPECT_CALL(*mock_store_, AddLogin(_)).Times(0);
  EXPECT_CALL(*mock_store_, UpdateLogin(_)).Times(0);
  EXPECT_CALL(*mock_store_, UpdateLoginWithPrimaryKey(_, _))
      .WillOnce(DoAll(SaveArg<0>(&saved_new), SaveArg<1>(&saved_old)));
  form_saver_.Update(pending, std::map<base::string16, const PasswordForm*>(),
                     nullptr, nullptr);
  EXPECT_EQ(ASCIIToUTF16("generatedU"), saved_old.username_value);
  EXPECT_EQ(ASCIIToUTF16("generatedP"), saved_old.password_value);
  EXPECT_EQ(ASCIIToUTF16("nameofuser"), saved_new.username_value);
  EXPECT_EQ(ASCIIToUTF16("wordToP4a55"), saved_new.password_value);
}

// Check that presaving a password for the third time results in updating it.
TEST_F(FormSaverImplTest, PresaveGeneratedPassword_ReplaceTwice) {
  PasswordForm generated = CreatePending("nameofuser", "wordToP4a55");

  EXPECT_CALL(*mock_store_, AddLogin(_));
  form_saver_.PresaveGeneratedPassword(generated);
  EXPECT_CALL(*mock_store_, UpdateLoginWithPrimaryKey(_, _));
  form_saver_.PresaveGeneratedPassword(generated);

  generated.password_value = ASCIIToUTF16("newgenpwd");
  PasswordForm saved_new;
  PasswordForm saved_old;
  EXPECT_CALL(*mock_store_, AddLogin(_)).Times(0);
  EXPECT_CALL(*mock_store_, UpdateLogin(_)).Times(0);
  EXPECT_CALL(*mock_store_, UpdateLoginWithPrimaryKey(_, _))
      .WillOnce(DoAll(SaveArg<0>(&saved_new), SaveArg<1>(&saved_old)));
  form_saver_.PresaveGeneratedPassword(generated);
  EXPECT_EQ(ASCIIToUTF16("nameofuser"), saved_old.username_value);
  EXPECT_EQ(ASCIIToUTF16("wordToP4a55"), saved_old.password_value);
  EXPECT_EQ(ASCIIToUTF16("nameofuser"), saved_new.username_value);
  EXPECT_EQ(ASCIIToUTF16("newgenpwd"), saved_new.password_value);
}

// Check that removing a presaved password is a no-op if none was presaved.
TEST_F(FormSaverImplTest, RemovePresavedPassword_NonePresaved) {
  EXPECT_CALL(*mock_store_, RemoveLogin(_)).Times(0);
  form_saver_.RemovePresavedPassword();
}

// Check that removing a presaved password removes the presaved password.
TEST_F(FormSaverImplTest, RemovePresavedPassword) {
  PasswordForm generated = CreatePending("nameofuser", "wordToP4a55");

  EXPECT_CALL(*mock_store_, AddLogin(_));
  form_saver_.PresaveGeneratedPassword(generated);

  PasswordForm removed;
  EXPECT_CALL(*mock_store_, RemoveLogin(_)).WillOnce(SaveArg<0>(&removed));
  form_saver_.RemovePresavedPassword();
  EXPECT_EQ(ASCIIToUTF16("nameofuser"), removed.username_value);
  EXPECT_EQ(ASCIIToUTF16("wordToP4a55"), removed.password_value);
}

// Check that removing the presaved password and then presaving again results in
// adding the second presaved password as new.
TEST_F(FormSaverImplTest, RemovePresavedPassword_AndPresaveAgain) {
  PasswordForm generated = CreatePending("nameofuser", "wordToP4a55");

  EXPECT_CALL(*mock_store_, AddLogin(_));
  form_saver_.PresaveGeneratedPassword(generated);

  EXPECT_CALL(*mock_store_, RemoveLogin(_));
  form_saver_.RemovePresavedPassword();

  PasswordForm saved;
  generated.username_value = ASCIIToUTF16("newgen");
  generated.password_value = ASCIIToUTF16("newgenpwd");
  EXPECT_CALL(*mock_store_, AddLogin(_)).WillOnce(SaveArg<0>(&saved));
  EXPECT_CALL(*mock_store_, UpdateLogin(_)).Times(0);
  EXPECT_CALL(*mock_store_, UpdateLoginWithPrimaryKey(_, _)).Times(0);
  form_saver_.PresaveGeneratedPassword(generated);
  EXPECT_EQ(ASCIIToUTF16("newgen"), saved.username_value);
  EXPECT_EQ(ASCIIToUTF16("newgenpwd"), saved.password_value);
}

// Check that presaving a password once in original and then once in clone
// results in the clone calling update, not a fresh save.
TEST_F(FormSaverImplTest, PresaveGeneratedPassword_CloneUpdates) {
  PasswordForm generated = CreatePending("nameofuser", "wordToP4a55");

  EXPECT_CALL(*mock_store_, AddLogin(_));
  form_saver_.PresaveGeneratedPassword(generated);
  std::unique_ptr<FormSaver> clone = form_saver_.Clone();
  EXPECT_CALL(*mock_store_, UpdateLoginWithPrimaryKey(_, _));
  clone->PresaveGeneratedPassword(generated);
}

// Check that a clone can still work after the original is destroyed.
TEST_F(FormSaverImplTest, PresaveGeneratedPassword_CloneSurvives) {
  auto original = std::make_unique<FormSaverImpl>(mock_store_.get());
  PasswordForm generated = CreatePending("nameofuser", "wordToP4a55");

  EXPECT_CALL(*mock_store_, AddLogin(_));
  original->PresaveGeneratedPassword(generated);
  std::unique_ptr<FormSaver> clone = original->Clone();
  original.reset();
  EXPECT_CALL(*mock_store_, UpdateLoginWithPrimaryKey(_, _));
  clone->PresaveGeneratedPassword(generated);
}

// Check that on saving the pending form |form_data| is sanitized.
TEST_F(FormSaverImplTest, FormDataSanitized) {
  PasswordForm pending = CreatePending("nameofuser", "wordToP4a55");
  FormFieldData field;
  field.name = ASCIIToUTF16("name");
  field.form_control_type = "password";
  field.value = ASCIIToUTF16("value");
  field.label = ASCIIToUTF16("label");
  field.placeholder = ASCIIToUTF16("placeholder");
  field.id = ASCIIToUTF16("id");
  field.css_classes = ASCIIToUTF16("css_classes");
  pending.form_data.fields.push_back(field);

  for (bool presave : {false, true}) {
    PasswordForm saved;
    EXPECT_CALL(*mock_store_, AddLogin(_)).WillOnce(SaveArg<0>(&saved));
    if (presave)
      form_saver_.PresaveGeneratedPassword(pending);
    else
      form_saver_.Save(pending, {});

    ASSERT_EQ(1u, saved.form_data.fields.size());
    const FormFieldData& saved_field = saved.form_data.fields[0];
    EXPECT_EQ(ASCIIToUTF16("name"), saved_field.name);
    EXPECT_EQ("password", saved_field.form_control_type);
    EXPECT_TRUE(saved_field.value.empty());
    EXPECT_TRUE(saved_field.label.empty());
    EXPECT_TRUE(saved_field.placeholder.empty());
    EXPECT_TRUE(saved_field.id.empty());
    EXPECT_TRUE(saved_field.css_classes.empty());
  }
}

}  // namespace password_manager
