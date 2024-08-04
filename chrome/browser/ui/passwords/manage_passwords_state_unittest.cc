// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_state.h"

#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "components/password_manager/core/browser/mock_password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using base::ASCIIToUTF16;
using password_manager::MockPasswordFormManagerForUI;
using password_manager::PasswordForm;
using password_manager::PasswordStoreChange;
using password_manager::PasswordStoreChangeList;
using ::testing::_;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Mock;
using ::testing::Not;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

namespace {

constexpr char kTestOrigin[] = "http://example.com/";
constexpr char kTestPSLOrigin[] = "http://1.example.com/";

std::vector<raw_ptr<const PasswordForm, VectorExperimental>> GetRawPointers(
    const std::vector<std::unique_ptr<PasswordForm>>& forms) {
  // &std::unique_ptr<PasswordForm>::get returns a non-const ptr and hence
  // cannot be used instead.
  return base::ToVector(
      forms,
      [](const auto& form) -> raw_ptr<const PasswordForm, VectorExperimental> {
        return form.get();
      });
}

class MockPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  MOCK_METHOD(void, UpdateFormManagers, (), (override));
};

class ManagePasswordsStateTest : public testing::Test {
 public:
  void SetUp() override {
    saved_match_.url = GURL(kTestOrigin);
    saved_match_.signon_realm = kTestOrigin;
    saved_match_.username_value = u"username";
    saved_match_.username_element = u"username_element";
    saved_match_.password_value = u"12345";
    saved_match_.password_element = u"password_element";
    saved_match_.match_type = PasswordForm::MatchType::kExact;

    psl_match_ = saved_match_;
    psl_match_.url = GURL(kTestPSLOrigin);
    psl_match_.signon_realm = kTestPSLOrigin;
    psl_match_.username_value = u"username_psl";
    psl_match_.match_type = PasswordForm::MatchType::kPSL;

    local_federated_form_ = saved_match_;
    local_federated_form_.federation_origin =
        url::SchemeHostPort(GURL("https://idp.com"));
    local_federated_form_.password_value.clear();
    local_federated_form_.signon_realm =
        "federation://example.com/accounts.com";
    local_federated_form_.match_type = PasswordForm::MatchType::kExact;

    passwords_data_.set_client(&mock_client_);
  }

  PasswordForm& saved_match() { return saved_match_; }
  PasswordForm& psl_match() { return psl_match_; }
  PasswordForm& local_federated_form() { return local_federated_form_; }
  ManagePasswordsState& passwords_data() { return passwords_data_; }

  // Returns a mock PasswordFormManager containing |best_matches| and
  // |federated_matches|.
  std::unique_ptr<MockPasswordFormManagerForUI> CreateFormManager(
      std::vector<PasswordForm> best_matches,
      std::vector<PasswordForm> federated_matches);

  // Pushes irrelevant updates to |passwords_data_| and checks that they don't
  // affect the state.
  void TestNoisyUpdates();

  // Pushes both relevant and irrelevant updates to |passwords_data_|.
  void TestAllUpdates();

  // Pushes a blocklisted form and checks that it doesn't affect the state.
  void TestBlocklistedUpdates();

 private:
  MockPasswordManagerClient mock_client_;

  ManagePasswordsState passwords_data_;
  PasswordForm saved_match_;
  PasswordForm psl_match_;
  PasswordForm local_federated_form_;
};

std::unique_ptr<MockPasswordFormManagerForUI>
ManagePasswordsStateTest::CreateFormManager(
    std::vector<PasswordForm> best_matches,
    std::vector<PasswordForm> federated_matches) {
  auto form_manager = std::make_unique<MockPasswordFormManagerForUI>();
  EXPECT_CALL(*form_manager, GetBestMatches())
      .WillOnce(testing::Return(std::move(best_matches)));
  EXPECT_CALL(*form_manager, GetFederatedMatches())
      .WillOnce(Return(std::move(federated_matches)));
  EXPECT_CALL(*form_manager, GetURL())
      .WillOnce(testing::ReturnRef(saved_match_.url));
  return form_manager;
}

void ManagePasswordsStateTest::TestNoisyUpdates() {
  const std::vector<raw_ptr<const PasswordForm, VectorExperimental>> forms =
      GetRawPointers(passwords_data_.GetCurrentForms());
  const password_manager::ui::State state = passwords_data_.state();
  const url::Origin origin = passwords_data_.origin();

  // Push "Add".
  PasswordForm form;
  form.url = GURL("http://3rdparty.com");
  form.username_value = u"username";
  form.password_value = u"12345";
  PasswordStoreChange change(PasswordStoreChange::ADD, form);
  PasswordStoreChangeList list(1, change);
  passwords_data().ProcessLoginsChanged(list);
  EXPECT_EQ(forms, GetRawPointers(passwords_data().GetCurrentForms()));
  EXPECT_EQ(state, passwords_data().state());
  EXPECT_EQ(origin, passwords_data().origin());

  // Update the form.
  form.password_value = u"password";
  list[0] = PasswordStoreChange(PasswordStoreChange::UPDATE, form);
  passwords_data().ProcessLoginsChanged(list);
  EXPECT_EQ(forms, GetRawPointers(passwords_data().GetCurrentForms()));
  EXPECT_EQ(state, passwords_data().state());
  EXPECT_EQ(origin, passwords_data().origin());

  // Delete the form.
  list[0] = PasswordStoreChange(PasswordStoreChange::REMOVE, form);
  passwords_data().ProcessLoginsChanged(list);
  EXPECT_EQ(forms, GetRawPointers(passwords_data().GetCurrentForms()));
  EXPECT_EQ(state, passwords_data().state());
  EXPECT_EQ(origin, passwords_data().origin());
}

void ManagePasswordsStateTest::TestAllUpdates() {
  const std::vector<raw_ptr<const PasswordForm, VectorExperimental>> forms =
      GetRawPointers(passwords_data_.GetCurrentForms());
  const password_manager::ui::State state = passwords_data_.state();
  const url::Origin origin = passwords_data_.origin();
  EXPECT_NE(url::Origin(), origin);

  // Push "Add".
  PasswordForm form;
  GURL::Replacements replace_path;
  replace_path.SetPathStr("absolutely_different_path");
  form.url = origin.GetURL().ReplaceComponents(replace_path);
  form.signon_realm = form.url.DeprecatedGetOriginAsURL().spec();
  form.username_value = u"user15";
  form.password_value = u"12345";
  PasswordStoreChange change(PasswordStoreChange::ADD, form);
  PasswordStoreChangeList list(1, change);
  EXPECT_CALL(mock_client_, UpdateFormManagers()).Times(0);
  passwords_data().ProcessLoginsChanged(list);
  EXPECT_THAT(passwords_data().GetCurrentForms(), Contains(Pointee(form)));
  EXPECT_EQ(state, passwords_data().state());
  EXPECT_EQ(origin, passwords_data().origin());
  Mock::VerifyAndClearExpectations(&mock_client_);

  // Remove and Add form.
  list[0] = PasswordStoreChange(PasswordStoreChange::REMOVE, form);
  form.username_value = u"user15";
  list.push_back(PasswordStoreChange(PasswordStoreChange::ADD, form));
  EXPECT_CALL(mock_client_, UpdateFormManagers()).Times(0);
  passwords_data().ProcessLoginsChanged(list);
  EXPECT_EQ(state, passwords_data().state());
  EXPECT_EQ(origin, passwords_data().origin());
  Mock::VerifyAndClearExpectations(&mock_client_);
  list.erase(++list.begin());

  // Update the form.
  form.password_value = u"password";
  list[0] = PasswordStoreChange(PasswordStoreChange::UPDATE, form);
  EXPECT_CALL(mock_client_, UpdateFormManagers()).Times(0);
  passwords_data().ProcessLoginsChanged(list);
  EXPECT_THAT(passwords_data().GetCurrentForms(), Contains(Pointee(form)));
  EXPECT_EQ(state, passwords_data().state());
  EXPECT_EQ(origin, passwords_data().origin());
  Mock::VerifyAndClearExpectations(&mock_client_);

  // Delete the form.
  list[0] = PasswordStoreChange(PasswordStoreChange::REMOVE, form);
  EXPECT_CALL(mock_client_, UpdateFormManagers());
  passwords_data().ProcessLoginsChanged(list);
  EXPECT_EQ(forms, GetRawPointers(passwords_data().GetCurrentForms()));
  EXPECT_EQ(state, passwords_data().state());
  EXPECT_EQ(origin, passwords_data().origin());
  Mock::VerifyAndClearExpectations(&mock_client_);

  TestNoisyUpdates();
}

void ManagePasswordsStateTest::TestBlocklistedUpdates() {
  const std::vector<raw_ptr<const PasswordForm, VectorExperimental>> forms =
      GetRawPointers(passwords_data_.GetCurrentForms());
  const password_manager::ui::State state = passwords_data_.state();
  const url::Origin origin = passwords_data_.origin();
  EXPECT_FALSE(origin.opaque());

  // Process the blocked form.
  PasswordForm blocked_form;
  blocked_form.blocked_by_user = true;
  blocked_form.url = origin.GetURL();
  PasswordStoreChangeList list;
  list.push_back(PasswordStoreChange(PasswordStoreChange::ADD, blocked_form));
  passwords_data().ProcessLoginsChanged(list);
  EXPECT_EQ(forms, GetRawPointers(passwords_data().GetCurrentForms()));
  EXPECT_EQ(state, passwords_data().state());
  EXPECT_EQ(origin, passwords_data().origin());

  // Delete the blocked form.
  list[0] = PasswordStoreChange(PasswordStoreChange::REMOVE, blocked_form);
  passwords_data().ProcessLoginsChanged(list);
  EXPECT_EQ(forms, GetRawPointers(passwords_data().GetCurrentForms()));
  EXPECT_EQ(state, passwords_data().state());
  EXPECT_EQ(origin, passwords_data().origin());
}

TEST_F(ManagePasswordsStateTest, DefaultState) {
  EXPECT_THAT(passwords_data().GetCurrentForms(), IsEmpty());
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, passwords_data().state());
  EXPECT_TRUE(passwords_data().origin().opaque());
  EXPECT_FALSE(passwords_data().form_manager());

  TestNoisyUpdates();
}

TEST_F(ManagePasswordsStateTest, PasswordSubmitted) {
  std::vector<PasswordForm> best_matches = {saved_match(), psl_match()};
  std::unique_ptr<MockPasswordFormManagerForUI> test_form_manager(
      CreateFormManager(best_matches, {}));
  passwords_data().OnPendingPassword(std::move(test_form_manager));

  EXPECT_THAT(passwords_data().GetCurrentForms(),
              ElementsAre(Pointee(saved_match())));
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE,
            passwords_data().state());
  EXPECT_EQ(url::Origin::Create(GURL(kTestOrigin)), passwords_data().origin());
  ASSERT_TRUE(passwords_data().form_manager());
  TestAllUpdates();
}

TEST_F(ManagePasswordsStateTest, PasswordSaved) {
  std::vector<PasswordForm> best_matches = {saved_match()};
  std::unique_ptr<MockPasswordFormManagerForUI> test_form_manager(
      CreateFormManager(best_matches, {}));

  passwords_data().OnPendingPassword(std::move(test_form_manager));
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE,
            passwords_data().state());

  passwords_data().TransitionToState(password_manager::ui::MANAGE_STATE);
  EXPECT_THAT(passwords_data().GetCurrentForms(),
              ElementsAre(Pointee(saved_match())));
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, passwords_data().state());
  EXPECT_EQ(url::Origin::Create(GURL(kTestOrigin)), passwords_data().origin());
  TestAllUpdates();
}

TEST_F(ManagePasswordsStateTest, PasswordSubmittedFederationsPresent) {
  std::unique_ptr<MockPasswordFormManagerForUI> test_form_manager(
      CreateFormManager({}, {local_federated_form()}));
  passwords_data().OnPendingPassword(std::move(test_form_manager));

  EXPECT_THAT(passwords_data().GetCurrentForms(),
              ElementsAre(Pointee(local_federated_form())));
}

TEST_F(ManagePasswordsStateTest, OnRequestCredentials) {
  std::vector<std::unique_ptr<PasswordForm>> local_credentials;
  local_credentials.emplace_back(new PasswordForm(saved_match()));
  const url::Origin origin = url::Origin::Create(saved_match().url);
  passwords_data().OnRequestCredentials(std::move(local_credentials), origin);
  base::MockCallback<ManagePasswordsState::CredentialsCallback> callback;
  passwords_data().set_credentials_callback(callback.Get());
  EXPECT_THAT(passwords_data().GetCurrentForms(),
              ElementsAre(Pointee(saved_match())));
  EXPECT_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE,
            passwords_data().state());
  EXPECT_EQ(origin, passwords_data().origin());
  TestAllUpdates();

  EXPECT_CALL(callback, Run(nullptr));
  passwords_data().TransitionToState(password_manager::ui::MANAGE_STATE);
  EXPECT_TRUE(passwords_data().credentials_callback().is_null());
  EXPECT_THAT(passwords_data().GetCurrentForms(),
              ElementsAre(Pointee(saved_match())));
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, passwords_data().state());
  EXPECT_EQ(origin, passwords_data().origin());
  TestAllUpdates();
}

TEST_F(ManagePasswordsStateTest, AutoSignin) {
  std::vector<std::unique_ptr<PasswordForm>> local_credentials;
  local_credentials.emplace_back(new PasswordForm(saved_match()));
  passwords_data().OnAutoSignin(std::move(local_credentials),
                                url::Origin::Create(saved_match().url));
  EXPECT_THAT(passwords_data().GetCurrentForms(),
              ElementsAre(Pointee(saved_match())));
  EXPECT_EQ(password_manager::ui::AUTO_SIGNIN_STATE, passwords_data().state());
  EXPECT_EQ(url::Origin::Create(saved_match().url), passwords_data().origin());
  TestAllUpdates();

  passwords_data().TransitionToState(password_manager::ui::MANAGE_STATE);
  EXPECT_THAT(passwords_data().GetCurrentForms(),
              ElementsAre(Pointee(saved_match())));
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, passwords_data().state());
  EXPECT_EQ(url::Origin::Create(saved_match().url), passwords_data().origin());
  TestAllUpdates();
}

TEST_F(ManagePasswordsStateTest, AutomaticPasswordSave) {
  std::vector<PasswordForm> best_matches = {saved_match(), psl_match()};
  std::unique_ptr<MockPasswordFormManagerForUI> test_form_manager(
      CreateFormManager(best_matches, {}));

  passwords_data().OnAutomaticPasswordSave(std::move(test_form_manager));
  EXPECT_EQ(password_manager::ui::SAVE_CONFIRMATION_STATE,
            passwords_data().state());
  EXPECT_EQ(url::Origin::Create(GURL(kTestOrigin)), passwords_data().origin());
  ASSERT_TRUE(passwords_data().form_manager());
  TestAllUpdates();

  passwords_data().TransitionToState(password_manager::ui::MANAGE_STATE);
  EXPECT_THAT(passwords_data().GetCurrentForms(),
              ElementsAre(Pointee(saved_match())));
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, passwords_data().state());
  EXPECT_EQ(url::Origin::Create(GURL(kTestOrigin)), passwords_data().origin());
  TestAllUpdates();
}

TEST_F(ManagePasswordsStateTest, AutomaticPasswordSaveWithFederations) {
  std::vector<PasswordForm> best_matches = {saved_match()};
  std::unique_ptr<MockPasswordFormManagerForUI> test_form_manager(
      CreateFormManager(best_matches, {local_federated_form()}));

  passwords_data().OnAutomaticPasswordSave(std::move(test_form_manager));
  EXPECT_THAT(passwords_data().GetCurrentForms(),
              UnorderedElementsAre(Pointee(saved_match()),
                                   Pointee(local_federated_form())));
}

TEST_F(ManagePasswordsStateTest, PasswordAutofilled) {
  std::vector<PasswordForm> password_forms = {saved_match()};
  const url::Origin origin = url::Origin::Create(GURL(kTestOrigin));
  passwords_data().OnPasswordAutofilled(password_forms, origin, {});

  EXPECT_THAT(passwords_data().GetCurrentForms(),
              ElementsAre(Pointee(saved_match())));
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, passwords_data().state());
  EXPECT_EQ(origin, passwords_data().origin());

  TestAllUpdates();
}

TEST_F(ManagePasswordsStateTest, PasswordAutofillWithSavedFederations) {
  std::vector<PasswordForm> password_forms = {saved_match()};
  const url::Origin origin = url::Origin::Create(GURL(kTestOrigin));
  std::vector<PasswordForm> federated = {local_federated_form()};
  passwords_data().OnPasswordAutofilled(password_forms, origin, federated);

  // |federated| represents the locally saved federations. These are bundled in
  // the "current forms".
  EXPECT_THAT(passwords_data().GetCurrentForms(),
              UnorderedElementsAre(Pointee(saved_match()),
                                   Pointee(local_federated_form())));
  // |federated_credentials_forms()| do not refer to the saved federations.
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, passwords_data().state());
}

TEST_F(ManagePasswordsStateTest, PasswordAutofillWithOnlyFederations) {
  const url::Origin origin = url::Origin::Create(GURL(kTestOrigin));
  std::vector<PasswordForm> federated = {local_federated_form()};
  passwords_data().OnPasswordAutofilled({}, origin, federated);

  EXPECT_THAT(passwords_data().GetCurrentForms(),
              ElementsAre(Pointee(local_federated_form())));

  EXPECT_EQ(password_manager::ui::MANAGE_STATE, passwords_data().state());
}

TEST_F(ManagePasswordsStateTest, ActiveOnMixedPSLAndNonPSLMatched) {
  std::vector<PasswordForm> password_forms = {saved_match(), psl_match()};
  const url::Origin origin = url::Origin::Create(GURL(kTestOrigin));
  passwords_data().OnPasswordAutofilled(password_forms, origin, {});

  EXPECT_THAT(passwords_data().GetCurrentForms(),
              ElementsAre(Pointee(saved_match())));
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, passwords_data().state());
  EXPECT_EQ(origin, passwords_data().origin());

  TestAllUpdates();
}

TEST_F(ManagePasswordsStateTest, InactiveOnPSLMatched) {
  std::vector<PasswordForm> password_forms = {psl_match()};
  passwords_data().OnPasswordAutofilled(
      password_forms, url::Origin::Create(GURL(kTestOrigin)), {});

  EXPECT_THAT(passwords_data().GetCurrentForms(), IsEmpty());
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, passwords_data().state());
  EXPECT_TRUE(passwords_data().origin().opaque());
  EXPECT_FALSE(passwords_data().form_manager());
}

TEST_F(ManagePasswordsStateTest, OnInactive) {
  std::unique_ptr<MockPasswordFormManagerForUI> test_form_manager(
      CreateFormManager({}, {}));

  passwords_data().OnPendingPassword(std::move(test_form_manager));
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE,
            passwords_data().state());
  passwords_data().OnInactive();
  EXPECT_THAT(passwords_data().GetCurrentForms(), IsEmpty());
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, passwords_data().state());
  EXPECT_TRUE(passwords_data().origin().opaque());
  EXPECT_FALSE(passwords_data().form_manager());
  TestNoisyUpdates();
}

TEST_F(ManagePasswordsStateTest, PendingPasswordAddBlocklisted) {
  std::unique_ptr<MockPasswordFormManagerForUI> test_form_manager(
      CreateFormManager({}, {}));
  passwords_data().OnPendingPassword(std::move(test_form_manager));
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE,
            passwords_data().state());

  TestBlocklistedUpdates();
}

TEST_F(ManagePasswordsStateTest, DefaultStoreChanged) {
  std::vector<PasswordForm> best_matches = {saved_match(), psl_match()};
  std::unique_ptr<MockPasswordFormManagerForUI> test_form_manager(
      CreateFormManager(best_matches, {}));
  passwords_data().OnDefaultStoreChanged(std::move(test_form_manager));

  EXPECT_THAT(passwords_data().GetCurrentForms(),
              ElementsAre(Pointee(saved_match())));
  EXPECT_EQ(password_manager::ui::PASSWORD_STORE_CHANGED_BUBBLE_STATE,
            passwords_data().state());
  EXPECT_EQ(url::Origin::Create(GURL(kTestOrigin)), passwords_data().origin());
  ASSERT_TRUE(passwords_data().form_manager());
  TestAllUpdates();
}

TEST_F(ManagePasswordsStateTest, RequestCredentialsAddBlocklisted) {
  std::vector<std::unique_ptr<PasswordForm>> local_credentials;
  local_credentials.emplace_back(new PasswordForm(saved_match()));
  const url::Origin origin = url::Origin::Create(saved_match().url);
  passwords_data().OnRequestCredentials(std::move(local_credentials), origin);
  base::MockCallback<ManagePasswordsState::CredentialsCallback> callback;
  passwords_data().set_credentials_callback(callback.Get());
  EXPECT_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE,
            passwords_data().state());

  TestBlocklistedUpdates();
}

TEST_F(ManagePasswordsStateTest, AutoSigninAddBlocklisted) {
  std::vector<std::unique_ptr<PasswordForm>> local_credentials;
  local_credentials.emplace_back(new PasswordForm(saved_match()));
  passwords_data().OnAutoSignin(std::move(local_credentials),
                                url::Origin::Create(saved_match().url));
  EXPECT_EQ(password_manager::ui::AUTO_SIGNIN_STATE, passwords_data().state());

  TestBlocklistedUpdates();
}

TEST_F(ManagePasswordsStateTest, AutomaticPasswordSaveAddBlocklisted) {
  std::unique_ptr<MockPasswordFormManagerForUI> test_form_manager(
      CreateFormManager({}, {}));
  passwords_data().OnAutomaticPasswordSave(std::move(test_form_manager));
  EXPECT_EQ(password_manager::ui::SAVE_CONFIRMATION_STATE,
            passwords_data().state());

  TestBlocklistedUpdates();
}

TEST_F(ManagePasswordsStateTest, BackgroundAutofilledAddBlocklisted) {
  std::vector<PasswordForm> password_forms = {saved_match()};
  passwords_data().OnPasswordAutofilled(
      password_forms, url::Origin::Create(password_forms.front().url), {});
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, passwords_data().state());

  TestBlocklistedUpdates();
}

TEST_F(ManagePasswordsStateTest, PasswordUpdateAddBlocklisted) {
  std::vector<PasswordForm> best_matches = {saved_match()};
  std::unique_ptr<MockPasswordFormManagerForUI> test_form_manager(
      CreateFormManager(best_matches, {}));
  passwords_data().OnUpdatePassword(std::move(test_form_manager));
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_UPDATE_STATE,
            passwords_data().state());

  TestBlocklistedUpdates();
}

TEST_F(ManagePasswordsStateTest, PasswordUpdateSubmitted) {
  std::vector<PasswordForm> best_matches = {saved_match(), psl_match()};
  std::unique_ptr<MockPasswordFormManagerForUI> test_form_manager(
      CreateFormManager(best_matches, {}));
  passwords_data().OnUpdatePassword(std::move(test_form_manager));

  EXPECT_THAT(passwords_data().GetCurrentForms(),
              ElementsAre(Pointee(saved_match())));
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_UPDATE_STATE,
            passwords_data().state());
  EXPECT_EQ(url::Origin::Create(GURL(kTestOrigin)), passwords_data().origin());
  ASSERT_TRUE(passwords_data().form_manager());
  TestAllUpdates();
}

TEST_F(ManagePasswordsStateTest, AndroidPasswordUpdateSubmitted) {
  PasswordForm android_form;
  android_form.signon_realm = "android://dHJhc2g=@com.example.android/";
  android_form.url = GURL(android_form.signon_realm);
  android_form.username_value = u"username";
  android_form.password_value = u"old pass";
  android_form.match_type = PasswordForm::MatchType::kAffiliated;
  std::vector<PasswordForm> best_matches = {android_form};
  std::unique_ptr<MockPasswordFormManagerForUI> test_form_manager(
      CreateFormManager(best_matches, {}));
  passwords_data().OnUpdatePassword(std::move(test_form_manager));

  EXPECT_THAT(passwords_data().GetCurrentForms(),
              ElementsAre(Pointee(android_form)));
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_UPDATE_STATE,
            passwords_data().state());
  EXPECT_EQ(url::Origin::Create(GURL(kTestOrigin)), passwords_data().origin());
  ASSERT_TRUE(passwords_data().form_manager());
  TestAllUpdates();
}

TEST_F(ManagePasswordsStateTest, PasswordUpdateSubmittedWithFederations) {
  std::vector<PasswordForm> best_matches = {saved_match()};
  std::unique_ptr<MockPasswordFormManagerForUI> test_form_manager(
      CreateFormManager(best_matches, {local_federated_form()}));
  passwords_data().OnUpdatePassword(std::move(test_form_manager));

  EXPECT_THAT(passwords_data().GetCurrentForms(),
              UnorderedElementsAre(Pointee(saved_match()),
                                   Pointee(local_federated_form())));
}

TEST_F(ManagePasswordsStateTest, ChooseCredentialLocal) {
  passwords_data().OnRequestCredentials(
      std::vector<std::unique_ptr<PasswordForm>>(),
      url::Origin::Create(saved_match().url));
  base::MockCallback<ManagePasswordsState::CredentialsCallback> callback;
  passwords_data().set_credentials_callback(callback.Get());
  EXPECT_CALL(callback, Run(&saved_match()));
  passwords_data().ChooseCredential(&saved_match());
}

TEST_F(ManagePasswordsStateTest, ChooseCredentialEmpty) {
  passwords_data().OnRequestCredentials(
      std::vector<std::unique_ptr<PasswordForm>>(),
      url::Origin::Create(saved_match().url));
  base::MockCallback<ManagePasswordsState::CredentialsCallback> callback;
  passwords_data().set_credentials_callback(callback.Get());
  EXPECT_CALL(callback, Run(nullptr));
  passwords_data().ChooseCredential(nullptr);
}

TEST_F(ManagePasswordsStateTest, ChooseCredentialLocalWithNonEmptyFederation) {
  passwords_data().OnRequestCredentials(
      std::vector<std::unique_ptr<PasswordForm>>(),
      url::Origin::Create(saved_match().url));
  base::MockCallback<ManagePasswordsState::CredentialsCallback> callback;
  passwords_data().set_credentials_callback(callback.Get());
  EXPECT_CALL(callback, Run(&local_federated_form()));
  passwords_data().ChooseCredential(&local_federated_form());
}

TEST_F(ManagePasswordsStateTest, AutofillCausedByInternalFormManager) {
  struct OwningPasswordFormManagerForUI : public MockPasswordFormManagerForUI {
    GURL url;
    std::vector<password_manager::PasswordForm> best_matches;
    std::vector<password_manager::PasswordForm> federated_matches;

    const GURL& GetURL() const override { return url; }
    base::span<const PasswordForm> GetBestMatches() const override {
      return best_matches;
    }
    base::span<const password_manager::PasswordForm> GetFederatedMatches()
        const override {
      return federated_matches;
    }
  };

  auto test_form_manager = std::make_unique<OwningPasswordFormManagerForUI>();
  auto* weak_manager = test_form_manager.get();
  test_form_manager->url = saved_match().url;
  test_form_manager->best_matches = {saved_match()};
  test_form_manager->federated_matches = {local_federated_form()};
  passwords_data().OnPendingPassword(std::move(test_form_manager));

  // Force autofill with the parameters coming from the object to be destroyed.
  passwords_data().OnPasswordAutofilled(weak_manager->best_matches,
                                        url::Origin::Create(weak_manager->url),
                                        weak_manager->federated_matches);
  EXPECT_THAT(passwords_data().GetCurrentForms(),
              UnorderedElementsAre(Pointee(local_federated_form()),
                                   Pointee(saved_match())));
  EXPECT_EQ(url::Origin::Create(saved_match().url), passwords_data().origin());
}

TEST_F(ManagePasswordsStateTest, ProcessUnsyncedCredentialsWillBeDeleted) {
  std::vector<PasswordForm> unsynced_credentials(1);
  unsynced_credentials[0].username_value = u"user";
  unsynced_credentials[0].password_value = u"password";
  passwords_data().ProcessUnsyncedCredentialsWillBeDeleted(
      unsynced_credentials);
  EXPECT_EQ(passwords_data().state(),
            password_manager::ui::WILL_DELETE_UNSYNCED_ACCOUNT_PASSWORDS_STATE);
  EXPECT_EQ(passwords_data().unsynced_credentials(), unsynced_credentials);
}

TEST_F(ManagePasswordsStateTest, OnMovablePasswordSubmitted) {
  std::vector<PasswordForm> password_forms = {saved_match()};
  std::vector<PasswordForm> federated_matches = {local_federated_form()};

  passwords_data().OnPasswordMovable(
      CreateFormManager(password_forms, federated_matches));

  EXPECT_THAT(
      passwords_data().GetCurrentForms(),
      ElementsAre(Pointee(saved_match()), Pointee(local_federated_form())));
  EXPECT_EQ(passwords_data().state(),
            password_manager::ui::MOVE_CREDENTIAL_AFTER_LOG_IN_STATE);
  EXPECT_EQ(passwords_data().origin(), url::Origin::Create(GURL(kTestOrigin)));

  TestAllUpdates();
}

TEST_F(ManagePasswordsStateTest, OnKeychainError) {
  passwords_data().OnKeychainError();
  EXPECT_EQ(password_manager::ui::KEYCHAIN_ERROR_STATE,
            passwords_data().state());
}

TEST_F(ManagePasswordsStateTest, OpenPasswordDetailsBubble) {
  PasswordForm form;
  form.username_value = u"user";
  form.password_value = u"passw0rd";
  form.signon_realm = "https://google.com/";
  form.url = GURL("https://google.com");

  passwords_data().OpenPasswordDetailsBubble(form);

  EXPECT_EQ(passwords_data().state(), password_manager::ui::MANAGE_STATE);
  EXPECT_EQ(passwords_data().single_credential_mode_credential(), form);
  EXPECT_TRUE(passwords_data().origin().GetURL().is_empty());
}
}  // namespace
