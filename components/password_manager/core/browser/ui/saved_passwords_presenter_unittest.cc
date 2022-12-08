// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"

#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/affiliation/mock_affiliation_service.h"
#include "components/password_manager/core/browser/fake_password_store_backend.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/sync/base/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

struct MockSavedPasswordsPresenterObserver : SavedPasswordsPresenter::Observer {
  MOCK_METHOD(void, OnEdited, (const CredentialUIEntry&), (override));
  MOCK_METHOD(void, OnSavedPasswordsChanged, (), (override));
};

using StrictMockSavedPasswordsPresenterObserver =
    ::testing::StrictMock<MockSavedPasswordsPresenterObserver>;

class SavedPasswordsPresenterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    store_->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);
    presenter_.Init();
    task_env_.RunUntilIdle();
  }

  void TearDown() override {
    store_->ShutdownOnUIThread();
    task_env_.RunUntilIdle();
  }

  TestPasswordStore& store() { return *store_; }
  SavedPasswordsPresenter& presenter() { return presenter_; }
  MockAffiliationService& affiliation_service() { return affiliation_service_; }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }

 private:
  base::test::SingleThreadTaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<TestPasswordStore> store_ =
      base::MakeRefCounted<TestPasswordStore>();
  MockAffiliationService affiliation_service_;
  SavedPasswordsPresenter presenter_{&affiliation_service_, store_,
                                     /*account_store=*/nullptr};
};

// Parametrized test class which enables or disables the password notes feature
// flag.
class SavedPasswordsPresenterWithPasswordNotesTest
    : public SavedPasswordsPresenterTest,
      public testing::WithParamInterface<bool> {
 protected:
  void SetUp() override {
    if (GetParam())
      feature_list_.InitAndEnableFeature(syncer::kPasswordNotesWithBackup);
    else
      feature_list_.InitAndDisableFeature(syncer::kPasswordNotesWithBackup);
    SavedPasswordsPresenterTest::SetUp();
    RunUntilIdle();
  }
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         SavedPasswordsPresenterWithPasswordNotesTest,
                         testing::Bool());

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
  EXPECT_CALL(observer, OnSavedPasswordsChanged);
  store().AddLogin(form);
  RunUntilIdle();
  EXPECT_FALSE(store().IsEmpty());

  // Remove should notify, and observers should be passed an empty list.
  EXPECT_CALL(observer, OnSavedPasswordsChanged);
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
  EXPECT_CALL(observer, OnSavedPasswordsChanged);
  store().AddLogin(federated_form);
  RunUntilIdle();

  PasswordForm blocked_form;
  blocked_form.blocked_by_user = true;
  EXPECT_CALL(observer, OnSavedPasswordsChanged);
  store().AddLogin(blocked_form);
  RunUntilIdle();

  presenter().RemoveObserver(&observer);
}

TEST_P(SavedPasswordsPresenterWithPasswordNotesTest,
       AddPasswordFailWhenInvalidUrl) {
  StrictMockSavedPasswordsPresenterObserver observer;
  presenter().AddObserver(&observer);

  PasswordForm form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  form.url = GURL("https://;/invalid");

  base::HistogramTester histogram_tester;
  EXPECT_CALL(observer, OnSavedPasswordsChanged).Times(0);
  EXPECT_FALSE(presenter().AddCredential(CredentialUIEntry(form)));
  RunUntilIdle();
  EXPECT_TRUE(store().IsEmpty());

  form.url = GURL("withoutscheme.com");
  EXPECT_CALL(observer, OnSavedPasswordsChanged).Times(0);
  EXPECT_FALSE(presenter().AddCredential(CredentialUIEntry(form)));
  RunUntilIdle();
  EXPECT_TRUE(store().IsEmpty());
  histogram_tester.ExpectTotalCount(
      "PasswordManager.PasswordNoteActionInSettings", 0);

  presenter().RemoveObserver(&observer);
}

TEST_P(SavedPasswordsPresenterWithPasswordNotesTest,
       AddPasswordFailWhenEmptyPassword) {
  StrictMockSavedPasswordsPresenterObserver observer;
  presenter().AddObserver(&observer);

  PasswordForm form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  form.password_value = u"";

  base::HistogramTester histogram_tester;
  EXPECT_CALL(observer, OnSavedPasswordsChanged).Times(0);
  EXPECT_FALSE(presenter().AddCredential(CredentialUIEntry(form)));
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
  ASSERT_THAT(presenter().GetSavedCredentials(),
              ElementsAre(CredentialUIEntry(blocked_form)));

  // Add a new entry with the same origin.
  EXPECT_TRUE(presenter().AddCredential(CredentialUIEntry(form_to_add)));
  RunUntilIdle();

  // The entry should be added despite the origin was blocklisted.
  EXPECT_THAT(
      store().stored_passwords(),
      ElementsAre(Pair(form_to_add.signon_realm, ElementsAre(form_to_add))));
  // The origin should be no longer blocklisted.
  EXPECT_THAT(presenter().GetSavedCredentials(),
              ElementsAre(CredentialUIEntry(form_to_add)));
}

// Tests whether editing a password works and results in the right
// notifications.
TEST_F(SavedPasswordsPresenterTest, EditPassword) {
  PasswordForm form;
  form.in_store = PasswordForm::Store::kProfileStore;
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

  const std::u16string new_password = u"new_password";

  PasswordForm updated = form;
  updated.password_value = new_password;
  CredentialUIEntry updated_credential(updated);
  // The expected updated form should have a new password and no password
  // issues.
  updated.date_password_modified = base::Time::Now();
  updated.password_issues.clear();

  // Verify that editing a password triggers the right notifications.
  EXPECT_CALL(observer, OnEdited(CredentialUIEntry(updated)));
  EXPECT_CALL(observer, OnSavedPasswordsChanged);
  EXPECT_EQ(SavedPasswordsPresenter::EditResult::kSuccess,
            presenter().EditSavedCredentials(CredentialUIEntry(form),
                                             updated_credential));
  RunUntilIdle();
  EXPECT_THAT(store().stored_passwords(),
              ElementsAre(Pair(updated.signon_realm, ElementsAre(updated))));

  // Verify that editing a password that does not exist does not triggers
  // notifications.
  form.username_value = u"another_username";
  EXPECT_CALL(observer, OnEdited).Times(0);
  EXPECT_CALL(observer, OnSavedPasswordsChanged).Times(0);
  EXPECT_EQ(SavedPasswordsPresenter::EditResult::kNotFound,
            presenter().EditSavedCredentials(CredentialUIEntry(form),
                                             updated_credential));
  RunUntilIdle();

  presenter().RemoveObserver(&observer);
}

TEST_P(SavedPasswordsPresenterWithPasswordNotesTest, EditOnlyUsername) {
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

  const std::u16string new_username = u"new_username";
  // The result of the update should have a new username and no password
  // issues.
  PasswordForm updated_username = form;
  updated_username.username_value = new_username;
  updated_username.password_issues.clear();

  CredentialUIEntry credential_to_edit(form);
  credential_to_edit.username = new_username;

  // Verify that editing a username triggers the right notifications.
  base::HistogramTester histogram_tester;

  EXPECT_CALL(observer, OnEdited(CredentialUIEntry(updated_username)));
  EXPECT_CALL(observer, OnSavedPasswordsChanged);
  EXPECT_EQ(SavedPasswordsPresenter::EditResult::kSuccess,
            presenter().EditSavedCredentials(CredentialUIEntry(form),
                                             credential_to_edit));
  RunUntilIdle();
  EXPECT_THAT(
      store().stored_passwords(),
      ElementsAre(Pair(form.signon_realm, ElementsAre(updated_username))));

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordEditUpdatedValues",
      metrics_util::PasswordEditUpdatedValues::kUsername, 1);

  if (GetParam()) {
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.PasswordNoteActionInSettings",
        metrics_util::PasswordNoteAction::kNoteNotChanged, 1);
  } else {
    histogram_tester.ExpectTotalCount(
        "PasswordManager.PasswordNoteActionInSettings", 0);
  }

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

  const std::u16string kNewUsername = u"new_username";
  // The result of the update should have a new username and weak and reused
  // password issues.
  PasswordForm updated_username = form;
  updated_username.username_value = kNewUsername;
  updated_username.password_issues = {
      {InsecureType::kReused,
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false))},
      {InsecureType::kWeak,
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false))}};

  CredentialUIEntry credential_to_edit(form);
  credential_to_edit.username = kNewUsername;

  EXPECT_CALL(observer, OnEdited(CredentialUIEntry(updated_username)));
  EXPECT_CALL(observer, OnSavedPasswordsChanged);
  EXPECT_EQ(SavedPasswordsPresenter::EditResult::kSuccess,
            presenter().EditSavedCredentials(CredentialUIEntry(form),
                                             credential_to_edit));
  RunUntilIdle();
  EXPECT_THAT(
      store().stored_passwords(),
      ElementsAre(Pair(form.signon_realm, ElementsAre(updated_username))));

  presenter().RemoveObserver(&observer);
}

TEST_P(SavedPasswordsPresenterWithPasswordNotesTest, EditOnlyPassword) {
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

  const std::u16string new_password = u"new_password";
  PasswordForm updated_password = form;
  // The result of the update should have a new password and no password
  // issues.
  updated_password.password_value = new_password;
  updated_password.date_password_modified = base::Time::Now();
  updated_password.password_issues.clear();

  CredentialUIEntry credential_to_edit(form);
  credential_to_edit.password = new_password;

  base::HistogramTester histogram_tester;
  // Verify that editing a password triggers the right notifications.
  EXPECT_CALL(observer, OnSavedPasswordsChanged);
  EXPECT_CALL(observer, OnEdited(CredentialUIEntry(updated_password)));
  EXPECT_EQ(SavedPasswordsPresenter::EditResult::kSuccess,
            presenter().EditSavedCredentials(CredentialUIEntry(form),
                                             credential_to_edit));
  RunUntilIdle();
  EXPECT_THAT(
      store().stored_passwords(),
      ElementsAre(Pair(form.signon_realm, ElementsAre(updated_password))));
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordEditUpdatedValues",
      metrics_util::PasswordEditUpdatedValues::kPassword, 1);
  if (GetParam()) {
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.PasswordNoteActionInSettings",
        metrics_util::PasswordNoteAction::kNoteNotChanged, 1);
  } else {
    histogram_tester.ExpectTotalCount(
        "PasswordManager.PasswordNoteActionInSettings", 0);
  }

  presenter().RemoveObserver(&observer);
}

TEST_F(SavedPasswordsPresenterTest, EditOnlyNoteFirstTime) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(syncer::kPasswordNotesWithBackup);
  PasswordForm form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  form.notes.emplace_back(u"display name", u"note with non-empty display name",
                          /*date_created=*/base::Time::Now(),
                          /*hide_by_default=*/true);

  store().AddLogin(form);
  RunUntilIdle();

  const std::u16string kNewNoteValue = u"new note";

  CredentialUIEntry credential_to_edit(form);
  credential_to_edit.note = kNewNoteValue;

  base::HistogramTester histogram_tester;
  EXPECT_EQ(SavedPasswordsPresenter::EditResult::kSuccess,
            presenter().EditSavedCredentials(CredentialUIEntry(form),
                                             credential_to_edit));
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
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(syncer::kPasswordNotesWithBackup);
  PasswordForm form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);

  form.password_issues.insert(
      {InsecureType::kLeaked,
       InsecurityMetadata(base::Time(), IsMuted(false))});

  store().AddLogin(form);
  RunUntilIdle();

  const std::u16string kNewNoteValue = u"new note";

  CredentialUIEntry credential_to_edit(form);
  credential_to_edit.note = kNewNoteValue;

  EXPECT_EQ(SavedPasswordsPresenter::EditResult::kSuccess,
            presenter().EditSavedCredentials(CredentialUIEntry(form),
                                             credential_to_edit));
  RunUntilIdle();

  PasswordForm expected_updated_form = form;
  expected_updated_form.notes = {
      PasswordNote(kNewNoteValue, base::Time::Now())};
  EXPECT_THAT(
      store().stored_passwords(),
      ElementsAre(Pair(form.signon_realm, ElementsAre(expected_updated_form))));
}

TEST_F(SavedPasswordsPresenterTest, EditOnlyNoteSecondTime) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(syncer::kPasswordNotesWithBackup);
  PasswordNote kExistingNote =
      PasswordNote(u"existing note", base::Time::Now());
  PasswordForm form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  form.notes = {kExistingNote};

  store().AddLogin(form);
  RunUntilIdle();
  std::vector<PasswordForm> forms = {form};

  const std::u16string kNewNoteValue = u"new note";

  CredentialUIEntry credential_to_edit(form);
  credential_to_edit.note = kNewNoteValue;

  base::HistogramTester histogram_tester;
  EXPECT_EQ(SavedPasswordsPresenter::EditResult::kSuccess,
            presenter().EditSavedCredentials(CredentialUIEntry(form),
                                             credential_to_edit));
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
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(syncer::kPasswordNotesWithBackup);
  PasswordForm form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  form.notes = {PasswordNote(u"existing note", base::Time::Now())};
  std::vector<PasswordForm> forms = {form};

  store().AddLogin(form);
  RunUntilIdle();

  CredentialUIEntry credential_to_edit(form);
  credential_to_edit.note = u"";

  base::HistogramTester histogram_tester;
  EXPECT_EQ(SavedPasswordsPresenter::EditResult::kSuccess,
            presenter().EditSavedCredentials(CredentialUIEntry(form),
                                             credential_to_edit));

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
  const std::u16string kNoteWithEmptyDisplayName =
      u"note with empty display name";
  PasswordForm form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  form.notes.emplace_back(u"display name", u"note with non-empty display name",
                          /*date_created=*/base::Time::Now(),
                          /*hide_by_default=*/true);
  form.notes.emplace_back(kNoteWithEmptyDisplayName, base::Time::Now());

  store().AddLogin(form);
  RunUntilIdle();

  // The expect credential UI entry should contain only the note with that empty
  // display name.
  std::vector<CredentialUIEntry> saved_credentials =
      presenter().GetSavedCredentials();
  ASSERT_EQ(1U, saved_credentials.size());
  EXPECT_EQ(kNoteWithEmptyDisplayName, saved_credentials[0].note);
}

TEST_P(SavedPasswordsPresenterWithPasswordNotesTest, EditUsernameAndPassword) {
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

  const std::u16string new_username = u"new_username";
  const std::u16string new_password = u"new_password";

  PasswordForm updated_both = form;
  // The result of the update should have a new username and password and no
  // password issues.
  updated_both.username_value = new_username;
  updated_both.password_value = new_password;
  updated_both.date_password_modified = base::Time::Now();
  updated_both.password_issues.clear();

  CredentialUIEntry credential_to_edit(form);
  credential_to_edit.username = new_username;
  credential_to_edit.password = new_password;

  base::HistogramTester histogram_tester;
  // Verify that editing username and password triggers the right notifications.
  EXPECT_CALL(observer, OnSavedPasswordsChanged);
  EXPECT_CALL(observer, OnEdited(CredentialUIEntry(updated_both)));
  EXPECT_EQ(SavedPasswordsPresenter::EditResult::kSuccess,
            presenter().EditSavedCredentials(CredentialUIEntry(form),
                                             credential_to_edit));
  RunUntilIdle();
  EXPECT_THAT(store().stored_passwords(),
              ElementsAre(Pair(form.signon_realm, ElementsAre(updated_both))));
  histogram_tester.ExpectBucketCount(
      "PasswordManager.PasswordEditUpdatedValues",
      metrics_util::PasswordEditUpdatedValues::kBoth, 1);
  if (GetParam()) {
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.PasswordNoteActionInSettings",
        metrics_util::PasswordNoteAction::kNoteNotChanged, 1);
  } else {
    histogram_tester.ExpectTotalCount(
        "PasswordManager.PasswordNoteActionInSettings", 0);
  }

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

  CredentialUIEntry credential_to_edit(form1);
  credential_to_edit.username = form2.username_value;
  // Updating the form with the username which is already used for same website
  // fails.
  EXPECT_EQ(SavedPasswordsPresenter::EditResult::kAlreadyExisits,
            presenter().EditSavedCredentials(CredentialUIEntry(form1),
                                             credential_to_edit));
  RunUntilIdle();
  EXPECT_THAT(store().stored_passwords(),
              ElementsAre(Pair(form1.signon_realm, ElementsAre(form1, form2))));

  credential_to_edit = CredentialUIEntry(form1);
  credential_to_edit.password = u"";
  // Updating the form with the empty password fails.
  EXPECT_EQ(SavedPasswordsPresenter::EditResult::kEmptyPassword,
            presenter().EditSavedCredentials(CredentialUIEntry(form1),
                                             credential_to_edit));
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

  EXPECT_EQ(SavedPasswordsPresenter::EditResult::kNothingChanged,
            presenter().EditSavedCredentials(CredentialUIEntry(form),
                                             CredentialUIEntry(form)));
  RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      "PasswordManager.PasswordEditUpdatedValues",
      metrics_util::PasswordEditUpdatedValues::kNone, 1);

  presenter().RemoveObserver(&observer);
}

TEST_F(SavedPasswordsPresenterTest, EditPasswordsEmptyList) {
  CredentialUIEntry credential(
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore));
  EXPECT_EQ(SavedPasswordsPresenter::EditResult::kNotFound,
            presenter().EditSavedCredentials(credential, credential));
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
  updated_form.password_value = new_password;
  CredentialUIEntry updated_credential(updated_form);

  // The result of the update should have a new password and no password_issues.
  // The same is valid for the duplicate form.
  updated_form.date_password_modified = base::Time::Now();
  updated_form.password_issues.clear();

  PasswordForm updated_duplicate_form = duplicate_form;
  updated_duplicate_form.password_value = new_password;
  updated_duplicate_form.date_password_modified = base::Time::Now();
  updated_duplicate_form.password_issues.clear();

  EXPECT_CALL(observer, OnEdited(CredentialUIEntry(updated_form)));
  // The notification that the logins have changed arrives after both updates
  // are sent to the store and db. This means that there will be 2 requests
  // from the presenter to get the updated credentials, BUT they are both sent
  // after the writes.
  EXPECT_CALL(observer, OnSavedPasswordsChanged).Times(2);
  EXPECT_EQ(SavedPasswordsPresenter::EditResult::kSuccess,
            presenter().EditSavedCredentials(CredentialUIEntry(form),
                                             updated_credential));
  RunUntilIdle();
  EXPECT_THAT(store().stored_passwords(),
              ElementsAre(Pair(form.signon_realm, ElementsAre(updated_form)),
                          Pair(duplicate_form.signon_realm,
                               ElementsAre(updated_duplicate_form))));
  presenter().RemoveObserver(&observer);
}

TEST_F(SavedPasswordsPresenterTest,
       GetSavedCredentialsReturnsBlockedAndFederatedForms) {
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

  EXPECT_THAT(presenter().GetSavedCredentials(),
              UnorderedElementsAre(CredentialUIEntry(form),
                                   CredentialUIEntry(blocked_form),
                                   CredentialUIEntry(federated_form)));
}

TEST_F(SavedPasswordsPresenterTest, UndoRemoval) {
  PasswordForm form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  store().AddLogin(form);
  RunUntilIdle();

  CredentialUIEntry credential = CredentialUIEntry(form);

  ASSERT_THAT(presenter().GetSavedCredentials(), ElementsAre(credential));

  presenter().RemoveCredential(credential);
  RunUntilIdle();
  EXPECT_THAT(presenter().GetSavedCredentials(), IsEmpty());

  presenter().UndoLastRemoval();
  RunUntilIdle();
  EXPECT_THAT(presenter().GetSavedCredentials(), ElementsAre(credential));
}

namespace {

class SavedPasswordsPresenterWithTwoStoresTest : public ::testing::Test {
 protected:
  void SetUp() override {
    profile_store_->Init(/*prefs=*/nullptr,
                         /*affiliated_match_helper=*/nullptr);
    account_store_->Init(/*prefs=*/nullptr,
                         /*affiliated_match_helper=*/nullptr);
    presenter_.Init();
    RunUntilIdle();
  }

  void TearDown() override {
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
  MockAffiliationService affiliation_service_;
  SavedPasswordsPresenter presenter_{&affiliation_service_, profile_store_,
                                     account_store_};
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

  EXPECT_CALL(observer, OnSavedPasswordsChanged);
  profile_store().AddLogin(profile_store_form);
  RunUntilIdle();

  EXPECT_CALL(observer, OnSavedPasswordsChanged);
  account_store().AddLogin(account_store_form1);
  RunUntilIdle();

  EXPECT_CALL(observer, OnSavedPasswordsChanged);
  account_store().AddLogin(account_store_form2);
  RunUntilIdle();

  EXPECT_CALL(observer, OnSavedPasswordsChanged);
  profile_store().RemoveLogin(profile_store_form);
  RunUntilIdle();

  EXPECT_CALL(observer, OnSavedPasswordsChanged);
  profile_store().AddLogin(profile_store_form);
  RunUntilIdle();

  presenter().RemoveObserver(&observer);
}

// Empty list should not crash.
TEST_F(SavedPasswordsPresenterTest, AddCredentialsListEmpty) {
  base::MockCallback<SavedPasswordsPresenter::AddCredentialsCallback>
      completion_callback;
  presenter().AddCredentials({},
                             password_manager::PasswordForm::Type::kImported,
                             completion_callback.Get());
  EXPECT_CALL(completion_callback,
              Run(std::vector<SavedPasswordsPresenter::AddResult>{}));
  RunUntilIdle();

  // Call RunUntilIdle again to await when SavedPasswordsPresenter obtain all
  // the logins.
  RunUntilIdle();
  EXPECT_THAT(presenter().GetSavedCredentials(), IsEmpty());
}

// Tests whether adding 1 password notifies observers with credentials in one
// store.
TEST_F(SavedPasswordsPresenterTest, AddCredentialsListOnePassword) {
  PasswordForm profile_store_form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore, /*index=*/0);
  profile_store_form.type =
      password_manager::PasswordForm::Type::kManuallyAdded;
  profile_store_form.date_created = base::Time::Now();
  profile_store_form.date_password_modified = base::Time::Now();

  StrictMockSavedPasswordsPresenterObserver observer;
  presenter().AddObserver(&observer);

  EXPECT_CALL(observer, OnSavedPasswordsChanged);

  base::MockCallback<SavedPasswordsPresenter::AddCredentialsCallback>
      completion_callback;
  CredentialUIEntry profile_store_cred(profile_store_form);
  presenter().AddCredentials(
      {profile_store_cred},
      password_manager::PasswordForm::Type::kManuallyAdded,
      completion_callback.Get());
  EXPECT_CALL(completion_callback,
              Run(std::vector<SavedPasswordsPresenter::AddResult>{
                  SavedPasswordsPresenter::AddResult::kSuccess}));
  RunUntilIdle();
  presenter().RemoveObserver(&observer);
}

// Tests whether adding 2 credentials with 1 that has same username and realm in
// the profile store fails with the correct response.
TEST_F(SavedPasswordsPresenterWithTwoStoresTest,
       AddCredentialsListTwoPasswordOneConflictsProfileStore) {
  PasswordForm existing_profile_form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore, /*index=*/0);
  PasswordForm new_profile_form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore, /*index=*/1);

  profile_store().AddLogin(existing_profile_form);
  RunUntilIdle();

  PasswordForm conflicting_profile_form = existing_profile_form;
  conflicting_profile_form.password_value = u"abc";

  base::MockCallback<SavedPasswordsPresenter::AddCredentialsCallback>
      completion_callback;

  presenter().AddCredentials({CredentialUIEntry(conflicting_profile_form),
                              CredentialUIEntry(new_profile_form)},
                             password_manager::PasswordForm::Type::kImported,
                             completion_callback.Get());
  EXPECT_CALL(completion_callback,
              Run(std::vector<SavedPasswordsPresenter::AddResult>{
                  SavedPasswordsPresenter::AddResult::kConflictInProfileStore,
                  SavedPasswordsPresenter::AddResult::kSuccess}));
  RunUntilIdle();

  // Call RunUntilIdle again to await when SavedPasswordsPresenter obtain all
  // the logins.
  RunUntilIdle();
  EXPECT_THAT(presenter().GetSavedCredentials(),
              UnorderedElementsAre(CredentialUIEntry(existing_profile_form),
                                   CredentialUIEntry(new_profile_form)));
}

// Tests whether adding whether adding 2 credentials with 1 that has same
// username and realm in the account store fails with the correct response.
TEST_F(SavedPasswordsPresenterWithTwoStoresTest,
       AddCredentialsListTwoPasswordOneConflictsAccountStore) {
  PasswordForm existing_account_form =
      CreateTestPasswordForm(PasswordForm::Store::kAccountStore, /*index=*/0);
  PasswordForm new_account_form =
      CreateTestPasswordForm(PasswordForm::Store::kAccountStore, /*index=*/1);

  account_store().AddLogin(existing_account_form);
  RunUntilIdle();

  PasswordForm conflicting_account_form = existing_account_form;
  conflicting_account_form.password_value = u"abc";

  base::MockCallback<SavedPasswordsPresenter::AddCredentialsCallback>
      completion_callback;
  presenter().AddCredentials({CredentialUIEntry(conflicting_account_form),
                              CredentialUIEntry(new_account_form)},
                             password_manager::PasswordForm::Type::kImported,
                             completion_callback.Get());
  EXPECT_CALL(completion_callback,
              Run(std::vector<SavedPasswordsPresenter::AddResult>{
                  SavedPasswordsPresenter::AddResult::kConflictInAccountStore,
                  SavedPasswordsPresenter::AddResult::kSuccess}));
  RunUntilIdle();

  // Call RunUntilIdle again to await when SavedPasswordsPresenter obtain all
  // the logins.
  RunUntilIdle();
  EXPECT_THAT(presenter().GetSavedCredentials(),
              UnorderedElementsAre(CredentialUIEntry(existing_account_form),
                                   CredentialUIEntry(new_account_form)));
}

// Tests whether adding 2 credentials with 1 that has same username and realm in
// both profile and account store fails with the correct response.
TEST_F(SavedPasswordsPresenterWithTwoStoresTest,
       AddCredentialsListTwoPasswordOneConflictsProfileAndAccountStore) {
  PasswordForm existing_profile_form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore, /*index=*/0);

  PasswordForm existing_account_form =
      CreateTestPasswordForm(PasswordForm::Store::kAccountStore, /*index=*/0);

  PasswordForm new_profile_form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore, /*index=*/1);

  profile_store().AddLogin(existing_profile_form);
  account_store().AddLogin(existing_account_form);
  RunUntilIdle();

  PasswordForm conflicting_profile_form = existing_profile_form;
  conflicting_profile_form.password_value = u"abc";

  base::MockCallback<SavedPasswordsPresenter::AddCredentialsCallback>
      completion_callback;
  presenter().AddCredentials({CredentialUIEntry(conflicting_profile_form),
                              CredentialUIEntry(new_profile_form)},
                             password_manager::PasswordForm::Type::kImported,
                             completion_callback.Get());
  EXPECT_CALL(
      completion_callback,
      Run(std::vector<SavedPasswordsPresenter::AddResult>{
          SavedPasswordsPresenter::AddResult::kConflictInProfileAndAccountStore,
          SavedPasswordsPresenter::AddResult::kSuccess}));
  RunUntilIdle();

  // Call RunUntilIdle again to await when SavedPasswordsPresenter obtain all
  // the logins.
  RunUntilIdle();
  EXPECT_THAT(presenter().GetSavedCredentials(),
              UnorderedElementsAre(CredentialUIEntry(existing_profile_form),
                                   CredentialUIEntry(new_profile_form)));
}

// Tests whether adding 2 passwords with 1 that already exists in the profile
// store fails with the correct response.
TEST_F(SavedPasswordsPresenterWithTwoStoresTest,
       AddCredentialsListTwoPasswordOneExactMatchProfileStore) {
  PasswordForm existing_profile_form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore, /*index=*/0);
  PasswordForm new_profile_form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore, /*index=*/1);

  profile_store().AddLogin(existing_profile_form);
  RunUntilIdle();

  base::MockCallback<SavedPasswordsPresenter::AddCredentialsCallback>
      completion_callback;

  presenter().AddCredentials({CredentialUIEntry(existing_profile_form),
                              CredentialUIEntry(new_profile_form)},
                             password_manager::PasswordForm::Type::kImported,
                             completion_callback.Get());
  EXPECT_CALL(completion_callback,
              Run(std::vector<SavedPasswordsPresenter::AddResult>{
                  SavedPasswordsPresenter::AddResult::kExactMatch,
                  SavedPasswordsPresenter::AddResult::kSuccess}));
  RunUntilIdle();

  // Call RunUntilIdle again to await when SavedPasswordsPresenter obtain all
  // the logins.
  RunUntilIdle();
  EXPECT_THAT(presenter().GetSavedCredentials(),
              UnorderedElementsAre(CredentialUIEntry(existing_profile_form),
                                   CredentialUIEntry(new_profile_form)));
}

// Tests whether adding whether adding 2 passwords with 1 that already exists in
// the account store fails with the correct response.
TEST_F(SavedPasswordsPresenterWithTwoStoresTest,
       AddCredentialsListTwoPasswordOneExactMatchAccountStore) {
  PasswordForm existing_account_form =
      CreateTestPasswordForm(PasswordForm::Store::kAccountStore, /*index=*/0);
  PasswordForm new_account_form =
      CreateTestPasswordForm(PasswordForm::Store::kAccountStore, /*index=*/1);

  account_store().AddLogin(existing_account_form);
  RunUntilIdle();

  base::MockCallback<SavedPasswordsPresenter::AddCredentialsCallback>
      completion_callback;
  presenter().AddCredentials({CredentialUIEntry(existing_account_form),
                              CredentialUIEntry(new_account_form)},
                             password_manager::PasswordForm::Type::kImported,
                             completion_callback.Get());
  EXPECT_CALL(completion_callback,
              Run(std::vector<SavedPasswordsPresenter::AddResult>{
                  SavedPasswordsPresenter::AddResult::kExactMatch,
                  SavedPasswordsPresenter::AddResult::kSuccess}));
  RunUntilIdle();

  // Call RunUntilIdle again to await when SavedPasswordsPresenter obtain all
  // the logins.
  RunUntilIdle();
  EXPECT_THAT(presenter().GetSavedCredentials(),
              UnorderedElementsAre(CredentialUIEntry(existing_account_form),
                                   CredentialUIEntry(new_account_form)));
}

// Tests whether adding 2 passwords with 1 that already exists in both profile
// and account store fails with the correct response.
TEST_F(SavedPasswordsPresenterWithTwoStoresTest,
       AddCredentialsListTwoPasswordOneExactMatchProfileAndAccountStore) {
  PasswordForm existing_profile_form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore, /*index=*/0);
  PasswordForm existing_account_form =
      CreateTestPasswordForm(PasswordForm::Store::kAccountStore, /*index=*/0);

  PasswordForm new_profile_form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore, /*index=*/1);

  profile_store().AddLogin(existing_profile_form);
  account_store().AddLogin(existing_account_form);
  RunUntilIdle();

  base::MockCallback<SavedPasswordsPresenter::AddCredentialsCallback>
      completion_callback;
  presenter().AddCredentials({CredentialUIEntry(existing_profile_form),
                              CredentialUIEntry(new_profile_form)},
                             password_manager::PasswordForm::Type::kImported,
                             completion_callback.Get());
  EXPECT_CALL(completion_callback,
              Run(std::vector<SavedPasswordsPresenter::AddResult>{
                  SavedPasswordsPresenter::AddResult::kExactMatch,
                  SavedPasswordsPresenter::AddResult::kSuccess}));
  RunUntilIdle();

  // Call RunUntilIdle again to await when SavedPasswordsPresenter obtain all
  // the logins.
  RunUntilIdle();
  EXPECT_THAT(presenter().GetSavedCredentials(),
              UnorderedElementsAre(CredentialUIEntry(existing_profile_form),
                                   CredentialUIEntry(new_profile_form)));
}

// Tests whether adding 2 passwords notifies observers with credentials in one
// store.
TEST_F(SavedPasswordsPresenterWithTwoStoresTest,
       AddCredentialsListPasswordAccountStore) {
  PasswordForm account_store_form_1 =
      CreateTestPasswordForm(PasswordForm::Store::kAccountStore, /*index=*/0);
  account_store_form_1.type = password_manager::PasswordForm::Type::kImported;
  account_store_form_1.date_created = base::Time::Now();
  account_store_form_1.date_password_modified = base::Time::Now();

  PasswordForm account_store_form_2 =
      CreateTestPasswordForm(PasswordForm::Store::kAccountStore, /*index=*/1);
  account_store_form_2.type = password_manager::PasswordForm::Type::kImported;
  account_store_form_2.date_created = base::Time::Now();
  account_store_form_2.date_password_modified = base::Time::Now();

  StrictMockSavedPasswordsPresenterObserver observer;
  presenter().AddObserver(&observer);

  base::MockCallback<SavedPasswordsPresenter::AddCredentialsCallback>
      completion_callback;

  EXPECT_CALL(observer, OnSavedPasswordsChanged).Times(2);

  CredentialUIEntry account_store_cred_1(account_store_form_1);
  CredentialUIEntry account_store_cred_2(account_store_form_2);

  presenter().AddCredentials({account_store_cred_1, account_store_cred_2},
                             password_manager::PasswordForm::Type::kImported,
                             completion_callback.Get());
  EXPECT_CALL(completion_callback,
              Run(std::vector<SavedPasswordsPresenter::AddResult>{
                  SavedPasswordsPresenter::AddResult::kSuccess,
                  SavedPasswordsPresenter::AddResult::kSuccess}));
  RunUntilIdle();

  // Call RunUntilIdle again to await when SavedPasswordsPresenter obtain all
  // the logins.
  RunUntilIdle();
  EXPECT_THAT(presenter().GetSavedCredentials(),
              UnorderedElementsAre(account_store_cred_1, account_store_cred_2));
  presenter().RemoveObserver(&observer);
}

// Tests whether adding 2 passwords (1 invalid, 1 valid) notifies observers with
// only the valid password and returns the correct list of statuses.
TEST_F(SavedPasswordsPresenterTest,
       AddCredentialsListPasswordProfileStoreWithOneInvalid) {
  PasswordForm profile_store_form_1 =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore, /*index=*/0);
  profile_store_form_1.password_value = u"";
  profile_store_form_1.type = password_manager::PasswordForm::Type::kImported;
  profile_store_form_1.date_created = base::Time::Now();
  profile_store_form_1.date_password_modified = base::Time::Now();

  PasswordForm profile_store_form_2 =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore, /*index=*/1);
  profile_store_form_2.type = password_manager::PasswordForm::Type::kImported;
  profile_store_form_2.date_created = base::Time::Now();
  profile_store_form_2.date_password_modified = base::Time::Now();

  StrictMockSavedPasswordsPresenterObserver observer;
  presenter().AddObserver(&observer);

  base::MockCallback<SavedPasswordsPresenter::AddCredentialsCallback>
      completion_callback;

  EXPECT_CALL(observer, OnSavedPasswordsChanged);

  CredentialUIEntry profile_store_cred_1(profile_store_form_1);
  CredentialUIEntry profile_store_cred_2(profile_store_form_2);
  presenter().AddCredentials({profile_store_cred_1, profile_store_cred_2},
                             password_manager::PasswordForm::Type::kImported,
                             completion_callback.Get());
  EXPECT_CALL(completion_callback,
              Run(std::vector<SavedPasswordsPresenter::AddResult>{
                  SavedPasswordsPresenter::AddResult::kInvalid,
                  SavedPasswordsPresenter::AddResult::kSuccess}));
  RunUntilIdle();
  presenter().RemoveObserver(&observer);

  // Call RunUntilIdle again to await when SavedPasswordsPresenter obtain all
  // the logins.
  RunUntilIdle();
  EXPECT_THAT(presenter().GetSavedCredentials(),
              UnorderedElementsAre(profile_store_cred_2));
}

TEST_F(SavedPasswordsPresenterTest, AddCredentialsAcceptsOnlyValidURLs) {
  base::MockCallback<SavedPasswordsPresenter::AddCredentialsCallback>
      completion_callback;

  PasswordForm valid_url_form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore, /*index=*/0);
  PasswordForm valid_android_form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore, /*index=*/1);
  PasswordForm invalid_ulr_form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore, /*index=*/2);

  valid_android_form.url = GURL(
      "android://"
      "Jzj5T2E45Hb33D-lk-"
      "EHZVCrb7a064dEicTwrTYQYGXO99JqE2YERhbMP1qLogwJiy87OsBzC09Gk094Z-U_hg==@"
      "com.netflix.mediaclient");
  valid_android_form.signon_realm = valid_android_form.url.spec();
  invalid_ulr_form.url = GURL("http/site:80");
  invalid_ulr_form.signon_realm = valid_android_form.url.spec();

  CredentialUIEntry valid_url_cred(valid_url_form);
  CredentialUIEntry valid_android_cred(valid_android_form);
  CredentialUIEntry invalid_ulr_cred(invalid_ulr_form);
  presenter().AddCredentials(
      {valid_url_cred, valid_android_cred, invalid_ulr_cred},
      password_manager::PasswordForm::Type::kImported,
      completion_callback.Get());
  EXPECT_CALL(completion_callback,
              Run(std::vector<SavedPasswordsPresenter::AddResult>{
                  SavedPasswordsPresenter::AddResult::kSuccess,
                  SavedPasswordsPresenter::AddResult::kSuccess,
                  SavedPasswordsPresenter::AddResult::kInvalid}));
  RunUntilIdle();

  // Call RunUntilIdle again to await when SavedPasswordsPresenter obtain all
  // the logins.
  RunUntilIdle();
  EXPECT_THAT(presenter().GetSavedCredentials(),
              UnorderedElementsAre(valid_url_cred, valid_android_cred));
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

  EXPECT_CALL(observer, OnSavedPasswordsChanged);
  EXPECT_TRUE(presenter().AddCredential(CredentialUIEntry(profile_store_form)));
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

  EXPECT_CALL(observer, OnSavedPasswordsChanged);
  EXPECT_TRUE(presenter().AddCredential(CredentialUIEntry(account_store_form)));
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
  EXPECT_CALL(observer, OnSavedPasswordsChanged);
  EXPECT_TRUE(presenter().AddCredential(CredentialUIEntry(form)));
  RunUntilIdle();
  EXPECT_THAT(profile_store().stored_passwords(),
              ElementsAre(Pair(form.signon_realm, ElementsAre(form))));
  histogram_tester.ExpectTotalCount(
      "PasswordManager.PasswordNoteActionInSettings", 0);

  // Add a password with note.
  EXPECT_CALL(observer, OnSavedPasswordsChanged);
  EXPECT_TRUE(presenter().AddCredential(CredentialUIEntry(form2)));
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

  EXPECT_CALL(observer, OnSavedPasswordsChanged);
  EXPECT_TRUE(presenter().AddCredential(CredentialUIEntry(form)));
  RunUntilIdle();
  EXPECT_THAT(profile_store().stored_passwords(),
              ElementsAre(Pair(form.signon_realm, ElementsAre(form))));
  EXPECT_TRUE(account_store().IsEmpty());

  // Adding password for the same url/username to the same store should fail.
  PasswordForm similar_form = form;
  similar_form.password_value = u"new password";
  EXPECT_CALL(observer, OnSavedPasswordsChanged).Times(0);
  EXPECT_FALSE(presenter().AddCredential(CredentialUIEntry(similar_form)));
  RunUntilIdle();
  EXPECT_THAT(profile_store().stored_passwords(),
              ElementsAre(Pair(form.signon_realm, ElementsAre(form))));
  EXPECT_TRUE(account_store().IsEmpty());

  // Adding password for the same url/username to another store should also
  // fail.
  similar_form.in_store = PasswordForm::Store::kAccountStore;
  EXPECT_CALL(observer, OnSavedPasswordsChanged).Times(0);
  EXPECT_FALSE(presenter().AddCredential(CredentialUIEntry(similar_form)));
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
  ASSERT_THAT(presenter().GetSavedCredentials(),
              ElementsAre(CredentialUIEntry(blocked_form)));

  // Add a new entry with the same origin to the profile store.
  EXPECT_TRUE(presenter().AddCredential(CredentialUIEntry(form_to_add)));
  RunUntilIdle();

  // The entry should be added despite the origin was blocklisted.
  EXPECT_THAT(
      profile_store().stored_passwords(),
      ElementsAre(Pair(form_to_add.signon_realm, ElementsAre(form_to_add))));
  // The origin should be no longer blocklisted irrespective of which store the
  // form was added to.
  EXPECT_THAT(presenter().GetSavedCredentials(),
              ElementsAre(CredentialUIEntry(form_to_add)));
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
  CredentialUIEntry credential_to_edit(profile_store_form);
  credential_to_edit.username = new_username;

  EXPECT_EQ(SavedPasswordsPresenter::EditResult::kSuccess,
            presenter().EditSavedCredentials(
                CredentialUIEntry(profile_store_form), credential_to_edit));
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

  presenter().RemoveCredential(CredentialUIEntry(profile_store_form));
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

  presenter().RemoveCredential(CredentialUIEntry(account_store_form));
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

  presenter().RemoveCredential(CredentialUIEntry(form_to_delete));
  RunUntilIdle();

  // All credentials which are considered duplicates of a 'form_to_delete'
  // should have been deleted from both stores.
  EXPECT_TRUE(profile_store().IsEmpty());
  EXPECT_TRUE(account_store().IsEmpty());
}

TEST_F(SavedPasswordsPresenterWithTwoStoresTest, GetSavedCredentials) {
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

  EXPECT_THAT(presenter().GetSavedCredentials(),
              ElementsAre(CredentialUIEntry(expected_form)));
}

TEST_F(SavedPasswordsPresenterTest, GetAffiliatedGroups) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordsGrouping);

  PasswordForm form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);

  PasswordForm form2 =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  form2.username_value = u"test2@gmail.com";
  form2.password_value = u"password2";

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
  store().AddLogin(form2);
  store().AddLogin(blocked_form);
  store().AddLogin(federated_form);

  EXPECT_CALL(affiliation_service(), GetAllGroups)
      .WillRepeatedly([&form, &federated_form](
                          AffiliationService::GroupsCallback callback) {
        // Setup callback result.
        std::vector<password_manager::GroupedFacets> grouped_facets_to_return;

        // Form, Form2 & Blocked form.
        Facet facet;
        facet.uri = FacetURI::FromPotentiallyInvalidSpec(form.signon_realm);
        GroupedFacets grouped_facets;
        grouped_facets.facets.push_back(std::move(facet));
        grouped_facets_to_return.push_back(std::move(grouped_facets));

        // Federated form.
        Facet facet2;
        facet2.uri =
            FacetURI::FromPotentiallyInvalidSpec(federated_form.signon_realm);
        GroupedFacets grouped_facets2;
        grouped_facets2.facets.push_back(std::move(facet2));
        grouped_facets_to_return.push_back(std::move(grouped_facets2));

        std::move(callback).Run(std::move(grouped_facets_to_return));
      });

  RunUntilIdle();

  ASSERT_THAT(
      store().stored_passwords(),
      UnorderedElementsAre(
          Pair(form.signon_realm,
               UnorderedElementsAre(form, form2, blocked_form)),
          Pair(federated_form.signon_realm, ElementsAre(federated_form))));

  // Setup results to compare.
  CredentialUIEntry credential1 = CredentialUIEntry(form);
  CredentialUIEntry credential2 = CredentialUIEntry(form2);
  AffiliatedGroup affiliated_group1;
  affiliated_group1.AddCredential(credential1);
  affiliated_group1.AddCredential(credential2);
  FacetBrandingInfo branding_info1;
  branding_info1.name = GetShownOrigin(credential1);
  affiliated_group1.SetBrandingInfo(branding_info1);

  CredentialUIEntry credential3 = CredentialUIEntry(federated_form);
  AffiliatedGroup affiliated_group2;
  affiliated_group2.AddCredential(credential3);
  FacetBrandingInfo branding_info2;
  branding_info2.name = GetShownOrigin(credential3);
  affiliated_group2.SetBrandingInfo(branding_info2);

  EXPECT_THAT(presenter().GetAffiliatedGroups(),
              UnorderedElementsAre(affiliated_group1, affiliated_group2));
}

TEST_F(SavedPasswordsPresenterTest, GetBlockedSites) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordsGrouping);

  PasswordForm form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);

  PasswordForm blocked_form;
  blocked_form.signon_realm = form.signon_realm;
  blocked_form.blocked_by_user = true;
  blocked_form.in_store = PasswordForm::Store::kProfileStore;

  PasswordForm blocked_form2;
  blocked_form2.signon_realm = "https://test2.com";
  blocked_form2.blocked_by_user = true;
  blocked_form2.in_store = PasswordForm::Store::kProfileStore;

  store().AddLogin(form);
  store().AddLogin(blocked_form);
  store().AddLogin(blocked_form2);

  EXPECT_CALL(affiliation_service(), GetAllGroups)
      .WillRepeatedly([&form, &blocked_form2](
                          AffiliationService::GroupsCallback callback) {
        // Setup callback result.
        std::vector<password_manager::GroupedFacets> grouped_facets_to_return;

        // Form & Blocked form.
        Facet facet;
        facet.uri = FacetURI::FromPotentiallyInvalidSpec(form.signon_realm);
        GroupedFacets grouped_facets;
        grouped_facets.facets.push_back(std::move(facet));
        grouped_facets_to_return.push_back(std::move(grouped_facets));

        // Blocked form 2.
        Facet facet2;
        facet2.uri =
            FacetURI::FromPotentiallyInvalidSpec(blocked_form2.signon_realm);
        GroupedFacets grouped_facets2;
        grouped_facets2.facets.push_back(std::move(facet2));
        grouped_facets_to_return.push_back(std::move(grouped_facets2));

        std::move(callback).Run(std::move(grouped_facets_to_return));
      });

  RunUntilIdle();

  ASSERT_THAT(
      store().stored_passwords(),
      UnorderedElementsAre(
          Pair(form.signon_realm, UnorderedElementsAre(form, blocked_form)),
          Pair(blocked_form2.signon_realm, ElementsAre(blocked_form2))));

  EXPECT_THAT(presenter().GetBlockedSites(),
              UnorderedElementsAre(CredentialUIEntry(blocked_form),
                                   CredentialUIEntry(blocked_form2)));
}

// Prefixes like [m, mobile, www] are considered as "same-site".
TEST_F(SavedPasswordsPresenterWithTwoStoresTest,
       GetSavedCredentialsGroupsSameSites) {
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

  EXPECT_THAT(presenter().GetSavedCredentials(),
              ElementsAre(CredentialUIEntry(expected_form)));
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

  CredentialUIEntry updated_credential(profile_store_form);
  updated_credential.username = new_username;
  updated_credential.password = new_password;
  EXPECT_EQ(SavedPasswordsPresenter::EditResult::kSuccess,
            presenter().EditSavedCredentials(
                CredentialUIEntry(profile_store_form), updated_credential));

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

TEST_F(SavedPasswordsPresenterWithTwoStoresTest, UndoRemoval) {
  PasswordForm profile_store_form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);

  PasswordForm account_store_form = profile_store_form;
  account_store_form.in_store = PasswordForm::Store::kAccountStore;

  profile_store().AddLogin(profile_store_form);
  account_store().AddLogin(account_store_form);
  RunUntilIdle();

  ASSERT_EQ(1u, presenter().GetSavedCredentials().size());
  CredentialUIEntry credential = presenter().GetSavedCredentials()[0];
  ASSERT_EQ(2u, credential.stored_in.size());
  presenter().RemoveCredential(credential);
  RunUntilIdle();

  EXPECT_THAT(presenter().GetSavedCredentials(), IsEmpty());

  presenter().UndoLastRemoval();
  RunUntilIdle();
  EXPECT_THAT(presenter().GetSavedCredentials(), ElementsAre(credential));
}

namespace {

class SavedPasswordsPresenterInitializationTest : public ::testing::Test {
 protected:
  SavedPasswordsPresenterInitializationTest() {
    profile_store_ = base::MakeRefCounted<PasswordStore>(
        std::make_unique<FakePasswordStoreBackend>(
            IsAccountStore(false), profile_store_backend_runner()));
    profile_store_->Init(/*prefs=*/nullptr,
                         /*affiliated_match_helper=*/nullptr);

    account_store_ = base::MakeRefCounted<PasswordStore>(
        std::make_unique<FakePasswordStoreBackend>(
            IsAccountStore(true), account_store_backend_runner()));
    account_store_->Init(/*prefs=*/nullptr,
                         /*affiliated_match_helper=*/nullptr);
  }

  ~SavedPasswordsPresenterInitializationTest() override {
    account_store_->ShutdownOnUIThread();
    profile_store_->ShutdownOnUIThread();

    ProcessBackendTasks(account_store_backend_runner());
    ProcessBackendTasks(profile_store_backend_runner());
  }

  void ProcessBackendTasks(scoped_refptr<base::TestMockTimeTaskRunner> runner) {
    runner->RunUntilIdle();
    task_env_.RunUntilIdle();
  }

  scoped_refptr<PasswordStore> profile_store() { return profile_store_; }
  scoped_refptr<PasswordStore> account_store() { return account_store_; }
  MockAffiliationService& affiliation_service() { return affiliation_service_; }

  const scoped_refptr<base::TestMockTimeTaskRunner>&
  profile_store_backend_runner() {
    return profile_store_backend_runner_;
  }
  const scoped_refptr<base::TestMockTimeTaskRunner>&
  account_store_backend_runner() {
    return account_store_backend_runner_;
  }

 private:
  // `TestMockTimeTaskRunner` is used to simulate different response times
  // between stores.
  base::test::SingleThreadTaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<base::TestMockTimeTaskRunner> profile_store_backend_runner_ =
      base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  scoped_refptr<base::TestMockTimeTaskRunner> account_store_backend_runner_ =
      base::MakeRefCounted<base::TestMockTimeTaskRunner>();

  MockAffiliationService affiliation_service_;
  scoped_refptr<PasswordStore> profile_store_ = nullptr;
  scoped_refptr<PasswordStore> account_store_ = nullptr;
};

}  // namespace

TEST_F(SavedPasswordsPresenterInitializationTest, InitWithTwoStores) {
  SavedPasswordsPresenter presenter{&affiliation_service(), profile_store(),
                                    account_store()};

  // As long as no `Init` is called, there are no pending requests.
  EXPECT_FALSE(presenter.IsWaitingForPasswordStore());

  presenter.Init();
  EXPECT_TRUE(presenter.IsWaitingForPasswordStore());
  ProcessBackendTasks(profile_store_backend_runner());
  EXPECT_TRUE(presenter.IsWaitingForPasswordStore());
  ProcessBackendTasks(account_store_backend_runner());
  EXPECT_FALSE(presenter.IsWaitingForPasswordStore());
}

TEST_F(SavedPasswordsPresenterInitializationTest, InitWithOneStore) {
  SavedPasswordsPresenter presenter{&affiliation_service(), profile_store(),
                                    nullptr};

  EXPECT_FALSE(presenter.IsWaitingForPasswordStore());

  presenter.Init();
  EXPECT_TRUE(presenter.IsWaitingForPasswordStore());
  ProcessBackendTasks(profile_store_backend_runner());
  EXPECT_FALSE(presenter.IsWaitingForPasswordStore());
}

TEST_F(SavedPasswordsPresenterInitializationTest, PendingUpdatesProfileStore) {
  SavedPasswordsPresenter presenter{&affiliation_service(), profile_store(),
                                    account_store()};
  presenter.Init();
  EXPECT_TRUE(presenter.IsWaitingForPasswordStore());
  ProcessBackendTasks(profile_store_backend_runner());
  EXPECT_TRUE(presenter.IsWaitingForPasswordStore());
  ProcessBackendTasks(account_store_backend_runner());
  EXPECT_FALSE(presenter.IsWaitingForPasswordStore());
}

TEST_F(SavedPasswordsPresenterInitializationTest, PendingUpdatesAccountStore) {
  SavedPasswordsPresenter presenter{&affiliation_service(), profile_store(),
                                    account_store()};
  presenter.Init();
  EXPECT_TRUE(presenter.IsWaitingForPasswordStore());
  ProcessBackendTasks(account_store_backend_runner());
  EXPECT_TRUE(presenter.IsWaitingForPasswordStore());
  ProcessBackendTasks(profile_store_backend_runner());
  EXPECT_FALSE(presenter.IsWaitingForPasswordStore());
}

}  // namespace password_manager
