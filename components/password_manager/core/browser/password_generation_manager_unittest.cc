// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_generation_manager.h"

#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/form_saver_impl.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/common/password_manager_features.h"
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
constexpr time_t kTime = 123456789;
constexpr time_t kAnotherTime = 987654321;

// Creates a dummy saved credential.
PasswordForm CreateSaved() {
  PasswordForm form;
  form.url = GURL(kURL);
  form.signon_realm = form.url.spec();
  form.action = GURL("https://login.example.org");
  form.username_value = ASCIIToUTF16("old_username");
  form.password_value = ASCIIToUTF16("12345");
  return form;
}

PasswordForm CreateSavedFederated() {
  PasswordForm federated;
  federated.url = GURL(kURL);
  federated.signon_realm = "federation://example.in/google.com";
  federated.type = PasswordForm::Type::kApi;
  federated.federation_origin =
      url::Origin::Create(GURL("https://google.com/"));
  federated.username_value = ASCIIToUTF16("federated_username");
  return federated;
}

// Creates a dummy saved PSL credential.
PasswordForm CreateSavedPSL() {
  PasswordForm form;
  form.url = GURL(kSubdomainURL);
  form.signon_realm = form.url.spec();
  form.action = GURL("https://login.example.org");
  form.username_value = ASCIIToUTF16("old_username2");
  form.password_value = ASCIIToUTF16("passw0rd");
  form.is_public_suffix_match = true;
  return form;
}

// Creates a dummy generated password.
PasswordForm CreateGenerated() {
  PasswordForm form;
  form.url = GURL(kURL);
  form.signon_realm = form.url.spec();
  form.action = GURL("https://signup.example.org");
  form.username_value = ASCIIToUTF16("MyName");
  form.password_value = ASCIIToUTF16("Strong password");
  form.type = PasswordForm::Type::kGenerated;
  return form;
}

MATCHER_P(FormHasUniqueKey, key, "") {
  return ArePasswordFormUniqueKeysEqual(arg, key);
}

class MockPasswordManagerDriver : public StubPasswordManagerDriver {
 public:
  MOCK_METHOD1(GeneratedPasswordAccepted, void(const base::string16& password));
};

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  bool PromptUserToSaveOrUpdatePassword(
      std::unique_ptr<PasswordFormManagerForUI> form_to_save,
      bool update_password) override;

  MOCK_METHOD1(PromptUserToSaveOrUpdatePasswordMock,
               bool(bool update_password));

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

  MockPasswordStore& store() { return *mock_store_; }
  PasswordGenerationManager& manager() { return generation_manager_; }
  FormSaverImpl& form_saver() { return form_saver_; }
  MockPasswordManagerClient& client() { return client_; }

  // Immitates user accepting the password that can't be immediately presaved.
  // The returned value represents the UI model for the update bubble.
  std::unique_ptr<PasswordFormManagerForUI> SetUpOverwritingUI(
      base::WeakPtr<PasswordManagerDriver> driver);

 private:
  // For the MockPasswordStore.
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<MockPasswordStore> mock_store_;
  // Test with the real form saver for better robustness.
  FormSaverImpl form_saver_;
  MockPasswordManagerClient client_;
  PasswordGenerationManager generation_manager_;
};

PasswordGenerationManagerTest::PasswordGenerationManagerTest()
    : mock_store_(new testing::StrictMock<MockPasswordStore>()),
      form_saver_(mock_store_.get()),
      generation_manager_(&client_) {
  auto clock = std::make_unique<base::SimpleTestClock>();
  clock->SetNow(base::Time::FromTimeT(kTime));
  generation_manager_.set_clock(std::move(clock));
}

PasswordGenerationManagerTest::~PasswordGenerationManagerTest() {
  mock_store_->ShutdownOnUIThread();
}

std::unique_ptr<PasswordFormManagerForUI>
PasswordGenerationManagerTest::SetUpOverwritingUI(
    base::WeakPtr<PasswordManagerDriver> driver) {
  PasswordForm generated = CreateGenerated();
  PasswordForm saved = CreateSaved();
  generated.username_value = ASCIIToUTF16("");
  saved.username_value = ASCIIToUTF16("");
  const PasswordForm federated = CreateSavedFederated();
  FakeFormFetcher fetcher;
  fetcher.SetNonFederated({&saved});
  fetcher.set_federated({&federated});

  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordMock(true))
      .WillOnce(testing::Return(true));
  manager().GeneratedPasswordAccepted(
      std::move(generated), fetcher.GetNonFederatedMatches(),
      fetcher.GetFederatedMatches(), std::move(driver));
  return client_.MoveForm();
}

// Check that accepting a generated password simply relays the message to the
// driver.
TEST_F(PasswordGenerationManagerTest, GeneratedPasswordAccepted_EmptyStore) {
  PasswordForm generated = CreateGenerated();
  MockPasswordManagerDriver driver;
  FakeFormFetcher fetcher;

  EXPECT_CALL(driver, GeneratedPasswordAccepted(generated.password_value));
  manager().GeneratedPasswordAccepted(
      std::move(generated), fetcher.GetNonFederatedMatches(),
      fetcher.GetFederatedMatches(), driver.AsWeakPtr());
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
  fetcher.SetNonFederated({&saved});

  EXPECT_CALL(driver, GeneratedPasswordAccepted(generated.password_value));
  manager().GeneratedPasswordAccepted(
      std::move(generated), fetcher.GetNonFederatedMatches(),
      fetcher.GetFederatedMatches(), driver.AsWeakPtr());
  EXPECT_FALSE(manager().HasGeneratedPassword());
}

TEST_F(PasswordGenerationManagerTest, GeneratedPasswordAccepted_UpdateUI) {
  MockPasswordManagerDriver driver;
  EXPECT_CALL(driver, GeneratedPasswordAccepted(_)).Times(0);
  std::unique_ptr<PasswordFormManagerForUI> ui_form =
      SetUpOverwritingUI(driver.AsWeakPtr());
  ASSERT_TRUE(ui_form);
  EXPECT_EQ(GURL(kURL), ui_form->GetURL());
  EXPECT_THAT(
      ui_form->GetBestMatches(),
      ElementsAre(Field(&PasswordForm::username_value, ASCIIToUTF16(""))));
  EXPECT_THAT(ui_form->GetFederatedMatches(),
              ElementsAre(Pointee(CreateSavedFederated())));
  EXPECT_EQ(ASCIIToUTF16(""), ui_form->GetPendingCredentials().username_value);
  EXPECT_EQ(CreateGenerated().password_value,
            ui_form->GetPendingCredentials().password_value);
  EXPECT_THAT(ui_form->GetInteractionsStats(), IsEmpty());
  EXPECT_FALSE(ui_form->IsBlacklisted());
}

TEST_F(PasswordGenerationManagerTest,
       GeneratedPasswordAccepted_UpdateUIDismissed) {
  MockPasswordManagerDriver driver;
  EXPECT_CALL(driver, GeneratedPasswordAccepted(_)).Times(0);
  std::unique_ptr<PasswordFormManagerForUI> ui_form =
      SetUpOverwritingUI(driver.AsWeakPtr());
  ASSERT_TRUE(ui_form);
  ui_form->OnNoInteraction(true);
}

TEST_F(PasswordGenerationManagerTest, GeneratedPasswordAccepted_UpdateUINope) {
  MockPasswordManagerDriver driver;
  EXPECT_CALL(driver, GeneratedPasswordAccepted(_)).Times(0);
  std::unique_ptr<PasswordFormManagerForUI> ui_form =
      SetUpOverwritingUI(driver.AsWeakPtr());
  ASSERT_TRUE(ui_form);
  ui_form->OnNopeUpdateClicked();
}

TEST_F(PasswordGenerationManagerTest, GeneratedPasswordAccepted_UpdateUINever) {
  MockPasswordManagerDriver driver;
  EXPECT_CALL(driver, GeneratedPasswordAccepted(_)).Times(0);
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
  EXPECT_CALL(driver,
              GeneratedPasswordAccepted(CreateGenerated().password_value));
  ui_form->Save();
}

// Check that presaving a password for the first time results in adding it.
TEST_F(PasswordGenerationManagerTest, PresaveGeneratedPassword_New) {
  const PasswordForm generated = CreateGenerated();
  PasswordForm generated_with_date = generated;
  generated_with_date.date_created = base::Time::FromTimeT(kTime);

  EXPECT_CALL(store(), AddLogin(generated_with_date));
  manager().PresaveGeneratedPassword(generated, {}, &form_saver());
  EXPECT_TRUE(manager().HasGeneratedPassword());
}

// Check that presaving a password for the second time results in updating it.
TEST_F(PasswordGenerationManagerTest, PresaveGeneratedPassword_Replace) {
  PasswordForm generated = CreateGenerated();
  PasswordForm generated_with_date = generated;
  generated_with_date.date_created = base::Time::FromTimeT(kTime);

  EXPECT_CALL(store(), AddLogin(generated_with_date));
  manager().PresaveGeneratedPassword(generated, {}, &form_saver());

  PasswordForm generated_updated = generated;
  generated_updated.password_value = ASCIIToUTF16("newgenpwd");
  generated_with_date = generated_updated;
  generated_with_date.date_created = base::Time::FromTimeT(kTime);
  EXPECT_CALL(store(), UpdateLoginWithPrimaryKey(generated_with_date,
                                                 FormHasUniqueKey(generated)));
  manager().PresaveGeneratedPassword(generated_updated, {}, &form_saver());
  EXPECT_TRUE(manager().HasGeneratedPassword());
}

// Check that presaving a password for the third time results in updating it.
TEST_F(PasswordGenerationManagerTest, PresaveGeneratedPassword_ReplaceTwice) {
  PasswordForm generated = CreateGenerated();
  PasswordForm generated_with_date = generated;
  generated_with_date.date_created = base::Time::FromTimeT(kTime);

  EXPECT_CALL(store(), AddLogin(generated_with_date));
  manager().PresaveGeneratedPassword(generated, {}, &form_saver());

  PasswordForm generated_updated = generated;
  generated_updated.password_value = ASCIIToUTF16("newgenpwd");
  generated_with_date = generated_updated;
  generated_with_date.date_created = base::Time::FromTimeT(kTime);
  EXPECT_CALL(store(), UpdateLoginWithPrimaryKey(generated_with_date,
                                                 FormHasUniqueKey(generated)));
  manager().PresaveGeneratedPassword(generated_updated, {}, &form_saver());

  generated = generated_updated;
  generated_updated.password_value = ASCIIToUTF16("newgenpwd2");
  generated_updated.username_value = ASCIIToUTF16("newusername");
  generated_with_date = generated_updated;
  generated_with_date.date_created = base::Time::FromTimeT(kTime);
  EXPECT_CALL(store(), UpdateLoginWithPrimaryKey(generated_with_date,
                                                 FormHasUniqueKey(generated)));
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
  generated_with_date.date_created = base::Time::FromTimeT(kTime);
  generated_with_date.username_value.clear();

  EXPECT_CALL(store(), AddLogin(generated_with_date));
  manager().PresaveGeneratedPassword(generated, {&saved}, &form_saver());
  EXPECT_TRUE(manager().HasGeneratedPassword());
}

// Check that presaving a password with an unknown username saves it as is.
TEST_F(PasswordGenerationManagerTest,
       PresaveGeneratedPassword_WithoutConflict) {
  const PasswordForm generated = CreateGenerated();
  PasswordForm generated_with_date = generated;
  generated_with_date.date_created = base::Time::FromTimeT(kTime);

  const PasswordForm saved = CreateSaved();
  EXPECT_CALL(store(), AddLogin(generated_with_date));
  manager().PresaveGeneratedPassword(generated, {&saved}, &form_saver());
  EXPECT_TRUE(manager().HasGeneratedPassword());
}

// Check that presaving a password followed by a call to save a pending
// credential (as new) results in replacing the presaved password with the
// pending one.
TEST_F(PasswordGenerationManagerTest, PresaveGeneratedPassword_ThenSaveAsNew) {
  const PasswordForm generated = CreateGenerated();

  EXPECT_CALL(store(), AddLogin(_));
  manager().PresaveGeneratedPassword(generated, {}, &form_saver());

  // User edits after submission.
  PasswordForm pending = generated;
  pending.password_value = ASCIIToUTF16("edited_password");
  pending.username_value = ASCIIToUTF16("edited_username");
  PasswordForm generated_with_date = pending;
  generated_with_date.date_created = base::Time::FromTimeT(kTime);
  generated_with_date.date_last_used = base::Time::FromTimeT(kTime);
  EXPECT_CALL(store(), UpdateLoginWithPrimaryKey(generated_with_date,
                                                 FormHasUniqueKey(generated)));
  manager().CommitGeneratedPassword(pending, {} /* matches */,
                                    base::string16() /* old_password */,
                                    &form_saver());
  EXPECT_TRUE(manager().HasGeneratedPassword());
}

// Check that presaving a password followed by a call to save a pending
// credential (as update) results in replacing the presaved password with the
// pending one.
TEST_F(PasswordGenerationManagerTest, PresaveGeneratedPassword_ThenUpdate) {
  PasswordForm generated = CreateGenerated();

  PasswordForm related_password = CreateSaved();
  related_password.username_value = ASCIIToUTF16("username");
  related_password.username_element = ASCIIToUTF16("username_field");
  related_password.password_value = ASCIIToUTF16("old password");

  PasswordForm related_psl_password = CreateSavedPSL();
  related_psl_password.username_value = ASCIIToUTF16("username");
  related_psl_password.password_value = ASCIIToUTF16("old password");

  PasswordForm unrelated_password = CreateSaved();
  unrelated_password.username_value = ASCIIToUTF16("another username");
  unrelated_password.password_value = ASCIIToUTF16("some password");

  PasswordForm unrelated_psl_password = CreateSavedPSL();
  unrelated_psl_password.username_value = ASCIIToUTF16("another username");
  unrelated_psl_password.password_value = ASCIIToUTF16("some password");

  EXPECT_CALL(store(), AddLogin(_));
  const std::vector<const PasswordForm*> matches = {
      &related_password, &related_psl_password, &unrelated_password,
      &unrelated_psl_password};
  manager().PresaveGeneratedPassword(generated, matches, &form_saver());

  generated.username_value = ASCIIToUTF16("username");
  PasswordForm generated_with_date = generated;
  generated_with_date.date_created = base::Time::FromTimeT(kTime);
  generated_with_date.date_last_used = base::Time::FromTimeT(kTime);

  EXPECT_CALL(store(),
              UpdateLoginWithPrimaryKey(generated_with_date,
                                        FormHasUniqueKey(CreateGenerated())));

  PasswordForm related_password_expected = related_password;
  related_password_expected.password_value = generated.password_value;
  EXPECT_CALL(store(), UpdateLogin(related_password_expected));

  PasswordForm related_psl_password_expected = related_psl_password;
  related_psl_password_expected.password_value = generated.password_value;
  EXPECT_CALL(store(), UpdateLogin(related_psl_password_expected));

  manager().CommitGeneratedPassword(
      generated, matches, ASCIIToUTF16("old password"), &form_saver());
  EXPECT_TRUE(manager().HasGeneratedPassword());
}

// Check that removing a presaved password removes the presaved password.
TEST_F(PasswordGenerationManagerTest, PasswordNoLongerGenerated) {
  PasswordForm generated = CreateGenerated();

  EXPECT_CALL(store(), AddLogin(_));
  manager().PresaveGeneratedPassword(generated, {}, &form_saver());

  generated.date_created = base::Time::FromTimeT(kTime);
  EXPECT_CALL(store(), RemoveLogin(generated));
  manager().PasswordNoLongerGenerated(&form_saver());
  EXPECT_FALSE(manager().HasGeneratedPassword());
}

// Check that removing the presaved password and then presaving again results in
// adding the second presaved password as new.
TEST_F(PasswordGenerationManagerTest,
       PasswordNoLongerGenerated_AndPresaveAgain) {
  PasswordForm generated = CreateGenerated();
  PasswordForm generated_with_date = generated;
  generated_with_date.date_created = base::Time::FromTimeT(kTime);

  EXPECT_CALL(store(), AddLogin(generated_with_date));
  manager().PresaveGeneratedPassword(generated, {}, &form_saver());

  EXPECT_CALL(store(), RemoveLogin(generated_with_date));
  manager().PasswordNoLongerGenerated(&form_saver());

  generated.username_value = ASCIIToUTF16("newgenusername");
  generated.password_value = ASCIIToUTF16("newgenpwd");
  generated_with_date = generated;
  generated_with_date.date_created = base::Time::FromTimeT(kTime);
  EXPECT_CALL(store(), AddLogin(generated_with_date));
  manager().PresaveGeneratedPassword(generated, {}, &form_saver());
  EXPECT_TRUE(manager().HasGeneratedPassword());
}

// Check that presaving a password once in original and then once in clone
// results in the clone calling update, not a fresh save.
TEST_F(PasswordGenerationManagerTest, PresaveGeneratedPassword_CloneUpdates) {
  PasswordForm generated = CreateGenerated();
  PasswordForm generated_with_date = generated;
  generated_with_date.date_created = base::Time::FromTimeT(kTime);

  EXPECT_CALL(store(), AddLogin(generated_with_date));
  manager().PresaveGeneratedPassword(generated, {}, &form_saver());

  std::unique_ptr<PasswordGenerationManager> cloned_state = manager().Clone();
  std::unique_ptr<base::SimpleTestClock> clock(new base::SimpleTestClock);
  clock->SetNow(base::Time::FromTimeT(kAnotherTime));
  cloned_state->set_clock(std::move(clock));

  EXPECT_TRUE(cloned_state->HasGeneratedPassword());
  PasswordForm generated_updated = generated;
  generated_updated.username_value = ASCIIToUTF16("newname");
  generated_with_date = generated_updated;
  generated_with_date.date_created = base::Time::FromTimeT(kAnotherTime);
  EXPECT_CALL(store(), UpdateLoginWithPrimaryKey(generated_with_date,
                                                 FormHasUniqueKey(generated)));
  cloned_state->PresaveGeneratedPassword(generated_updated, {}, &form_saver());
  EXPECT_TRUE(cloned_state->HasGeneratedPassword());
}

// Check that a clone can still work after the original is destroyed.
TEST_F(PasswordGenerationManagerTest, PresaveGeneratedPassword_CloneSurvives) {
  auto original = std::make_unique<PasswordGenerationManager>(&client());
  const PasswordForm generated = CreateGenerated();

  EXPECT_CALL(store(), AddLogin(_));
  original->PresaveGeneratedPassword(generated, {}, &form_saver());

  std::unique_ptr<PasswordGenerationManager> cloned_manager = original->Clone();
  original.reset();
  EXPECT_CALL(store(), UpdateLoginWithPrimaryKey(_, _));
  cloned_manager->PresaveGeneratedPassword(generated, {}, &form_saver());
}

}  // namespace
}  // namespace password_manager
