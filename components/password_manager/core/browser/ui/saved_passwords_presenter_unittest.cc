// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"

#include <array>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/rand_util.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/affiliations/core/browser/mock_affiliation_service.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/fake_password_store_backend.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_ANDROID)
// components/webauthn/core is a desktop-only dependency of
// components/password_manager/core. gn cannot parse the preprocessor directive
// above when checking includes, so we need nogncheck here.
#include "components/webauthn/core/browser/test_passkey_model.h"  // nogncheck
#endif

namespace password_manager {

namespace {

using affiliations::Facet;
using affiliations::FacetURI;
using affiliations::FakeAffiliationService;
using ::testing::_;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

struct MockSavedPasswordsPresenterObserver : SavedPasswordsPresenter::Observer {
  MOCK_METHOD(void, OnEdited, (const CredentialUIEntry&), (override));
  MOCK_METHOD(void,
              OnSavedPasswordsChanged,
              (const PasswordStoreChangeList& changes),
              (override));
};

using StrictMockSavedPasswordsPresenterObserver =
    ::testing::StrictMock<MockSavedPasswordsPresenterObserver>;

#if !BUILDFLAG(IS_ANDROID)
constexpr char kPasskeyCredentialId[] = "abcd";
constexpr char kPasskeyRPID[] = "passkeys.com";
constexpr char kPasskeyUserId[] = "1234";
constexpr char kPasskeyUsername[] = "hmiku";
constexpr char kPasskeyUserDisplayName[] = "Hatsune Miku";
constexpr char kPasskeyFacet[] = "https://passkeys.com";

sync_pb::WebauthnCredentialSpecifics CreateTestPasskey() {
  sync_pb::WebauthnCredentialSpecifics credential;
  credential.set_sync_id(base::RandBytesAsString(16));
  credential.set_credential_id(kPasskeyCredentialId);
  credential.set_rp_id(kPasskeyRPID);
  credential.set_user_id(kPasskeyUserId);
  credential.set_user_name(kPasskeyUsername);
  credential.set_user_display_name(kPasskeyUserDisplayName);
  return credential;
}

CredentialUIEntry AsCredentialUIEntry(
    sync_pb::WebauthnCredentialSpecifics passkey) {
  return CredentialUIEntry(
      PasskeyCredential::FromCredentialSpecifics(std::array{std::move(passkey)})
          .at(0));
}
#endif

class SavedPasswordsPresenterTest : public testing::Test {
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
#if !BUILDFLAG(IS_ANDROID)
  webauthn::TestPasskeyModel& passkey_store() { return test_passkey_store_; }
#endif
  SavedPasswordsPresenter& presenter() { return presenter_; }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }
  void AdvanceClock(base::TimeDelta time) { task_env_.AdvanceClock(time); }

  constexpr bool IsGroupingEnabled() {
#if BUILDFLAG(IS_ANDROID)
    return false;
#else
    return true;
#endif
  }

 private:
  base::test::SingleThreadTaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<TestPasswordStore> store_ =
      base::MakeRefCounted<TestPasswordStore>();
  FakeAffiliationService affiliation_service_;
#if !BUILDFLAG(IS_ANDROID)
  webauthn::TestPasskeyModel test_passkey_store_;
  SavedPasswordsPresenter presenter_{&affiliation_service_, store_,
                                     /*account_store=*/nullptr,
                                     &test_passkey_store_};
#else
  SavedPasswordsPresenter presenter_{&affiliation_service_, store_,
                                     /*account_store=*/nullptr};
#endif
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

password_manager::PasswordForm CreateTestBlockedSiteForm(
    password_manager::PasswordForm::Store store,
    int index = 0) {
  PasswordForm form;
  form.url = GURL("https://blockedsite" + base::NumberToString(index) + ".com");
  form.blocked_by_user = true;
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
  store().RemoveLogin(FROM_HERE, form);
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
      url::SchemeHostPort(GURL("https://example.com"));

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

TEST_F(SavedPasswordsPresenterTest, AddPasswordFailWhenInvalidUrl) {
  StrictMockSavedPasswordsPresenterObserver observer;
  presenter().AddObserver(&observer);

  PasswordForm form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  form.url = GURL("https://^/invalid");

  EXPECT_CALL(observer, OnSavedPasswordsChanged).Times(0);
  EXPECT_FALSE(presenter().AddCredential(CredentialUIEntry(form)));
  RunUntilIdle();
  EXPECT_TRUE(store().IsEmpty());

  form.url = GURL("withoutscheme.com");
  EXPECT_CALL(observer, OnSavedPasswordsChanged).Times(0);
  EXPECT_FALSE(presenter().AddCredential(CredentialUIEntry(form)));
  RunUntilIdle();
  EXPECT_TRUE(store().IsEmpty());

  presenter().RemoveObserver(&observer);
}

TEST_F(SavedPasswordsPresenterTest, AddPasswordFailWhenEmptyPassword) {
  StrictMockSavedPasswordsPresenterObserver observer;
  presenter().AddObserver(&observer);

  PasswordForm form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  form.password_value = u"";

  EXPECT_CALL(observer, OnSavedPasswordsChanged).Times(0);
  EXPECT_FALSE(presenter().AddCredential(CredentialUIEntry(form)));
  RunUntilIdle();
  EXPECT_TRUE(store().IsEmpty());

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

  if (IsGroupingEnabled()) {
    ASSERT_THAT(presenter().GetBlockedSites(),
                ElementsAre(CredentialUIEntry(blocked_form)));
  } else {
    ASSERT_THAT(presenter().GetSavedCredentials(),
                ElementsAre(CredentialUIEntry(blocked_form)));
  }

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
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false),
                          TriggerBackendNotification(false))}};

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

TEST_F(SavedPasswordsPresenterTest, EditOnlyUsername) {
  PasswordForm form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  // Make sure the form has some issues and expect that they are cleared
  // because of the username change.
  form.password_issues = {
      {InsecureType::kLeaked,
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false),
                          TriggerBackendNotification(false))}};

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

  presenter().RemoveObserver(&observer);
}

TEST_F(SavedPasswordsPresenterTest, EditOnlyUsernameClearsPartialIssues) {
  PasswordForm form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  // Make sure the form has some issues and expect that only phished and leaked
  // are cleared because of the username change.
  form.password_issues = {
      {InsecureType::kLeaked,
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false),
                          TriggerBackendNotification(false))},
      {InsecureType::kPhished,
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false),
                          TriggerBackendNotification(false))},
      {InsecureType::kReused,
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false),
                          TriggerBackendNotification(false))},
      {InsecureType::kWeak,
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false),
                          TriggerBackendNotification(false))}};

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
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false),
                          TriggerBackendNotification(false))},
      {InsecureType::kWeak,
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false),
                          TriggerBackendNotification(false))}};

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

TEST_F(SavedPasswordsPresenterTest, EditOnlyPassword) {
  PasswordForm form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  // Make sure the form has some issues and expect that they are cleared
  // because of the password change.
  form.password_issues = {
      {InsecureType::kLeaked,
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false),
                          TriggerBackendNotification(false))}};

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
}

TEST_F(SavedPasswordsPresenterTest, EditingNotesShouldNotResetPasswordIssues) {
  PasswordForm form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);

  form.password_issues.insert(
      {InsecureType::kLeaked,
       InsecurityMetadata(base::Time(), IsMuted(false),
                          TriggerBackendNotification(true))});

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
}

TEST_F(SavedPasswordsPresenterTest, EditNoteAsEmpty) {
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

TEST_F(SavedPasswordsPresenterTest, EditUsernameAndPassword) {
  PasswordForm form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  // Make sure the form has some issues and expect that they are cleared
  // because of the username and password change.
  form.password_issues = {
      {InsecureType::kLeaked,
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false),
                          TriggerBackendNotification(false))}};

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
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false),
                          TriggerBackendNotification(false))}};

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
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false),
                          TriggerBackendNotification(false))}};

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
      url::SchemeHostPort(GURL(u"federatedOrigin.com"));
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

  if (IsGroupingEnabled()) {
    EXPECT_THAT(presenter().GetSavedCredentials(),
                UnorderedElementsAre(CredentialUIEntry(form),
                                     CredentialUIEntry(federated_form)));
    EXPECT_THAT(presenter().GetBlockedSites(),
                UnorderedElementsAre(CredentialUIEntry(blocked_form)));
  } else {
    EXPECT_THAT(presenter().GetSavedCredentials(),
                UnorderedElementsAre(CredentialUIEntry(form),
                                     CredentialUIEntry(blocked_form),
                                     CredentialUIEntry(federated_form)));
  }
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(SavedPasswordsPresenterTest, GetSavedCredentialsWithPasskeys) {
  // Password grouping is required for passkey support.
  if (!IsGroupingEnabled()) {
    return;
  }
  PasswordForm form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);

  PasswordForm blocked_form;
  blocked_form.signon_realm = form.signon_realm;
  blocked_form.blocked_by_user = true;
  blocked_form.in_store = PasswordForm::Store::kProfileStore;

  PasswordForm federated_form;
  federated_form.url = GURL("https://federated.com");
  federated_form.signon_realm = "federation://federated.com/idp.com";
  federated_form.username_value = u"example@gmail.com";
  federated_form.federation_origin =
      url::SchemeHostPort(GURL("federation-origin.com"));
  federated_form.in_store = PasswordForm::Store::kProfileStore;

  sync_pb::WebauthnCredentialSpecifics passkey = CreateTestPasskey();
  passkey_store().AddNewPasskeyForTesting(passkey);

  store().AddLogin(form);
  store().AddLogin(blocked_form);
  store().AddLogin(federated_form);
  RunUntilIdle();

  ASSERT_THAT(
      store().stored_passwords(),
      UnorderedElementsAre(
          Pair(form.signon_realm, UnorderedElementsAre(form, blocked_form)),
          Pair(federated_form.signon_realm, ElementsAre(federated_form))));

  // GetAllSavedCredentials should return all credentials.
  EXPECT_THAT(presenter().GetSavedCredentials(),
              UnorderedElementsAre(CredentialUIEntry(form),
                                   CredentialUIEntry(federated_form),
                                   AsCredentialUIEntry(std::move(passkey))));
}

TEST_F(SavedPasswordsPresenterTest, GetAffiliatedGroupsWithPasskeys) {
  // Password grouping is required for passkey support.
  if (!IsGroupingEnabled()) {
    return;
  }

  affiliations::MockAffiliationService mock_affiliation_service;
  SavedPasswordsPresenter presenter{&mock_affiliation_service, &store(),
                                    nullptr, &passkey_store()};
  presenter.Init();
  RunUntilIdle();

  PasswordForm form1 =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  PasswordForm form2 =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore, 1);
  PasswordForm form3 =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore, 2);

  sync_pb::WebauthnCredentialSpecifics passkey = CreateTestPasskey();
  passkey_store().AddNewPasskeyForTesting(passkey);

  store().AddLogin(form1);
  store().AddLogin(form2);
  store().AddLogin(form3);

  std::vector<affiliations::GroupedFacets> grouped_facets(2);
  grouped_facets[0].facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec(form1.signon_realm)),
      Facet(FacetURI::FromPotentiallyInvalidSpec(form2.signon_realm)),
      Facet(FacetURI::FromPotentiallyInvalidSpec(kPasskeyFacet))};
  grouped_facets[0].branding_info.name = "Group 1";
  grouped_facets[0].branding_info.icon_url =
      GURL("https://test1.com/favicon.ico");
  grouped_facets[1].facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec(form3.signon_realm))};
  grouped_facets[1].branding_info.name = "Group 2";
  grouped_facets[1].branding_info.icon_url =
      GURL("https://test3.com/favicon.ico");
  EXPECT_CALL(mock_affiliation_service, GetGroupingInfo)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(grouped_facets));
  RunUntilIdle();

  CredentialUIEntry credential1(form1), credential2(form2), credential3(form3);
  EXPECT_THAT(
      presenter.GetAffiliatedGroups(),
      UnorderedElementsAre(
          AffiliatedGroup({credential1, credential2,
                           AsCredentialUIEntry(std::move(passkey))},
                          grouped_facets[0].branding_info),
          AffiliatedGroup({credential3}, grouped_facets[1].branding_info)));
}

TEST_F(SavedPasswordsPresenterTest, DeletePasskey) {
  // Password grouping is required for passkey support.
  if (!IsGroupingEnabled()) {
    return;
  }
  sync_pb::WebauthnCredentialSpecifics passkey = CreateTestPasskey();
  passkey_store().AddNewPasskeyForTesting(passkey);
  RunUntilIdle();

  std::vector<CredentialUIEntry> passkeys = presenter().GetSavedCredentials();
  ASSERT_EQ(passkeys.size(), 1u);
  ASSERT_FALSE(passkeys.at(0).passkey_credential_id.empty());

  MockSavedPasswordsPresenterObserver observer;
  presenter().AddObserver(&observer);
  EXPECT_CALL(observer, OnSavedPasswordsChanged);
  presenter().RemoveCredential(passkeys.at(0));
  RunUntilIdle();

  EXPECT_TRUE(presenter().GetSavedCredentials().empty());
  presenter().RemoveObserver(&observer);
}

TEST_F(SavedPasswordsPresenterTest, NotifyPasskeyAdded) {
  // Password grouping is required for passkey support.
  if (!IsGroupingEnabled()) {
    return;
  }
  RunUntilIdle();

  std::vector<CredentialUIEntry> passkeys = presenter().GetSavedCredentials();
  ASSERT_TRUE(passkeys.empty());

  MockSavedPasswordsPresenterObserver observer;
  presenter().AddObserver(&observer);
  EXPECT_CALL(observer, OnSavedPasswordsChanged);
  sync_pb::WebauthnCredentialSpecifics passkey = CreateTestPasskey();
  passkey_store().AddNewPasskeyForTesting(passkey);
  RunUntilIdle();

  EXPECT_EQ(presenter().GetSavedCredentials().size(), 1u);
  presenter().RemoveObserver(&observer);
}

TEST_F(SavedPasswordsPresenterTest, EditPasskey) {
  // Password grouping is required for passkey support.
  if (!IsGroupingEnabled()) {
    return;
  }
  passkey_store().AddNewPasskeyForTesting(CreateTestPasskey());
  RunUntilIdle();

  std::vector<CredentialUIEntry> passkeys = presenter().GetSavedCredentials();
  ASSERT_EQ(passkeys.size(), 1u);
  CredentialUIEntry& original_passkey = passkeys.at(0);
  CredentialUIEntry updated_passkey = original_passkey;
  updated_passkey.username = u"anya";
  updated_passkey.user_display_name = u"Anya Forger";

  MockSavedPasswordsPresenterObserver observer;
  presenter().AddObserver(&observer);
  EXPECT_CALL(observer, OnSavedPasswordsChanged);
  EXPECT_EQ(presenter().EditSavedCredentials(original_passkey, updated_passkey),
            SavedPasswordsPresenter::EditResult::kSuccess);
  RunUntilIdle();
  passkeys = presenter().GetSavedCredentials();
  ASSERT_EQ(passkeys.size(), 1u);
  EXPECT_EQ(passkeys.at(0).username, u"anya");
  EXPECT_EQ(passkeys.at(0).user_display_name, u"Anya Forger");
  presenter().RemoveObserver(&observer);
}

TEST_F(SavedPasswordsPresenterTest, EditPasskeyNoChanges) {
  // Password grouping is required for passkey support.
  if (!IsGroupingEnabled()) {
    return;
  }
  passkey_store().AddNewPasskeyForTesting(CreateTestPasskey());
  RunUntilIdle();

  std::vector<CredentialUIEntry> passkeys = presenter().GetSavedCredentials();
  ASSERT_EQ(passkeys.size(), 1u);
  CredentialUIEntry& original_passkey = passkeys.at(0);
  CredentialUIEntry updated_passkey = original_passkey;

  MockSavedPasswordsPresenterObserver observer;
  presenter().AddObserver(&observer);
  EXPECT_CALL(observer, OnSavedPasswordsChanged).Times(0);
  EXPECT_EQ(presenter().EditSavedCredentials(original_passkey, updated_passkey),
            SavedPasswordsPresenter::EditResult::kNothingChanged);
  RunUntilIdle();
  presenter().RemoveObserver(&observer);
}

TEST_F(SavedPasswordsPresenterTest, EditPasskeyNotFound) {
  // Password grouping is required for passkey support.
  if (!IsGroupingEnabled()) {
    return;
  }
  passkey_store().AddNewPasskeyForTesting(CreateTestPasskey());
  RunUntilIdle();

  std::vector<CredentialUIEntry> passkeys = presenter().GetSavedCredentials();
  ASSERT_EQ(passkeys.size(), 1u);
  CredentialUIEntry& original_passkey = passkeys.at(0);
  CredentialUIEntry updated_passkey = original_passkey;
  updated_passkey.username = u"anya";
  updated_passkey.passkey_credential_id = {1, 2, 3, 4};
  ASSERT_NE(original_passkey.passkey_credential_id,
            updated_passkey.passkey_credential_id);

  MockSavedPasswordsPresenterObserver observer;
  presenter().AddObserver(&observer);
  EXPECT_CALL(observer, OnSavedPasswordsChanged).Times(0);
  EXPECT_EQ(presenter().EditSavedCredentials(original_passkey, updated_passkey),
            SavedPasswordsPresenter::EditResult::kNotFound);
  RunUntilIdle();
  presenter().RemoveObserver(&observer);
}

TEST_F(SavedPasswordsPresenterTest, DeleteAllDataWithPasskey) {
  // Password grouping is required for passkey support.
  if (!IsGroupingEnabled()) {
    return;
  }
  sync_pb::WebauthnCredentialSpecifics passkey = CreateTestPasskey();
  passkey_store().AddNewPasskeyForTesting(passkey);
  RunUntilIdle();

  PasswordForm form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  PasswordForm blocked_form =
      CreateTestBlockedSiteForm(PasswordForm::Store::kProfileStore);

  store().AddLogins({form, blocked_form});
  RunUntilIdle();

  base::MockCallback<base::OnceCallback<void(bool)>> completion_callback;
  EXPECT_CALL(completion_callback, Run(true)).Times(1);
  presenter().DeleteAllData(completion_callback.Get());
  RunUntilIdle();
  EXPECT_TRUE(passkey_store().GetAllPasskeys().empty());
  EXPECT_TRUE(store().IsEmpty());
}

#endif

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

TEST_F(SavedPasswordsPresenterTest, DeleteAllData) {
  PasswordForm form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  PasswordForm blocked_form =
      CreateTestBlockedSiteForm(PasswordForm::Store::kProfileStore);

  store().AddLogins({form, blocked_form});
  RunUntilIdle();

  EXPECT_EQ(store().stored_passwords().size(), 2u);

  base::MockCallback<base::OnceCallback<void(bool)>> completion_callback;
  EXPECT_CALL(completion_callback, Run(true)).Times(1);
  presenter().DeleteAllData(completion_callback.Get());
  RunUntilIdle();
  EXPECT_TRUE(store().IsEmpty());
}

TEST_F(SavedPasswordsPresenterTest, DeleteAllDataEmpty) {
  base::MockCallback<base::OnceCallback<void(bool)>> completion_callback;
  EXPECT_CALL(completion_callback, Run(true)).Times(1);
  presenter().DeleteAllData(completion_callback.Get());
  RunUntilIdle();
  EXPECT_TRUE(store().IsEmpty());
}

namespace {

class SavedPasswordsPresenterWithTwoStoresTest : public testing::Test {
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

  constexpr bool IsGroupingEnabled() {
#if BUILDFLAG(IS_ANDROID)
    return false;
#else
    return true;
#endif
  }

 private:
  base::test::SingleThreadTaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<TestPasswordStore> profile_store_ =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  scoped_refptr<TestPasswordStore> account_store_ =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(true));
  FakeAffiliationService affiliation_service_;
  SavedPasswordsPresenter presenter_{&affiliation_service_, profile_store_,
                                     account_store_};
};

}  // namespace

TEST_F(SavedPasswordsPresenterWithTwoStoresTest,
       DeleteAllDataFromTwoStoresEmpty) {
  base::MockCallback<base::OnceCallback<void(bool)>> completion_callback;
  EXPECT_CALL(completion_callback, Run(true)).Times(1);
  presenter().DeleteAllData(completion_callback.Get());
  RunUntilIdle();
  EXPECT_TRUE(account_store().IsEmpty());
  EXPECT_TRUE(profile_store().IsEmpty());
}

TEST_F(SavedPasswordsPresenterWithTwoStoresTest, DeleteAllDataFromTwoStores) {
  PasswordForm profile_store_form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore, 1);
  PasswordForm blocked_form =
      CreateTestBlockedSiteForm(PasswordForm::Store::kProfileStore);

  PasswordForm account_store_form =
      CreateTestPasswordForm(PasswordForm::Store::kAccountStore, 2);

  profile_store().AddLogins({profile_store_form, blocked_form});
  account_store().AddLogins({account_store_form});
  RunUntilIdle();

  EXPECT_EQ(profile_store().stored_passwords().size(), 2u);
  EXPECT_EQ(account_store().stored_passwords().size(), 1u);

  base::MockCallback<base::OnceCallback<void(bool)>> completion_callback;
  EXPECT_CALL(completion_callback, Run(true)).Times(1);
  presenter().DeleteAllData(completion_callback.Get());
  RunUntilIdle();
  EXPECT_TRUE(account_store().IsEmpty());
  EXPECT_TRUE(profile_store().IsEmpty());
}

TEST_F(SavedPasswordsPresenterWithTwoStoresTest,
       DeleteAllDataAccountStoreNotEmpty) {
  PasswordForm account_store_form =
      CreateTestPasswordForm(PasswordForm::Store::kAccountStore);
  PasswordForm blocked_form =
      CreateTestBlockedSiteForm(PasswordForm::Store::kAccountStore);

  account_store().AddLogins({account_store_form, blocked_form});
  RunUntilIdle();

  EXPECT_EQ(account_store().stored_passwords().size(), 2u);

  base::MockCallback<base::OnceCallback<void(bool)>> completion_callback;
  EXPECT_CALL(completion_callback, Run(true)).Times(1);
  presenter().DeleteAllData(completion_callback.Get());
  RunUntilIdle();
  EXPECT_TRUE(account_store().IsEmpty());
  EXPECT_TRUE(profile_store().IsEmpty());
}

TEST_F(SavedPasswordsPresenterWithTwoStoresTest,
       DeleteAllDataProfileStoreNotEmpty) {
  PasswordForm profile_store_form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  PasswordForm blocked_form =
      CreateTestBlockedSiteForm(PasswordForm::Store::kProfileStore);

  profile_store().AddLogins({profile_store_form, blocked_form});
  RunUntilIdle();

  EXPECT_EQ(profile_store().stored_passwords().size(), 2u);

  base::MockCallback<base::OnceCallback<void(bool)>> completion_callback;
  EXPECT_CALL(completion_callback, Run(true)).Times(1);
  presenter().DeleteAllData(completion_callback.Get());
  RunUntilIdle();
  EXPECT_TRUE(account_store().IsEmpty());
  EXPECT_TRUE(profile_store().IsEmpty());
}

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
  profile_store().RemoveLogin(FROM_HERE, profile_store_form);
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
  EXPECT_CALL(completion_callback, Run).Times(1);
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
  EXPECT_CALL(completion_callback, Run).Times(1);
  RunUntilIdle();
  presenter().RemoveObserver(&observer);
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

  EXPECT_CALL(observer, OnSavedPasswordsChanged).Times(1);

  CredentialUIEntry account_store_cred_1(account_store_form_1);
  CredentialUIEntry account_store_cred_2(account_store_form_2);

  presenter().AddCredentials({account_store_cred_1, account_store_cred_2},
                             password_manager::PasswordForm::Type::kImported,
                             completion_callback.Get());
  EXPECT_CALL(completion_callback, Run).Times(1);
  RunUntilIdle();

  // Call RunUntilIdle again to await when SavedPasswordsPresenter obtain all
  // the logins.
  RunUntilIdle();
  EXPECT_THAT(presenter().GetSavedCredentials(),
              UnorderedElementsAre(account_store_cred_1, account_store_cred_2));
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

  // Add a password with note.
  EXPECT_CALL(observer, OnSavedPasswordsChanged);
  EXPECT_TRUE(presenter().AddCredential(CredentialUIEntry(form2)));
  RunUntilIdle();
  EXPECT_THAT(
      profile_store().stored_passwords(),
      UnorderedElementsAre(Pair(form.signon_realm, ElementsAre(form)),
                           Pair(form2.signon_realm, ElementsAre(form2))));

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

TEST_F(SavedPasswordsPresenterWithTwoStoresTest, UpdatePasswordForms) {
  PasswordForm account_store_form_1 =
      CreateTestPasswordForm(PasswordForm::Store::kAccountStore, /*index=*/0);
  PasswordForm account_store_form_2 =
      CreateTestPasswordForm(PasswordForm::Store::kAccountStore, /*index=*/1);

  std::vector<CredentialUIEntry> credentials_to_add = {
      CredentialUIEntry(account_store_form_1),
      CredentialUIEntry(account_store_form_2)};

  presenter().AddCredentials(credentials_to_add,
                             password_manager::PasswordForm::Type::kImported,
                             base::DoNothing());

  RunUntilIdle();
  EXPECT_THAT(presenter().GetSavedCredentials(),
              testing::UnorderedElementsAreArray(credentials_to_add));

  account_store_form_1.password_value = u"new_password_1";
  account_store_form_2.password_value = u"new_password_2";

  presenter().UpdatePasswordForms({account_store_form_1, account_store_form_2});
  RunUntilIdle();

  ASSERT_THAT(presenter().GetSavedCredentials(),
              UnorderedElementsAre(CredentialUIEntry(account_store_form_1),
                                   CredentialUIEntry(account_store_form_2)));
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

  if (IsGroupingEnabled()) {
    ASSERT_THAT(presenter().GetBlockedSites(),
                ElementsAre(CredentialUIEntry(blocked_form)));
  } else {
    ASSERT_THAT(presenter().GetSavedCredentials(),
                ElementsAre(CredentialUIEntry(blocked_form)));
  }

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
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false),
                          TriggerBackendNotification(false))}};

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

// Tests whether editing passwords in a credential group modify them properly.
TEST_F(SavedPasswordsPresenterTest, EditPasswordsInCredentialGroup) {
  if (!IsGroupingEnabled()) {
    return;
  }

  PasswordForm form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  PasswordForm form2 =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  form2.url = GURL("https://m.test0.com");
  form2.signon_realm = form2.url.spec();

  store().AddLogin(form);
  store().AddLogin(form2);

  RunUntilIdle();

  std::vector<PasswordForm> original_forms = {form, form2};
  CredentialUIEntry original_credential(original_forms);

  // Prepare updated credential.
  const std::u16string new_password = u"new_password";
  CredentialUIEntry updated_credential(original_forms);
  updated_credential.password = new_password;

  // Expect successful passwords editing.
  EXPECT_EQ(SavedPasswordsPresenter::EditResult::kSuccess,
            presenter().EditSavedCredentials(CredentialUIEntry({form, form2}),
                                             updated_credential));
  base::Time date_password_modified = base::Time::Now();
  RunUntilIdle();

  // Prepare expected updated forms.
  PasswordForm updated1 = form;
  updated1.password_value = new_password;
  updated1.date_password_modified = date_password_modified;
  PasswordForm updated2 = form2;
  updated2.password_value = new_password;
  updated2.date_password_modified = date_password_modified;

  EXPECT_THAT(store().stored_passwords(),
              UnorderedElementsAre(
                  Pair(form.signon_realm, UnorderedElementsAre(updated1)),
                  Pair(form2.signon_realm, ElementsAre(updated2))));
}

// Tests whether deleting passwords in a credential group works properly.
TEST_F(SavedPasswordsPresenterTest, DeletePasswordsInCredentialGroup) {
  if (!IsGroupingEnabled()) {
    return;
  }

  PasswordForm form =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  PasswordForm form2 =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  form2.url = GURL("https://m.test0.com");
  form2.signon_realm = form2.url.spec();

  store().AddLogin(form);
  store().AddLogin(form2);

  RunUntilIdle();

  presenter().GetSavedCredentials();

  // Delete credential with multiple facets.
  presenter().RemoveCredential(CredentialUIEntry({form, form2}));
  RunUntilIdle();

  EXPECT_TRUE(store().IsEmpty());
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
  if (!IsGroupingEnabled()) {
    return;
  }

  base::HistogramTester histogram_tester;
  affiliations::MockAffiliationService mock_affiliation_service;
  SavedPasswordsPresenter presenter{&mock_affiliation_service, &store(),
                                    nullptr, /*passkey_store=*/nullptr};
  presenter.Init();
  RunUntilIdle();

  PasswordForm form1 =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore);
  PasswordForm form2 =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore, 1);
  PasswordForm form3 =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore, 2);

  PasswordForm blocked_form;
  blocked_form.signon_realm = form1.signon_realm;
  blocked_form.blocked_by_user = true;
  blocked_form.in_store = PasswordForm::Store::kProfileStore;

  store().AddLogins({form1, form2, form3, blocked_form});

  std::vector<affiliations::GroupedFacets> grouped_facets(2);
  grouped_facets[0].facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec(form1.signon_realm)),
      Facet(FacetURI::FromPotentiallyInvalidSpec(form2.signon_realm))};
  grouped_facets[0].branding_info.name = "Form 1";
  grouped_facets[0].branding_info.icon_url =
      GURL("https://test1.com/favicon.ico");
  grouped_facets[1].facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec(form3.signon_realm))};
  grouped_facets[1].branding_info.name = "Form 3";
  grouped_facets[1].branding_info.icon_url =
      GURL("https://test3.com/favicon.ico");

  affiliations::AffiliationService::GroupsCallback callback;
  EXPECT_CALL(mock_affiliation_service, GetGroupingInfo)
      .WillOnce(MoveArg<1>(&callback));
  RunUntilIdle();

  const int kDelay = 23;
  AdvanceClock(base::Milliseconds(kDelay));
  std::move(callback).Run(grouped_facets);

  CredentialUIEntry credential1(form1), credential2(form2), credential3(form3);
  EXPECT_THAT(
      presenter.GetAffiliatedGroups(),
      UnorderedElementsAre(
          AffiliatedGroup({credential1, credential2},
                          grouped_facets[0].branding_info),
          AffiliatedGroup({credential3}, grouped_facets[1].branding_info)));
  EXPECT_THAT(presenter.GetBlockedSites(),
              ElementsAre(CredentialUIEntry(blocked_form)));

  histogram_tester.ExpectTimeBucketCount(
      "PasswordManager.PasswordsGrouping.Time", base::Milliseconds(kDelay), 1);
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
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false),
                          TriggerBackendNotification(false))},
      {InsecureType::kReused,
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false),
                          TriggerBackendNotification(false))},
      {InsecureType::kWeak,
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false),
                          TriggerBackendNotification(false))},
      {InsecureType::kPhished,
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false),
                          TriggerBackendNotification(false))}};

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
  FakeAffiliationService& affiliation_service() { return affiliation_service_; }

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

  FakeAffiliationService affiliation_service_;
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
                                    /*account_store=*/nullptr};

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

namespace {

class SavedPasswordsPresenterMoveToAccountTest : public testing::Test {
 protected:
  ~SavedPasswordsPresenterMoveToAccountTest() override = default;

  MockPasswordStoreInterface* profile_store() { return profile_store_.get(); }
  MockPasswordStoreInterface* account_store() { return account_store_.get(); }
  SavedPasswordsPresenter& presenter() { return presenter_; }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }

 private:
  base::test::SingleThreadTaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<MockPasswordStoreInterface> profile_store_ =
      base::MakeRefCounted<testing::NiceMock<MockPasswordStoreInterface>>();
  scoped_refptr<MockPasswordStoreInterface> account_store_ =
      base::MakeRefCounted<testing::NiceMock<MockPasswordStoreInterface>>();
  FakeAffiliationService affiliation_service_;
  SavedPasswordsPresenter presenter_{&affiliation_service_, profile_store_,
                                     account_store_};
};

}  // namespace

TEST_F(SavedPasswordsPresenterMoveToAccountTest, MovesToAccount) {
  PasswordForm form_1 =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore, 1);
  PasswordForm form_2 =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore, 2);

  std::vector<CredentialUIEntry> credentials;
  credentials.emplace_back(form_1);
  credentials.emplace_back(form_2);

  std::vector<PasswordForm> forms;
  forms.push_back(form_1);
  forms.push_back(form_2);

  presenter().Init();
  static_cast<PasswordStoreConsumer*>(&presenter())
      ->OnGetPasswordStoreResultsOrErrorFrom(profile_store(), std::move(forms));
  static_cast<PasswordStoreConsumer*>(&presenter())
      ->OnGetPasswordStoreResultsOrErrorFrom(account_store(), {});
  RunUntilIdle();

  EXPECT_CALL(*account_store(), AddLogin(form_1, _));
  EXPECT_CALL(*account_store(), AddLogin(form_2, _));
  EXPECT_CALL(*profile_store(), RemoveLogin(_, form_1));
  EXPECT_CALL(*profile_store(), RemoveLogin(_, form_2));

  presenter().MoveCredentialsToAccount(
      credentials,
      metrics_util::MoveToAccountStoreTrigger::kExplicitlyTriggeredInSettings);
}

TEST_F(SavedPasswordsPresenterMoveToAccountTest,
       MovesToAccountSkipsExistingPasswordsOnAccount) {
  PasswordForm form_profile =
      CreateTestPasswordForm(PasswordForm::Store::kProfileStore, 1);
  PasswordForm form_account =
      CreateTestPasswordForm(PasswordForm::Store::kAccountStore, 1);

  std::vector<CredentialUIEntry> credentials;
  credentials.emplace_back(
      std::vector<PasswordForm>{form_profile, form_account});

  std::vector<PasswordForm> forms_from_profile;
  forms_from_profile.push_back(form_profile);

  std::vector<PasswordForm> forms_from_account;
  forms_from_account.push_back(form_account);

  presenter().Init();
  static_cast<PasswordStoreConsumer*>(&presenter())
      ->OnGetPasswordStoreResultsOrErrorFrom(profile_store(),
                                             std::move(forms_from_profile));
  RunUntilIdle();
  static_cast<PasswordStoreConsumer*>(&presenter())
      ->OnGetPasswordStoreResultsOrErrorFrom(account_store(),
                                             std::move(forms_from_account));
  RunUntilIdle();

  EXPECT_CALL(*account_store(), AddLogin).Times(0);
  EXPECT_CALL(*profile_store(), RemoveLogin(_, form_profile));

  presenter().MoveCredentialsToAccount(
      credentials,
      metrics_util::MoveToAccountStoreTrigger::kExplicitlyTriggeredInSettings);
}

}  // namespace password_manager
