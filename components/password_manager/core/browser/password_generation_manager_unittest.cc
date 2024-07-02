// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_generation_manager.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/form_saver_impl.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "password_form.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using base::ASCIIToUTF16;
using testing::_;
using testing::ElementsAre;
using testing::Field;
using testing::IsEmpty;
using testing::Key;
using testing::Pointee;

constexpr char kURL[] = "https://example.in/login";
constexpr char kSubdomainURL[] = "https://m.example.in/login";

// Creates a dummy saved credential.
PasswordForm CreateSaved() {
  PasswordForm form;
  form.url = GURL(kURL);
  form.signon_realm = form.url.spec();
  form.action = GURL("https://login.example.org");
  form.username_value = u"old_username";
  form.password_value = u"12345";
  form.in_store = PasswordForm::Store::kProfileStore;
  return form;
}

PasswordForm CreateSavedFederated() {
  PasswordForm federated;
  federated.url = GURL(kURL);
  federated.signon_realm = "federation://example.in/google.com";
  federated.type = PasswordForm::Type::kApi;
  federated.federation_origin =
      url::SchemeHostPort(GURL("https://google.com/"));
  federated.username_value = u"federated_username";
  return federated;
}

// Creates a dummy saved PSL credential.
PasswordForm CreateSavedPSL() {
  PasswordForm form;
  form.url = GURL(kSubdomainURL);
  form.signon_realm = form.url.spec();
  form.action = GURL("https://login.example.org");
  form.username_value = u"old_username2";
  form.password_value = u"passw0rd";
  form.match_type = PasswordForm::MatchType::kPSL;
  form.in_store = PasswordForm::Store::kProfileStore;
  return form;
}

// Creates a dummy generated password.
PasswordForm CreateGenerated() {
  PasswordForm form;
  form.url = GURL(kURL);
  form.signon_realm = form.url.spec();
  form.action = GURL("https://signup.example.org");
  form.username_value = u"MyName";
  form.password_value = u"Strong password";
  form.type = PasswordForm::Type::kGenerated;
  return form;
}

MATCHER_P(FormHasUniqueKey, key, "") {
  return ArePasswordFormUniqueKeysEqual(arg, key);
}

class MockPasswordManagerDriver : public StubPasswordManagerDriver {
 public:
  MOCK_METHOD(void,
              GeneratedPasswordAccepted,
              (const std::u16string&),
              (override));
  MOCK_METHOD(void, ClearPreviewedForm, (), (override));
};

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  bool PromptUserToSaveOrUpdatePassword(
      std::unique_ptr<PasswordFormManagerForUI> form_to_save,
      bool update_password) override;

  MOCK_METHOD(bool, PromptUserToSaveOrUpdatePasswordMock, (bool), ());

  std::unique_ptr<PasswordFormManagerForUI> MoveForm() {
    return std::move(form_to_save_);
  }

 private:
  std::unique_ptr<PasswordFormManagerForUI> form_to_save_;
};

bool MockPasswordManagerClient::PromptUserToSaveOrUpdatePassword(
    std::unique_ptr<PasswordFormManagerForUI> form_to_save,
    bool update_password) {
  form_to_save_ = std::move(form_to_save);
  return PromptUserToSaveOrUpdatePasswordMock(update_password);
}

class PasswordGenerationManagerTest : public testing::Test {
 public:
  PasswordGenerationManagerTest();
  ~PasswordGenerationManagerTest() override;

  MockPasswordStoreInterface& store() { return *mock_store_; }
  PasswordGenerationManager& manager() { return generation_manager_; }
  FormSaverImpl& form_saver() { return form_saver_; }
  MockPasswordManagerClient& client() { return client_; }

  // Immitates user accepting the password that can't be immediately presaved.
  // The returned value represents the UI model for the update bubble.
  std::unique_ptr<PasswordFormManagerForUI> SetUpOverwritingUI(
      base::WeakPtr<PasswordManagerDriver> driver);

  void ForwardByMinute();

  void SetAccountStoreEnabled(bool is_enabled);

 private:
  // For the MockPasswordStore.
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<MockPasswordStoreInterface> mock_store_;
  // Test with the real form saver for better robustness.
  FormSaverImpl form_saver_;

  MockPasswordManagerClient client_;
  PasswordGenerationManager generation_manager_;
};

PasswordGenerationManagerTest::PasswordGenerationManagerTest()
    : mock_store_(new testing::StrictMock<MockPasswordStoreInterface>()),
      form_saver_(mock_store_.get()),
      generation_manager_(&client_) {}

PasswordGenerationManagerTest::~PasswordGenerationManagerTest() {
  mock_store_->ShutdownOnUIThread();
}

std::unique_ptr<PasswordFormManagerForUI>
PasswordGenerationManagerTest::SetUpOverwritingUI(
    base::WeakPtr<PasswordManagerDriver> driver) {
  PasswordForm generated = CreateGenerated();
  PasswordForm saved = CreateSaved();
  generated.username_value = u"";
  saved.username_value = u"";
  const PasswordForm federated = CreateSavedFederated();
  FakeFormFetcher fetcher;
  fetcher.SetNonFederated({saved});
  fetcher.SetBestMatches({saved});
  fetcher.set_federated({federated});

  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordMock(true))
      .WillOnce(testing::Return(true));
  manager().GeneratedPasswordAccepted(
      std::move(generated), {&saved}, {&federated},
      PasswordForm::Store::kAccountStore, std::move(driver));
  return client_.MoveForm();
}

void PasswordGenerationManagerTest::ForwardByMinute() {
  task_environment_.FastForwardBy(base::Minutes(1));
}

void PasswordGenerationManagerTest::SetAccountStoreEnabled(bool is_enabled) {
  ON_CALL(*client().GetPasswordFeatureManager(), IsOptedInForAccountStorage())
      .WillByDefault(testing::Return(is_enabled));
}

// Check that accepting a generated password simply relays the message to the
// driver.
TEST_F(PasswordGenerationManagerTest, GeneratedPasswordAccepted_EmptyStore) {
  PasswordForm generated = CreateGenerated();
  MockPasswordManagerDriver driver;
  FakeFormFetcher fetcher;

  EXPECT_CALL(driver, GeneratedPasswordAccepted(generated.password_value));
  manager().GeneratedPasswordAccepted(std::move(generated), {}, {},
                                      PasswordForm::Store::kAccountStore,
                                      driver.AsWeakPtr());
  EXPECT_FALSE(manager().HasGeneratedPassword());
}

// In case of accepted password conflicts with an existing username the
// credential can be presaved with an empty one. Thus, no conflict happens and
// the driver should be notified directly.
TEST_F(PasswordGenerationManagerTest, GeneratedPasswordAccepted_Conflict) {
  PasswordForm generated = CreateGenerated();
  const PasswordForm saved = CreateSaved();
  generated.username_value = saved.username_value;
  MockPasswordManagerDriver driver;
  FakeFormFetcher fetcher;
  fetcher.SetNonFederated({saved});
  fetcher.SetBestMatches({saved});

  EXPECT_CALL(driver, GeneratedPasswordAccepted(generated.password_value));
  manager().GeneratedPasswordAccepted(std::move(generated), {&saved}, {},
                                      PasswordForm::Store::kAccountStore,
                                      driver.AsWeakPtr());
  EXPECT_FALSE(manager().HasGeneratedPassword());
}

TEST_F(PasswordGenerationManagerTest, GeneratedPasswordAccepted_UpdateUI) {
  MockPasswordManagerDriver driver;
  EXPECT_CALL(driver, GeneratedPasswordAccepted(_)).Times(0);
  std::unique_ptr<PasswordFormManagerForUI> ui_form =
      SetUpOverwritingUI(driver.AsWeakPtr());
  ASSERT_TRUE(ui_form);
  EXPECT_EQ(GURL(kURL), ui_form->GetURL());
  EXPECT_THAT(ui_form->GetBestMatches(),
              ElementsAre(Field(&PasswordForm::username_value, u"")));
  EXPECT_THAT(ui_form->GetFederatedMatches(),
              ElementsAre(CreateSavedFederated()));
  EXPECT_EQ(u"", ui_form->GetPendingCredentials().username_value);
  EXPECT_EQ(CreateGenerated().password_value,
            ui_form->GetPendingCredentials().password_value);
  EXPECT_THAT(ui_form->GetInteractionsStats(), IsEmpty());
  EXPECT_FALSE(ui_form->IsBlocklisted());
}

TEST_F(PasswordGenerationManagerTest,
       GeneratedPasswordAccepted_UpdateUIDismissed) {
  MockPasswordManagerDriver driver;
  EXPECT_CALL(driver, GeneratedPasswordAccepted).Times(0);
  std::unique_ptr<PasswordFormManagerForUI> ui_form =
      SetUpOverwritingUI(driver.AsWeakPtr());
  ASSERT_TRUE(ui_form);
  ui_form->OnNoInteraction(true);
}

TEST_F(PasswordGenerationManagerTest, GeneratedPasswordAccepted_UpdateUINope) {
  MockPasswordManagerDriver driver;
  EXPECT_CALL(driver, GeneratedPasswordAccepted).Times(0);
  std::unique_ptr<PasswordFormManagerForUI> ui_form =
      SetUpOverwritingUI(driver.AsWeakPtr());
  ASSERT_TRUE(ui_form);
  EXPECT_CALL(driver, ClearPreviewedForm);
  ui_form->OnNopeUpdateClicked();
}

TEST_F(PasswordGenerationManagerTest, GeneratedPasswordAccepted_UpdateUINever) {
  MockPasswordManagerDriver driver;
  EXPECT_CALL(driver, GeneratedPasswordAccepted).Times(0);
  std::unique_ptr<PasswordFormManagerForUI> ui_form =
      SetUpOverwritingUI(driver.AsWeakPtr());
  ASSERT_TRUE(ui_form);
  ui_form->OnNeverClicked();
}

TEST_F(PasswordGenerationManagerTest, GeneratedPasswordAccepted_UpdateUISave) {
  MockPasswordManagerDriver driver;
  std::unique_ptr<PasswordFormManagerForUI> ui_form =
      SetUpOverwritingUI(driver.AsWeakPtr());
  ASSERT_TRUE(ui_form);
  EXPECT_EQ(
      PasswordForm::Store::kAccountStore,
      ui_form->GetPasswordStoreForSaving(ui_form->GetPendingCredentials()));
  EXPECT_CALL(driver,
              GeneratedPasswordAccepted(CreateGenerated().password_value));
  ui_form->Save();
}

// Check that presaving a password for the first time results in adding it.
TEST_F(PasswordGenerationManagerTest, PresaveGeneratedPassword_New) {
  const PasswordForm generated = CreateGenerated();
  PasswordForm generated_with_date = generated;
  generated_with_date.date_created = base::Time::Now();
  generated_with_date.date_password_modified = base::Time::Now();

  EXPECT_CALL(store(), AddLogin(generated_with_date, _));
  manager().PresaveGeneratedPassword(generated, {}, &form_saver());
  EXPECT_TRUE(manager().HasGeneratedPassword());
}

// Check that presaving a password for the second time results in updating it.
TEST_F(PasswordGenerationManagerTest, PresaveGeneratedPassword_Replace) {
  PasswordForm generated = CreateGenerated();
  PasswordForm generated_with_date = generated;
  generated_with_date.date_created = base::Time::Now();
  generated_with_date.date_password_modified = base::Time::Now();

  EXPECT_CALL(store(), AddLogin(generated_with_date, _));
  manager().PresaveGeneratedPassword(generated, {}, &form_saver());

  ForwardByMinute();
  PasswordForm generated_updated = generated;
  generated_updated.password_value = u"newgenpwd";
  generated_with_date = generated_updated;
  generated_with_date.date_created = base::Time::Now();
  generated_with_date.date_password_modified = base::Time::Now();
  EXPECT_CALL(store(),
              UpdateLoginWithPrimaryKey(generated_with_date,
                                        FormHasUniqueKey(generated), _));
  manager().PresaveGeneratedPassword(generated_updated, {}, &form_saver());
  EXPECT_TRUE(manager().HasGeneratedPassword());
}

// Check that presaving a password for the third time results in updating it.
TEST_F(PasswordGenerationManagerTest, PresaveGeneratedPassword_ReplaceTwice) {
  PasswordForm generated = CreateGenerated();
  PasswordForm generated_with_date = generated;
  generated_with_date.date_created = base::Time::Now();
  generated_with_date.date_password_modified = base::Time::Now();

  EXPECT_CALL(store(), AddLogin(generated_with_date, _));
  manager().PresaveGeneratedPassword(generated, {}, &form_saver());

  ForwardByMinute();
  PasswordForm generated_updated = generated;
  generated_updated.password_value = u"newgenpwd";
  generated_with_date = generated_updated;
  generated_with_date.date_created = base::Time::Now();
  generated_with_date.date_password_modified = base::Time::Now();
  EXPECT_CALL(store(),
              UpdateLoginWithPrimaryKey(generated_with_date,
                                        FormHasUniqueKey(generated), _));
  manager().PresaveGeneratedPassword(generated_updated, {}, &form_saver());

  ForwardByMinute();
  generated = generated_updated;
  generated_updated.password_value = u"newgenpwd2";
  generated_updated.username_value = u"newusername";
  generated_with_date = generated_updated;
  generated_with_date.date_created = base::Time::Now();
  generated_with_date.date_password_modified = base::Time::Now();
  EXPECT_CALL(store(),
              UpdateLoginWithPrimaryKey(generated_with_date,
                                        FormHasUniqueKey(generated), _));
  manager().PresaveGeneratedPassword(generated_updated, {}, &form_saver());
  EXPECT_TRUE(manager().HasGeneratedPassword());
}

// Check that presaving a password with a known username results in clearing the
// username.
TEST_F(PasswordGenerationManagerTest, PresaveGeneratedPassword_WithConflict) {
  const PasswordForm generated = CreateGenerated();

  PasswordForm saved = CreateSaved();
  saved.username_value = generated.username_value;

  PasswordForm generated_with_date = generated;
  generated_with_date.date_created = base::Time::Now();
  generated_with_date.date_password_modified = base::Time::Now();
  generated_with_date.username_value.clear();

  EXPECT_CALL(store(), AddLogin(generated_with_date, _));
  manager().PresaveGeneratedPassword(generated, {&saved}, &form_saver());
  EXPECT_TRUE(manager().HasGeneratedPassword());
}

// Check that presaving a password with an unknown username saves it as is.
TEST_F(PasswordGenerationManagerTest,
       PresaveGeneratedPassword_WithoutConflict) {
  const PasswordForm generated = CreateGenerated();
  PasswordForm generated_with_date = generated;
  generated_with_date.date_created = base::Time::Now();
  generated_with_date.date_password_modified = base::Time::Now();

  const PasswordForm saved = CreateSaved();
  EXPECT_CALL(store(), AddLogin(generated_with_date, _));
  manager().PresaveGeneratedPassword(generated, {&saved}, &form_saver());
  EXPECT_TRUE(manager().HasGeneratedPassword());
}

// Check that presaving a password followed by a call to save a pending
// credential (as new) results in replacing the presaved password with the
// pending one.
TEST_F(PasswordGenerationManagerTest, PresaveGeneratedPassword_ThenSaveAsNew) {
  const PasswordForm generated = CreateGenerated();

  EXPECT_CALL(store(), AddLogin);
  manager().PresaveGeneratedPassword(generated, {}, &form_saver());

  // User edits after submission.
  PasswordForm pending = generated;
  pending.password_value = u"edited_password";
  pending.username_value = u"edited_username";
  PasswordForm generated_with_date = pending;
  generated_with_date.date_created = base::Time::Now();
  generated_with_date.date_password_modified = base::Time::Now();
  generated_with_date.date_last_used = base::Time::Now();
  EXPECT_CALL(store(),
              UpdateLoginWithPrimaryKey(generated_with_date,
                                        FormHasUniqueKey(generated), _));
  manager().CommitGeneratedPassword(
      pending, {} /* matches */, std::u16string() /* old_password */,
      PasswordForm::Store::kProfileStore, &form_saver(),
      nullptr /* account_store_form_saver */);
  EXPECT_TRUE(manager().HasGeneratedPassword());
}

// Check that presaving a password followed by a call to save a pending
// credential (as update) results in replacing the presaved password with the
// pending one.
TEST_F(PasswordGenerationManagerTest, PresaveGeneratedPassword_ThenUpdate) {
  PasswordForm generated = CreateGenerated();

  PasswordForm related_password = CreateSaved();
  related_password.username_value = u"username";
  related_password.username_element = u"username_field";
  related_password.password_value = u"old password";
  related_password.match_type = PasswordForm::MatchType::kExact;

  PasswordForm related_psl_password = CreateSavedPSL();
  related_psl_password.username_value = u"username";
  related_psl_password.password_value = u"old password";
  related_psl_password.match_type = PasswordForm::MatchType::kPSL;

  PasswordForm unrelated_password = CreateSaved();
  unrelated_password.username_value = u"another username";
  unrelated_password.password_value = u"some password";
  unrelated_password.match_type = PasswordForm::MatchType::kExact;

  PasswordForm unrelated_psl_password = CreateSavedPSL();
  unrelated_psl_password.username_value = u"another username";
  unrelated_psl_password.password_value = u"some password";
  unrelated_psl_password.match_type = PasswordForm::MatchType::kPSL;

  EXPECT_CALL(store(), AddLogin);
  const std::vector<raw_ptr<const PasswordForm, VectorExperimental>> matches = {
      &related_password, &related_psl_password, &unrelated_password,
      &unrelated_psl_password};
  manager().PresaveGeneratedPassword(generated, matches, &form_saver());

  generated.username_value = u"username";
  PasswordForm generated_with_date = generated;
  generated_with_date.date_created = base::Time::Now();
  generated_with_date.date_password_modified = base::Time::Now();
  generated_with_date.date_last_used = base::Time::Now();

  EXPECT_CALL(store(),
              UpdateLoginWithPrimaryKey(
                  generated_with_date, FormHasUniqueKey(CreateGenerated()), _));

  PasswordForm related_password_expected = related_password;
  related_password_expected.password_value = generated.password_value;
  related_password_expected.date_password_modified = base::Time::Now();
  EXPECT_CALL(store(), UpdateLogin(related_password_expected, _));

  PasswordForm related_psl_password_expected = related_psl_password;
  related_psl_password_expected.password_value = generated.password_value;
  related_psl_password_expected.date_password_modified = base::Time::Now();
  EXPECT_CALL(store(), UpdateLogin(related_psl_password_expected, _));

  const std::vector<PasswordForm> matches_for_generation = {
      related_password, related_psl_password, unrelated_password,
      unrelated_psl_password};
  manager().CommitGeneratedPassword(
      generated, matches_for_generation, u"old password",
      PasswordForm::Store::kProfileStore, &form_saver(),
      nullptr /* account_store_form_saver */);
  EXPECT_TRUE(manager().HasGeneratedPassword());
}

// Check that removing a presaved password removes the presaved password.
TEST_F(PasswordGenerationManagerTest, PasswordNoLongerGenerated) {
  const PasswordForm generated = CreateGenerated();

  EXPECT_CALL(store(), AddLogin);
  manager().PresaveGeneratedPassword(generated, {}, &form_saver());

  EXPECT_CALL(store(), RemoveLogin(_, FormHasUniqueKey(generated)));
  manager().PasswordNoLongerGenerated(&form_saver());
  EXPECT_FALSE(manager().HasGeneratedPassword());
}

// Check that removing the presaved password and then presaving again results in
// adding the second presaved password as new.
TEST_F(PasswordGenerationManagerTest,
       PasswordNoLongerGenerated_AndPresaveAgain) {
  PasswordForm generated = CreateGenerated();
  PasswordForm generated_with_date = generated;
  generated_with_date.date_created = base::Time::Now();
  generated_with_date.date_password_modified = base::Time::Now();

  EXPECT_CALL(store(), AddLogin(generated_with_date, _));
  manager().PresaveGeneratedPassword(generated, {}, &form_saver());

  EXPECT_CALL(store(), RemoveLogin(_, FormHasUniqueKey(generated_with_date)));
  manager().PasswordNoLongerGenerated(&form_saver());

  ForwardByMinute();
  generated.username_value = u"newgenusername";
  generated.password_value = u"newgenpwd";
  generated_with_date = generated;
  generated_with_date.date_created = base::Time::Now();
  generated_with_date.date_password_modified = base::Time::Now();
  EXPECT_CALL(store(), AddLogin(generated_with_date, _));
  manager().PresaveGeneratedPassword(generated, {}, &form_saver());
  EXPECT_TRUE(manager().HasGeneratedPassword());
}

// Check that presaving a password once in original and then once in clone
// results in the clone calling update, not a fresh save.
TEST_F(PasswordGenerationManagerTest, PresaveGeneratedPassword_CloneUpdates) {
  PasswordForm generated = CreateGenerated();
  PasswordForm generated_with_date = generated;
  generated_with_date.date_created = base::Time::Now();
  generated_with_date.date_password_modified = base::Time::Now();

  EXPECT_CALL(store(), AddLogin(generated_with_date, _));
  manager().PresaveGeneratedPassword(generated, {}, &form_saver());

  std::unique_ptr<PasswordGenerationManager> cloned_state = manager().Clone();
  ForwardByMinute();

  EXPECT_TRUE(cloned_state->HasGeneratedPassword());
  PasswordForm generated_updated = generated;
  generated_updated.username_value = u"newname";
  generated_with_date = generated_updated;
  generated_with_date.date_created = base::Time::Now();
  generated_with_date.date_password_modified = base::Time::Now();
  EXPECT_CALL(store(),
              UpdateLoginWithPrimaryKey(generated_with_date,
                                        FormHasUniqueKey(generated), _));
  cloned_state->PresaveGeneratedPassword(generated_updated, {}, &form_saver());
  EXPECT_TRUE(cloned_state->HasGeneratedPassword());
}

// Check that a clone can still work after the original is destroyed.
TEST_F(PasswordGenerationManagerTest, PresaveGeneratedPassword_CloneSurvives) {
  auto original = std::make_unique<PasswordGenerationManager>(&client());
  const PasswordForm generated = CreateGenerated();

  EXPECT_CALL(store(), AddLogin);
  original->PresaveGeneratedPassword(generated, {}, &form_saver());

  std::unique_ptr<PasswordGenerationManager> cloned_manager = original->Clone();
  original.reset();
  EXPECT_CALL(store(), UpdateLoginWithPrimaryKey);
  cloned_manager->PresaveGeneratedPassword(generated, {}, &form_saver());
}

// Check that changing a generated password emits UMA metrics. This test case
// changes every character class.
TEST_F(PasswordGenerationManagerTest, EditsInGeneratedPasswordMetrics) {
  // User accepts a generated password.
  PasswordForm generated = CreateGenerated();
  generated.date_created = base::Time::Now();
  generated.date_password_modified = base::Time::Now();
  generated.date_last_used = base::Time::Now();
  generated.password_value = u"aaa123&*";
  EXPECT_CALL(store(), AddLogin);
  manager().PresaveGeneratedPassword(generated, {}, &form_saver());

  // User edits the generated password.
  PasswordForm generated_after_edits = generated;
  generated_after_edits.password_value = u"AAA#";
  base::HistogramTester histogram_tester;
  EXPECT_CALL(store(), UpdateLoginWithPrimaryKey(
                           generated_after_edits,
                           FormHasUniqueKey(generated_after_edits), _));
  manager().CommitGeneratedPassword(
      generated_after_edits, {} /* matches */,
      std::u16string() /* old_password */, PasswordForm::Store::kProfileStore,
      &form_saver(), nullptr /* account_store_form_saver */);

  // Check emitted metrics.
  histogram_tester.ExpectUniqueSample(
      "PasswordGeneration.EditsInGeneratedPassword.Uppercase",
      password_manager::CharacterClassPresenceChange::kAdded, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordGeneration.EditsInGeneratedPassword.Lowercase",
      password_manager::CharacterClassPresenceChange::kDeleted, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordGeneration.EditsInGeneratedPassword.Letters",
      password_manager::CharacterClassPresenceChange::
          kSpecificCharactersChanged,
      1);
  histogram_tester.ExpectUniqueSample(
      "PasswordGeneration.EditsInGeneratedPassword.Numerics",
      password_manager::CharacterClassPresenceChange::kDeleted, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordGeneration.EditsInGeneratedPassword.Symbols",
      password_manager::CharacterClassPresenceChange::
          kSpecificCharactersChanged,
      1);
  histogram_tester.ExpectTotalCount(
      "PasswordGeneration.EditsInGeneratedPassword.AlteredLengthIncreased", 0);
  histogram_tester.ExpectUniqueSample(
      "PasswordGeneration.EditsInGeneratedPassword.AttributesMask", 5, 1);
}

// Check that changing length of the generated password emits UMA metrics only
// if character classes presence is not changed. E.g. "abcd" => "abcd&" (symbols
// added) should not emit a "length changed", because the wrong length is not
// the root cause of the password edit.
TEST_F(PasswordGenerationManagerTest,
       EditsInGeneratedPasswordMetrics_OnlyLengthChanged) {
  // User accepts a generated password.
  PasswordForm generated = CreateGenerated();
  generated.date_created = base::Time::Now();
  generated.date_password_modified = base::Time::Now();
  generated.date_last_used = base::Time::Now();
  generated.password_value = u"12345";
  EXPECT_CALL(store(), AddLogin);
  manager().PresaveGeneratedPassword(generated, {}, &form_saver());

  // User edits the generated password.
  PasswordForm generated_after_edits = generated;
  generated_after_edits.password_value = u"12345678";
  base::HistogramTester histogram_tester;
  EXPECT_CALL(store(), UpdateLoginWithPrimaryKey(
                           generated_after_edits,
                           FormHasUniqueKey(generated_after_edits), _));
  manager().CommitGeneratedPassword(
      generated_after_edits, {} /* matches */,
      std::u16string() /* old_password */, PasswordForm::Store::kProfileStore,
      &form_saver(), nullptr /* account_store_form_saver */);

  // Check emitted metrics.
  histogram_tester.ExpectUniqueSample(
      "PasswordGeneration.EditsInGeneratedPassword.Uppercase",
      password_manager::CharacterClassPresenceChange::kNoChange, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordGeneration.EditsInGeneratedPassword.Lowercase",
      password_manager::CharacterClassPresenceChange::kNoChange, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordGeneration.EditsInGeneratedPassword.Letters",
      password_manager::CharacterClassPresenceChange::kNoChange, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordGeneration.EditsInGeneratedPassword.Numerics",
      password_manager::CharacterClassPresenceChange::
          kSpecificCharactersChanged,
      1);
  histogram_tester.ExpectUniqueSample(
      "PasswordGeneration.EditsInGeneratedPassword.Symbols",
      password_manager::CharacterClassPresenceChange::kNoChange, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordGeneration.EditsInGeneratedPassword.AlteredLengthIncreased", 1,
      1);
  histogram_tester.ExpectUniqueSample(
      "PasswordGeneration.EditsInGeneratedPassword.AttributesMask", 1, 1);
}

// Check that committing a password for the second time results in updating it.
// This may happen when the user submits the form with empty username and the
// crediential with no username gets committed and then the dialog with the
// proposition to add username is displayed. If the user adds a username the
// credential will be committed for the second time.
TEST_F(PasswordGenerationManagerTest, CommitGeneratedPassword_Replace) {
  PasswordForm generated = CreateGenerated();
  generated.date_created = base::Time::Now();
  generated.date_password_modified = base::Time::Now();
  generated.date_last_used = base::Time::Now();

  EXPECT_CALL(store(), AddLogin);
  manager().PresaveGeneratedPassword(generated, {}, &form_saver());

  EXPECT_CALL(store(), UpdateLoginWithPrimaryKey(
                           generated, FormHasUniqueKey(generated), _));
  manager().CommitGeneratedPassword(
      generated, {}, u"", PasswordForm::Store::kProfileStore, &form_saver(),
      nullptr /* account_store_form_saver */);

  ForwardByMinute();
  PasswordForm generated_updated = generated;
  generated_updated.username_value = u"NewUsername";
  generated_updated.date_created = base::Time::Now();
  generated_updated.date_password_modified = base::Time::Now();
  generated_updated.date_last_used = base::Time::Now();
  EXPECT_CALL(store(), UpdateLoginWithPrimaryKey(
                           generated_updated, FormHasUniqueKey(generated), _));
  manager().CommitGeneratedPassword(
      generated_updated, {}, u"", PasswordForm::Store::kProfileStore,
      &form_saver(), nullptr /* account_store_form_saver */);
}

TEST_F(PasswordGenerationManagerTest,
       UpdateWithAGeneratedPasswordInBothStores) {
  // This test assumes that there is a password with the same username and url
  // saved in both account and profile stores and it's been updated with a
  // generated password. Expected result: the password should be updated in both
  // stores.
  SetAccountStoreEnabled(true);
  PasswordForm generated = CreateGenerated();
  generated.date_created = base::Time::Now();
  generated.date_password_modified = base::Time::Now();
  generated.date_last_used = base::Time::Now();
  FormSaverImpl account_store_form_saver(&store());
  PasswordForm saved = CreateSaved();
  saved.username_value = generated.username_value;

  EXPECT_CALL(store(), AddLogin);
  manager().PresaveGeneratedPassword(generated, {&saved},
                                     &account_store_form_saver);

  // Should call UpdateLoginWithPrimaryKey for account form saver and
  // UpdateLogin for profile form saver.
  EXPECT_CALL(store(), UpdateLogin(generated, _));
  EXPECT_CALL(store(), UpdateLoginWithPrimaryKey(generated, _, _));

  manager().CommitGeneratedPassword(
      generated, std::vector{saved}, u"",
      PasswordForm::Store::kProfileStore | PasswordForm::Store::kAccountStore,
      &form_saver(), &account_store_form_saver);
}

}  // namespace
}  // namespace password_manager
