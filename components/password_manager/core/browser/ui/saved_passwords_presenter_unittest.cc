// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"

#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
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
  SavedPasswordsPresenterTest() {
    store_->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);
  }

  ~SavedPasswordsPresenterTest() override {
    store_->ShutdownOnUIThread();
    task_env_.RunUntilIdle();
  }

  TestPasswordStore& store() { return *store_; }
  SavedPasswordsPresenter& presenter() { return presenter_; }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }

 private:
  base::test::SingleThreadTaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<TestPasswordStore> store_ =
      base::MakeRefCounted<TestPasswordStore>();
  SavedPasswordsPresenter presenter_{store_};
};

password_manager::PasswordForm CreateTestPasswordForm(
    password_manager::PasswordForm::Store store,
    int index = 0) {
  PasswordForm form;
  form.url = GURL("https://test" + base::NumberToString(index) + ".com");
  form.signon_realm = form.url.spec();
  form.username_value = u"username" + base::NumberToString16(index);
  form.password_value = u"password" + base::NumberToString16(index);
  form.in_store = store;
  return form;
}

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

TEST_F(SavedPasswordsPresenterTest, AddPasswordFailWhenInvalidUrl) {
  StrictMockSavedPasswordsPresenterObserver observer;
  presenter().AddObserver(&observer);

  PasswordForm form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  form.url = GURL("https://;/invalid");

  base::HistogramTester histogram_tester;
  EXPECT_CALL(observer, OnSavedPasswordsChanged).Times(0);
  EXPECT_FALSE(presenter().AddPassword(form));
  RunUntilIdle();
  EXPECT_TRUE(store().IsEmpty());

  form.url = GURL("withoutscheme.com");
  EXPECT_CALL(observer, OnSavedPasswordsChanged).Times(0);
  EXPECT_FALSE(presenter().AddPassword(form));
  RunUntilIdle();
  EXPECT_TRUE(store().IsEmpty());
  histogram_tester.ExpectTotalCount(
      "PasswordManager.PasswordNoteActionInSettings", 0);

  presenter().RemoveObserver(&observer);
}

TEST_F(SavedPasswordsPresenterTest, AddPasswordFailWhenEmptyPassword) {
  StrictMockSavedPasswordsPresenterObserver observer;
  presenter().AddObserver(&observer);

  PasswordForm form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  form.password_value = u"";

  base::HistogramTester histogram_tester;
  EXPECT_CALL(observer, OnSavedPasswordsChanged).Times(0);
  EXPECT_FALSE(presenter().AddPassword(form));
  RunUntilIdle();
  EXPECT_TRUE(store().IsEmpty());
  histogram_tester.ExpectTotalCount(
      "PasswordManager.PasswordNoteActionInSettings", 0);

  presenter().RemoveObserver(&observer);
}

TEST_F(SavedPasswordsPresenterTest, AddPasswordUnblocklistsOrigin) {
  PasswordForm form_to_add =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  form_to_add.type = password_manager::PasswordForm::Type::kManuallyAdded;
  form_to_add.date_created = base::Time::Now();
  form_to_add.date_password_modified = base::Time::Now();

  PasswordForm blocked_form;
  blocked_form.blocked_by_user = true;
  blocked_form.signon_realm = form_to_add.signon_realm;
  blocked_form.in_store = PasswordForm::Store::kProfileStore;
  // Blocklist some origin.
  store().AddLogin(blocked_form);
  RunUntilIdle();
  ASSERT_THAT(presenter().GetUniquePasswordForms(), ElementsAre(blocked_form));

  // Add a new entry with the same origin.
  EXPECT_TRUE(presenter().AddPassword(form_to_add));
  RunUntilIdle();

  // The entry should be added despite the origin was blocklisted.
  EXPECT_THAT(
      store().stored_passwords(),
      ElementsAre(Pair(form_to_add.signon_realm, ElementsAre(form_to_add))));
  // The origin should be no longer blocklisted.
  EXPECT_THAT(presenter().GetUniquePasswordForms(), ElementsAre(form_to_add));
}

// Tests whether editing a password works and results in the right
// notifications.
TEST_F(SavedPasswordsPresenterTest, EditPassword) {
  PasswordForm form;
  // Make sure the form has some issues and expect that they are cleared
  // because of the password change.
  form.password_issues = {
      {InsecureType::kLeaked,
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false))}};

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
  // The expected updated form should have a new password and no password
  // issues.
  PasswordForm updated = form;
  updated.password_value = new_password;
  updated.date_password_modified = base::Time::Now();
  updated.password_issues.clear();

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
  PasswordForm form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  // Make sure the form has some issues and expect that they are cleared
  // because of the username change.
  form.password_issues = {
      {InsecureType::kLeaked,
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false))}};

  StrictMockSavedPasswordsPresenterObserver observer;
  presenter().AddObserver(&observer);

  EXPECT_CALL(observer, OnSavedPasswordsChanged);
  store().AddLogin(form);
  RunUntilIdle();
  EXPECT_FALSE(store().IsEmpty());

  std::vector<PasswordForm> forms = {form};

  const std::u16string new_username = u"new_username";
  // The result of the update should have a new username and no password
  // issues.
  PasswordForm updated_username = form;
  updated_username.username_value = new_username;
  updated_username.password_issues.clear();

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

TEST_F(SavedPasswordsPresenterTest, EditOnlyUsernameClearsPartialIssues) {
  PasswordForm form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  // Make sure the form has some issues and expect that only phished and leaked
  // are cleared because of the username change.
  form.password_issues = {
      {InsecureType::kLeaked,
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false))},
      {InsecureType::kPhished,
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false))},
      {InsecureType::kReused,
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false))},
      {InsecureType::kWeak,
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false))}};

  StrictMockSavedPasswordsPresenterObserver observer;
  presenter().AddObserver(&observer);

  EXPECT_CALL(observer, OnSavedPasswordsChanged);
  store().AddLogin(form);
  RunUntilIdle();
  EXPECT_FALSE(store().IsEmpty());

  std::vector<PasswordForm> forms = {form};

  const std::u16string new_username = u"new_username";
  // The result of the update should have a new username and weak and reused
  // password issues.
  PasswordForm updated_username = form;
  updated_username.username_value = new_username;
  updated_username.password_issues = {
      {InsecureType::kReused,
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false))},
      {InsecureType::kWeak,
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false))}};

  EXPECT_CALL(observer, OnEdited(updated_username));
  EXPECT_CALL(observer, OnSavedPasswordsChanged(ElementsAre(updated_username)));
  EXPECT_TRUE(
      presenter().EditSavedPasswords(forms, new_username, form.password_value));
  RunUntilIdle();
  EXPECT_THAT(
      store().stored_passwords(),
      ElementsAre(Pair(form.signon_realm, ElementsAre(updated_username))));

  presenter().RemoveObserver(&observer);
}

TEST_F(SavedPasswordsPresenterTest, EditOnlyPassword) {
  PasswordForm form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  // Make sure the form has some issues and expect that they are cleared
  // because of the password change.
  form.password_issues = {
      {InsecureType::kLeaked,
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false))}};

  StrictMockSavedPasswordsPresenterObserver observer;
  presenter().AddObserver(&observer);

  EXPECT_CALL(observer, OnSavedPasswordsChanged);
  store().AddLogin(form);
  RunUntilIdle();
  EXPECT_FALSE(store().IsEmpty());

  std::vector<PasswordForm> forms = {form};

  const std::u16string new_password = u"new_password";
  PasswordForm updated_password = form;
  // The result of the update should have a new password and no password
  // issues.
  updated_password.password_value = new_password;
  updated_password.date_password_modified = base::Time::Now();
  updated_password.password_issues.clear();

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
  histogram_tester.ExpectTotalCount(
      "PasswordManager.PasswordNoteActionInSettings", 0);

  presenter().RemoveObserver(&observer);
}

TEST_F(SavedPasswordsPresenterTest, EditOnlyNoteFirstTime) {
  PasswordForm form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  form.notes.emplace_back(u"display name", u"note with non-empty display name",
                          /*date_created=*/base::Time::Now(),
                          /*hide_by_default=*/true);

  store().AddLogin(form);
  RunUntilIdle();
  std::vector<PasswordForm> forms = {form};

  const std::u16string kNewNoteValue = u"new note";

  base::HistogramTester histogram_tester;
  EXPECT_TRUE(presenter().EditSavedPasswords(
      forms, form.username_value, form.password_value, kNewNoteValue));
  RunUntilIdle();

  // The note with the non-empty display name should be untouched. Another note
  // with an empty display name should be added.
  PasswordForm expected_updated_form = form;
  expected_updated_form.notes.emplace_back(kNewNoteValue,
                                           /*date_created=*/base::Time::Now());
  EXPECT_THAT(
      store().stored_passwords(),
      ElementsAre(Pair(form.signon_realm, ElementsAre(expected_updated_form))));
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordNoteActionInSettings",
      metrics_util::PasswordNoteAction::kNoteAddedInEditDialog, 1);
}

TEST_F(SavedPasswordsPresenterTest, EditingNotesShouldNotResetPasswordIssues) {
  PasswordForm form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);

  form.password_issues.insert(
      {InsecureType::kLeaked,
       InsecurityMetadata(base::Time(), IsMuted(false))});

  store().AddLogin(form);
  RunUntilIdle();
  std::vector<PasswordForm> forms = {form};

  const std::u16string kNewNoteValue = u"new note";

  EXPECT_TRUE(presenter().EditSavedPasswords(
      forms, form.username_value, form.password_value, kNewNoteValue));
  RunUntilIdle();

  PasswordForm expected_updated_form = form;
  expected_updated_form.notes = {
      PasswordNote(kNewNoteValue, base::Time::Now())};
  EXPECT_THAT(
      store().stored_passwords(),
      ElementsAre(Pair(form.signon_realm, ElementsAre(expected_updated_form))));
}

TEST_F(SavedPasswordsPresenterTest, EditOnlyNoteSecondTime) {
  PasswordNote kExistingNote =
      PasswordNote(u"existing note", base::Time::Now());
  PasswordForm form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  form.notes = {kExistingNote};

  store().AddLogin(form);
  RunUntilIdle();
  std::vector<PasswordForm> forms = {form};

  const std::u16string kNewNoteValue = u"new note";

  base::HistogramTester histogram_tester;
  EXPECT_TRUE(presenter().EditSavedPasswords(
      forms, form.username_value, form.password_value, kNewNoteValue));
  RunUntilIdle();

  PasswordForm expected_updated_form = form;
  expected_updated_form.notes[0].value = kNewNoteValue;
  EXPECT_THAT(
      store().stored_passwords(),
      ElementsAre(Pair(form.signon_realm, ElementsAre(expected_updated_form))));
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordNoteActionInSettings",
      metrics_util::PasswordNoteAction::kNoteEditedInEditDialog, 1);
}

TEST_F(SavedPasswordsPresenterTest, EditNoteAsEmpty) {
  PasswordForm form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  form.notes = {PasswordNote(u"existing note", base::Time::Now())};
  std::vector<PasswordForm> forms = {form};

  store().AddLogin(form);
  RunUntilIdle();

  base::HistogramTester histogram_tester;
  EXPECT_TRUE(presenter().EditSavedPasswords(forms, form.username_value,
                                             form.password_value, u""));
  RunUntilIdle();

  PasswordForm expected_updated_form = form;
  expected_updated_form.notes[0].value = u"";
  EXPECT_THAT(
      store().stored_passwords(),
      ElementsAre(Pair(form.signon_realm, ElementsAre(expected_updated_form))));
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordNoteActionInSettings",
      metrics_util::PasswordNoteAction::kNoteRemovedInEditDialog, 1);
}

TEST_F(SavedPasswordsPresenterTest,
       GetSavedCredentialsReturnNotesWithEmptyDisplayName) {
  // Create form with two notes, first is with a non-empty display name, and the
  // second with an empty one.
  PasswordNote kNoteWithEmptyDisplayName =
      PasswordNote(u"note with empty display name",
                   /*date_created=*/base::Time::Now());
  PasswordForm form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  form.notes.emplace_back(u"display name", u"note with non-empty display name",
                          /*date_created=*/base::Time::Now(),
                          /*hide_by_default=*/true);
  form.notes.push_back(kNoteWithEmptyDisplayName);

  store().AddLogin(form);
  RunUntilIdle();

  // The expect credential UI entry should contain only the note with that empty
  // display name.
  std::vector<CredentialUIEntry> saved_credentials =
      presenter().GetSavedCredentials();
  ASSERT_EQ(1U, saved_credentials.size());
  EXPECT_EQ(kNoteWithEmptyDisplayName, saved_credentials[0].note);
}

TEST_F(SavedPasswordsPresenterTest, EditUsernameAndPassword) {
  PasswordForm form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  // Make sure the form has some issues and expect that they are cleared
  // because of the username and password change.
  form.password_issues = {
      {InsecureType::kLeaked,
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false))}};

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
  // The result of the update should have a new username and password and no
  // password issues.
  updated_both.username_value = new_username;
  updated_both.password_value = new_password;
  updated_both.date_password_modified = base::Time::Now();
  updated_both.password_issues.clear();

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
  histogram_tester.ExpectTotalCount(
      "PasswordManager.PasswordNoteActionInSettings", 0);

  presenter().RemoveObserver(&observer);
}

TEST_F(SavedPasswordsPresenterTest, EditPasswordFails) {
  PasswordForm form1 =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);

  PasswordForm form2 = form1;
  form2.username_value = u"test2@gmail.com";

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
  PasswordForm form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  form.password_issues = {
      {InsecureType::kLeaked,
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false))}};

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

TEST_F(SavedPasswordsPresenterTest, EditPasswordsEmptyList) {
  EXPECT_FALSE(presenter().EditSavedPasswords(
      SavedPasswordsPresenter::SavedPasswordsView(), u"test1@gmail.com",
      u"password"));
}

TEST_F(SavedPasswordsPresenterTest, EditUpdatesDuplicates) {
  PasswordForm form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  form.signon_realm = "https://example.com";
  form.password_issues = {
      {InsecureType::kLeaked,
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false))}};

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
  // The result of the update should have a new password and no password_issues.
  // The same is valid for the duplicate form.
  updated_form.password_value = new_password;
  updated_form.date_password_modified = base::Time::Now();
  updated_form.password_issues.clear();

  PasswordForm updated_duplicate_form = duplicate_form;
  updated_duplicate_form.password_value = new_password;
  updated_duplicate_form.date_password_modified = base::Time::Now();
  updated_duplicate_form.password_issues.clear();

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

TEST_F(SavedPasswordsPresenterTest,
       GetUniquePasswordFormsShouldReturnBlockedAndFederatedForms) {
  PasswordForm form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);

  PasswordForm blocked_form;
  blocked_form.signon_realm = form.signon_realm;
  blocked_form.blocked_by_user = true;
  blocked_form.in_store = PasswordForm::Store::kProfileStore;

  PasswordForm federated_form;
  federated_form.signon_realm = "https://federated.com";
  federated_form.username_value = u"example@gmail.com";
  federated_form.federation_origin =
      url::Origin::Create(GURL(u"federatedOrigin.com"));
  federated_form.in_store = PasswordForm::Store::kProfileStore;

  store().AddLogin(form);
  store().AddLogin(blocked_form);
  store().AddLogin(federated_form);
  RunUntilIdle();

  ASSERT_THAT(
      store().stored_passwords(),
      UnorderedElementsAre(
          Pair(form.signon_realm, UnorderedElementsAre(form, blocked_form)),
          Pair(federated_form.signon_realm, ElementsAre(federated_form))));

  EXPECT_THAT(presenter().GetUniquePasswordForms(),
              UnorderedElementsAre(form, blocked_form, federated_form));
  EXPECT_THAT(presenter().GetSavedCredentials(),
              UnorderedElementsAre(CredentialUIEntry(form),
                                   CredentialUIEntry(blocked_form),
                                   CredentialUIEntry(federated_form)));
}

namespace {

class SavedPasswordsPresenterWithTwoStoresTest : public ::testing::Test {
 protected:
  SavedPasswordsPresenterWithTwoStoresTest() {
    profile_store_->Init(/*prefs=*/nullptr,
                         /*affiliated_match_helper=*/nullptr);
    account_store_->Init(/*prefs=*/nullptr,
                         /*affiliated_match_helper=*/nullptr);
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
  base::test::SingleThreadTaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
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
  PasswordForm profile_store_form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore, /*index=*/0);

  PasswordForm account_store_form1 =
      CreateTestPasswordForm(PasswordForm::Store::kAccountStore, /*index=*/1);

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

// Tests whether passwords added via AddPassword are saved to the correct store
// based on |in_store| value.
TEST_F(SavedPasswordsPresenterWithTwoStoresTest,
       AddPasswordSucceedsToCorrectStore) {
  StrictMockSavedPasswordsPresenterObserver observer;
  presenter().AddObserver(&observer);

  // Add a password to the profile, and check it's added only to the profile
  // store.
  PasswordForm profile_store_form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore, /*index=*/0);
  profile_store_form.type =
      password_manager::PasswordForm::Type::kManuallyAdded;
  profile_store_form.date_created = base::Time::Now();
  profile_store_form.date_password_modified = base::Time::Now();

  EXPECT_CALL(observer,
              OnSavedPasswordsChanged(ElementsAre(profile_store_form)));
  EXPECT_TRUE(presenter().AddPassword(profile_store_form));
  RunUntilIdle();
  EXPECT_THAT(profile_store().stored_passwords(),
              ElementsAre(Pair(profile_store_form.signon_realm,
                               ElementsAre(profile_store_form))));
  EXPECT_TRUE(account_store().IsEmpty());

  // Now add a password to the account store, check it's added only there too.
  PasswordForm account_store_form =
      CreateTestPasswordForm(PasswordForm::Store::kAccountStore, /*index=*/1);
  account_store_form.type =
      password_manager::PasswordForm::Type::kManuallyAdded;
  account_store_form.date_created = base::Time::Now();
  account_store_form.date_password_modified = base::Time::Now();

  EXPECT_CALL(observer, OnSavedPasswordsChanged(UnorderedElementsAre(
                            profile_store_form, account_store_form)));
  EXPECT_TRUE(presenter().AddPassword(account_store_form));
  RunUntilIdle();
  EXPECT_THAT(profile_store().stored_passwords(),
              ElementsAre(Pair(profile_store_form.signon_realm,
                               ElementsAre(profile_store_form))));
  EXPECT_THAT(account_store().stored_passwords(),
              ElementsAre(Pair(account_store_form.signon_realm,
                               ElementsAre(account_store_form))));

  presenter().RemoveObserver(&observer);
}

// Tests AddPassword stores passwords with or without note
TEST_F(SavedPasswordsPresenterWithTwoStoresTest,
       AddPasswordStoresNoteIfExists) {
  StrictMockSavedPasswordsPresenterObserver observer;
  presenter().AddObserver(&observer);

  // Add a password without a note.
  PasswordForm form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore, /*index=*/0);
  form.type = password_manager::PasswordForm::Type::kManuallyAdded;
  form.date_created = base::Time::Now();
  form.date_password_modified = base::Time::Now();

  PasswordForm form2 =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore, /*index=*/1);
  form2.type = password_manager::PasswordForm::Type::kManuallyAdded;
  form2.date_created = base::Time::Now();
  form2.date_password_modified = base::Time::Now();
  form2.notes = {PasswordNote(u"new note", base::Time::Now())};

  base::HistogramTester histogram_tester;
  EXPECT_CALL(observer, OnSavedPasswordsChanged(ElementsAre(form)));
  EXPECT_TRUE(presenter().AddPassword(form));
  RunUntilIdle();
  EXPECT_THAT(profile_store().stored_passwords(),
              ElementsAre(Pair(form.signon_realm, ElementsAre(form))));
  histogram_tester.ExpectTotalCount(
      "PasswordManager.PasswordNoteActionInSettings", 0);

  // Add a password with note.
  EXPECT_CALL(observer,
              OnSavedPasswordsChanged(UnorderedElementsAre(form, form2)));
  EXPECT_TRUE(presenter().AddPassword(form2));
  RunUntilIdle();
  EXPECT_THAT(
      profile_store().stored_passwords(),
      UnorderedElementsAre(Pair(form.signon_realm, ElementsAre(form)),
                           Pair(form2.signon_realm, ElementsAre(form2))));
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordNoteActionInSettings",
      metrics_util::PasswordNoteAction::kNoteAddedInAddDialog, 1);

  presenter().RemoveObserver(&observer);
}

TEST_F(SavedPasswordsPresenterWithTwoStoresTest,
       AddPasswordFailWhenUsernameAlreadyExistsForTheSameDomain) {
  StrictMockSavedPasswordsPresenterObserver observer;
  presenter().AddObserver(&observer);

  PasswordForm form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  form.type = password_manager::PasswordForm::Type::kManuallyAdded;
  form.date_created = base::Time::Now();
  form.date_password_modified = base::Time::Now();

  EXPECT_CALL(observer, OnSavedPasswordsChanged(UnorderedElementsAre(form)));
  EXPECT_TRUE(presenter().AddPassword(form));
  RunUntilIdle();
  EXPECT_THAT(profile_store().stored_passwords(),
              ElementsAre(Pair(form.signon_realm, ElementsAre(form))));
  EXPECT_TRUE(account_store().IsEmpty());

  // Adding password for the same url/username to the same store should fail.
  PasswordForm similar_form = form;
  similar_form.password_value = u"new password";
  EXPECT_CALL(observer, OnSavedPasswordsChanged).Times(0);
  EXPECT_FALSE(presenter().AddPassword(similar_form));
  RunUntilIdle();
  EXPECT_THAT(profile_store().stored_passwords(),
              ElementsAre(Pair(form.signon_realm, ElementsAre(form))));
  EXPECT_TRUE(account_store().IsEmpty());

  // Adding password for the same url/username to another store should also
  // fail.
  similar_form.in_store = PasswordForm::Store::kAccountStore;
  EXPECT_CALL(observer, OnSavedPasswordsChanged).Times(0);
  EXPECT_FALSE(presenter().AddPassword(similar_form));
  RunUntilIdle();
  EXPECT_THAT(profile_store().stored_passwords(),
              ElementsAre(Pair(form.signon_realm, ElementsAre(form))));
  EXPECT_TRUE(account_store().IsEmpty());

  presenter().RemoveObserver(&observer);
}

TEST_F(SavedPasswordsPresenterWithTwoStoresTest,
       AddPasswordUnblocklistsOriginInDifferentStore) {
  PasswordForm form_to_add =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  form_to_add.type = password_manager::PasswordForm::Type::kManuallyAdded;
  form_to_add.date_created = base::Time::Now();
  form_to_add.date_password_modified = base::Time::Now();

  PasswordForm blocked_form;
  blocked_form.blocked_by_user = true;
  blocked_form.signon_realm = form_to_add.signon_realm;
  blocked_form.in_store = PasswordForm::Store::kAccountStore;
  // Blocklist some origin in the account store.
  account_store().AddLogin(blocked_form);
  RunUntilIdle();
  ASSERT_THAT(presenter().GetUniquePasswordForms(), ElementsAre(blocked_form));

  // Add a new entry with the same origin to the profile store.
  EXPECT_TRUE(presenter().AddPassword(form_to_add));
  RunUntilIdle();

  // The entry should be added despite the origin was blocklisted.
  EXPECT_THAT(
      profile_store().stored_passwords(),
      ElementsAre(Pair(form_to_add.signon_realm, ElementsAre(form_to_add))));
  // The origin should be no longer blocklisted irrespective of which store the
  // form was added to.
  EXPECT_THAT(presenter().GetUniquePasswordForms(), ElementsAre(form_to_add));
}

// This tests changing the username of a credentials stored in the profile store
// to be equal to a username of a credential stored in the account store for the
// same domain.
TEST_F(SavedPasswordsPresenterWithTwoStoresTest, EditUsername) {
  PasswordForm profile_store_form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore, /*index=*/0);
  // Make sure the form has a leaked issue and expect that it is cleared
  // because of a username change.
  profile_store_form.password_issues = {
      {InsecureType::kLeaked,
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false))}};

  PasswordForm account_store_form =
      CreateTestPasswordForm(PasswordForm::Store::kAccountStore, /*index=*/1);

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
  profile_store_form.password_issues.clear();
  EXPECT_THAT(profile_store().stored_passwords(),
              ElementsAre(Pair(profile_store_form.signon_realm,
                               ElementsAre(profile_store_form))));
}

// Tests that duplicates of credentials are removed only from the store that
// the initial credential belonged to.
TEST_F(SavedPasswordsPresenterWithTwoStoresTest, DeleteCredentialProfileStore) {
  PasswordForm profile_store_form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  profile_store_form.signon_realm = "https://example.com";

  PasswordForm duplicate_profile_store_form = profile_store_form;
  duplicate_profile_store_form.signon_realm = "https://m.example.com";

  PasswordForm account_store_form = profile_store_form;
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
  PasswordForm profile_store_form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  profile_store_form.signon_realm = "https://example.com";

  PasswordForm account_store_form = profile_store_form;
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

TEST_F(SavedPasswordsPresenterWithTwoStoresTest, DeleteCredentialBothStores) {
  PasswordForm profile_store_form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  profile_store_form.signon_realm = "https://example.com";

  PasswordForm account_store_form = profile_store_form;
  account_store_form.in_store = PasswordForm::Store::kAccountStore;

  PasswordForm mobile_account_store_form = account_store_form;
  mobile_account_store_form.signon_realm = "https://mobile.example.com";

  profile_store().AddLogin(profile_store_form);
  account_store().AddLogin(account_store_form);
  account_store().AddLogin(mobile_account_store_form);
  RunUntilIdle();

  ASSERT_THAT(profile_store().stored_passwords(),
              ElementsAre(Pair(profile_store_form.signon_realm,
                               ElementsAre(profile_store_form))));
  ASSERT_THAT(account_store().stored_passwords(),
              ElementsAre(Pair(account_store_form.signon_realm,
                               ElementsAre(account_store_form)),
                          Pair(mobile_account_store_form.signon_realm,
                               ElementsAre(mobile_account_store_form))));

  PasswordForm form_to_delete = profile_store_form;
  form_to_delete.in_store =
      PasswordForm::Store::kProfileStore | PasswordForm::Store::kAccountStore;

  presenter().RemovePassword(form_to_delete);
  RunUntilIdle();

  // All credentials which are considered duplicates of a 'form_to_delete'
  // should have been deleted from both stores.
  EXPECT_TRUE(profile_store().IsEmpty());
  EXPECT_TRUE(account_store().IsEmpty());
}

TEST_F(SavedPasswordsPresenterWithTwoStoresTest,
       ReturnsUsernamesForRealmFromSameStore) {
  PasswordForm form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);

  PasswordForm other_form = form;
  other_form.username_value = u"test2@gmail.com";

  PasswordForm account_store_form = other_form;
  account_store_form.username_value = u"test3@gmail.com";
  account_store_form.in_store = PasswordForm::Store::kAccountStore;

  profile_store().AddLogin(form);
  profile_store().AddLogin(other_form);

  account_store().AddLogin(account_store_form);

  RunUntilIdle();

  ASSERT_THAT(profile_store().stored_passwords(),
              ElementsAre(Pair(form.signon_realm,
                               UnorderedElementsAre(form, other_form))));

  ASSERT_THAT(account_store().stored_passwords(),
              ElementsAre(Pair(account_store_form.signon_realm,
                               ElementsAre(account_store_form))));

  EXPECT_THAT(
      presenter().GetUsernamesForRealm(form.signon_realm,
                                       /*is_using_account_store=*/false),
      UnorderedElementsAre(form.username_value, other_form.username_value));

  EXPECT_THAT(presenter().GetUsernamesForRealm(account_store_form.signon_realm,
                                               /*is_using_account_store=*/true),
              ElementsAre(account_store_form.username_value));
}

TEST_F(SavedPasswordsPresenterWithTwoStoresTest, GetUniquePasswords) {
  PasswordForm profile_store_form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);

  PasswordForm account_store_form = profile_store_form;
  account_store_form.in_store = PasswordForm::Store::kAccountStore;

  profile_store().AddLogin(profile_store_form);
  account_store().AddLogin(account_store_form);
  RunUntilIdle();

  ASSERT_THAT(profile_store().stored_passwords(),
              ElementsAre(Pair(profile_store_form.signon_realm,
                               ElementsAre(profile_store_form))));
  ASSERT_THAT(account_store().stored_passwords(),
              ElementsAre(Pair(account_store_form.signon_realm,
                               ElementsAre(account_store_form))));

  PasswordForm expected_form = profile_store_form;
  expected_form.in_store =
      PasswordForm::Store::kProfileStore | PasswordForm::Store::kAccountStore;

  EXPECT_THAT(presenter().GetUniquePasswordForms(), ElementsAre(expected_form));
  EXPECT_THAT(presenter().GetSavedCredentials(),
              ElementsAre(CredentialUIEntry(expected_form)));
}

// Prefixes like [m, mobile, www] are considered as "same-site".
TEST_F(SavedPasswordsPresenterWithTwoStoresTest, GetUniquePasswords2) {
  PasswordForm profile_store_form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  profile_store_form.signon_realm = "https://example.com";

  PasswordForm mobile_profile_store_form = profile_store_form;
  mobile_profile_store_form.signon_realm = "https://m.example.com";

  PasswordForm account_form_with_www = profile_store_form;
  account_form_with_www.signon_realm = "https://www.example.com";
  account_form_with_www.in_store = PasswordForm::Store::kAccountStore;

  profile_store().AddLogin(mobile_profile_store_form);
  profile_store().AddLogin(profile_store_form);
  account_store().AddLogin(account_form_with_www);

  RunUntilIdle();

  ASSERT_THAT(
      profile_store().stored_passwords(),
      UnorderedElementsAre(Pair(profile_store_form.signon_realm,
                                ElementsAre(profile_store_form)),
                           Pair(mobile_profile_store_form.signon_realm,
                                ElementsAre(mobile_profile_store_form))));
  ASSERT_THAT(account_store().stored_passwords(),
              ElementsAre(Pair(account_form_with_www.signon_realm,
                               ElementsAre(account_form_with_www))));

  PasswordForm expected_form = profile_store_form;
  expected_form.in_store =
      PasswordForm::Store::kProfileStore | PasswordForm::Store::kAccountStore;

  EXPECT_THAT(presenter().GetUniquePasswordForms(), ElementsAre(expected_form));
}

TEST_F(SavedPasswordsPresenterWithTwoStoresTest, EditPasswordBothStores) {
  PasswordForm profile_store_form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  // Make sure the form has some issues and expect that they are cleared
  // because of the password change.
  profile_store_form.password_issues = {
      {InsecureType::kLeaked,
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false))},
      {InsecureType::kReused,
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false))},
      {InsecureType::kWeak,
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false))},
      {InsecureType::kPhished,
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false))}};

  PasswordForm account_store_form = profile_store_form;
  account_store_form.in_store = PasswordForm::Store::kAccountStore;

  profile_store().AddLogin(profile_store_form);
  account_store().AddLogin(account_store_form);
  RunUntilIdle();

  EXPECT_THAT(profile_store().stored_passwords(),
              ElementsAre(Pair(profile_store_form.signon_realm,
                               ElementsAre(profile_store_form))));

  std::u16string new_username = u"new_test@gmail.com";
  std::u16string new_password = u"new_password";

  EXPECT_TRUE(presenter().EditSavedPasswords(profile_store_form, new_username,
                                             new_password));

  RunUntilIdle();

  PasswordForm expected_profile_store_form = profile_store_form;
  expected_profile_store_form.username_value = new_username;
  expected_profile_store_form.password_value = new_password;
  expected_profile_store_form.in_store = PasswordForm::Store::kProfileStore;
  expected_profile_store_form.date_password_modified = base::Time::Now();
  // The result of the update should not contain password issues, because
  // the username and password have changed.
  expected_profile_store_form.password_issues.clear();
  PasswordForm expected_account_store_form = expected_profile_store_form;
  expected_account_store_form.in_store = PasswordForm::Store::kAccountStore;


  EXPECT_THAT(profile_store().stored_passwords(),
              ElementsAre(Pair(profile_store_form.signon_realm,
                               ElementsAre(expected_profile_store_form))));
  EXPECT_THAT(account_store().stored_passwords(),
              ElementsAre(Pair(account_store_form.signon_realm,
                               ElementsAre(expected_account_store_form))));
}

}  // namespace password_manager
