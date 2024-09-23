// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credential_manager_pending_prevent_silent_access_task.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/password_manager/core/browser/affiliation/mock_affiliated_match_helper.h"
#include "components/password_manager/core/browser/form_parsing/form_data_parser.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

using testing::NiceMock;

constexpr const char kUrl[] = "https://www.example.com/";
constexpr const char kUnrelatedUrl[] = "https://www.foo.com/";
constexpr const char kGroupedMatchUrl[] = "https://www.grouped.com/";

class CredentialManagerPendingPreventSilentAccessTaskDelegateMock
    : public CredentialManagerPendingPreventSilentAccessTaskDelegate {
 public:
  CredentialManagerPendingPreventSilentAccessTaskDelegateMock() = default;
  ~CredentialManagerPendingPreventSilentAccessTaskDelegateMock() override =
      default;

  MOCK_METHOD(PasswordStoreInterface*, GetProfilePasswordStore, (), (override));
  MOCK_METHOD(PasswordStoreInterface*, GetAccountPasswordStore, (), (override));
  MOCK_METHOD(void, DoneRequiringUserMediation, (), (override));
};

// Checks that `PasswordStore::GetLogins` returns expected number of password
// forms and that the `PasswordForm::skip_zero_click` is populated correctly.
class PasswordStoreLoginsUpdateHelper : public PasswordStoreConsumer {
 public:
  PasswordStoreLoginsUpdateHelper(size_t expected_logins_num,
                                  bool skip_zero_click)
      : expected_logins_num_(expected_logins_num),
        skip_zero_click_(skip_zero_click) {}
  ~PasswordStoreLoginsUpdateHelper() override = default;

  base::WeakPtr<PasswordStoreLoginsUpdateHelper> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override {
    EXPECT_EQ(results.size(), expected_logins_num_);
    for (const auto& form : results) {
      EXPECT_EQ(form->skip_zero_click, skip_zero_click_);
    }
  }

  const size_t expected_logins_num_;
  const bool skip_zero_click_;

  base::WeakPtrFactory<PasswordStoreLoginsUpdateHelper> weak_ptr_factory_{this};
};

}  // namespace

class CredentialManagerPendingPreventSilentAccessTaskTest
    : public ::testing::Test {
 public:
  CredentialManagerPendingPreventSilentAccessTaskTest() {
    auto profile_store_match_helper =
        std::make_unique<NiceMock<MockAffiliatedMatchHelper>>(
            affiliation_service_.get());
    mock_affiliated_match_helper_ = profile_store_match_helper.get();

    profile_store_ = new TestPasswordStore(IsAccountStore(false));
    profile_store_->Init(
        /*prefs=*/nullptr,
        /*affiliated_match_helper=*/std::move(profile_store_match_helper));

    account_store_ = new TestPasswordStore(IsAccountStore(true));
    account_store_->Init(/*prefs=*/nullptr,
                         /*affiliated_match_helper=*/nullptr);
  }

  ~CredentialManagerPendingPreventSilentAccessTaskTest() override {
    mock_affiliated_match_helper_ = nullptr;

    account_store_->ShutdownOnUIThread();
    profile_store_->ShutdownOnUIThread();
    // It's needed to cleanup the password store asynchronously.
    ProcessPasswordStoreUpdates();
  }

  void ProcessPasswordStoreUpdates() { task_environment_.RunUntilIdle(); }

  MockAffiliatedMatchHelper& affiliated_match_helper() {
    return *mock_affiliated_match_helper_;
  }

 protected:
  testing::NiceMock<CredentialManagerPendingPreventSilentAccessTaskDelegateMock>
      delegate_mock_;
  scoped_refptr<TestPasswordStore> profile_store_;
  scoped_refptr<TestPasswordStore> account_store_;
  std::unique_ptr<affiliations::FakeAffiliationService> affiliation_service_ =
      std::make_unique<affiliations::FakeAffiliationService>();
  raw_ptr<NiceMock<MockAffiliatedMatchHelper>> mock_affiliated_match_helper_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

// Verify that the delegate is notified when the credentials are fetched from
// the password store.
TEST_F(CredentialManagerPendingPreventSilentAccessTaskTest,
       NotifiesDelegate_ProfileStoreOnly) {
  ON_CALL(delegate_mock_, GetProfilePasswordStore)
      .WillByDefault(testing::Return(profile_store_.get()));
  ON_CALL(delegate_mock_, GetAccountPasswordStore)
      .WillByDefault(testing::Return(nullptr));

  // We are expecting results from only one store, delegate should be called
  // upon getting a response from the store.
  EXPECT_CALL(delegate_mock_, DoneRequiringUserMediation);

  CredentialManagerPendingPreventSilentAccessTask task(&delegate_mock_);
  task.AddOrigin(PasswordFormDigest(PasswordForm::Scheme::kHtml,
                                    GetSignonRealm(GURL(kUrl)), GURL(kUrl)));
  ProcessPasswordStoreUpdates();
}

// Verify that the delegate is nofitied only once when credentials are fetched
// from both the profile and account password stores.
TEST_F(CredentialManagerPendingPreventSilentAccessTaskTest,
       NotifiesDelegate_ProfileAndAccountStores) {
  ON_CALL(delegate_mock_, GetProfilePasswordStore)
      .WillByDefault(testing::Return(profile_store_.get()));
  ON_CALL(delegate_mock_, GetAccountPasswordStore)
      .WillByDefault(testing::Return(account_store_.get()));

  // We are expecting results from 2 stores, the delegate should be called only
  // once after both stores return logins.
  EXPECT_CALL(delegate_mock_, DoneRequiringUserMediation);

  CredentialManagerPendingPreventSilentAccessTask task(&delegate_mock_);
  task.AddOrigin(PasswordFormDigest(PasswordForm::Scheme::kHtml,
                                    GetSignonRealm(GURL(kUrl)), GURL(kUrl)));
  ProcessPasswordStoreUpdates();
}

// Verify that the `PasswordForm::skip_zero_click` is populated correctly for
// the passwords with matching domain.
TEST_F(CredentialManagerPendingPreventSilentAccessTaskTest,
       SameDomain_SetsSkipZeroClick) {
  ON_CALL(delegate_mock_, GetProfilePasswordStore)
      .WillByDefault(testing::Return(profile_store_.get()));
  ON_CALL(delegate_mock_, GetAccountPasswordStore)
      .WillByDefault(testing::Return(nullptr));

  PasswordForm form = CreateEntry("username", "password", GURL(kUrl),
                                  PasswordForm::MatchType::kExact);
  profile_store_->AddLogin(form);
  ProcessPasswordStoreUpdates();

  CredentialManagerPendingPreventSilentAccessTask task(&delegate_mock_);
  task.AddOrigin(PasswordFormDigest(form));
  ProcessPasswordStoreUpdates();

  PasswordStoreLoginsUpdateHelper helper(/*expected_logins_num=*/1,
                                         /*silent_access_disabled=*/true);
  profile_store_->GetLogins(PasswordFormDigest(form), helper.GetWeakPtr());
  ProcessPasswordStoreUpdates();
}

// Verify that the `PasswordForm::skip_zero_click` is not populated for the
// unrelated passwords.
TEST_F(CredentialManagerPendingPreventSilentAccessTaskTest,
       DifferentDomain_NoFormUpdates) {
  ON_CALL(delegate_mock_, GetProfilePasswordStore)
      .WillByDefault(testing::Return(profile_store_.get()));
  ON_CALL(delegate_mock_, GetAccountPasswordStore)
      .WillByDefault(testing::Return(nullptr));

  PasswordForm form = CreateEntry("username", "password", GURL(kUrl),
                                  PasswordForm::MatchType::kExact);
  profile_store_->AddLogin(form);
  ProcessPasswordStoreUpdates();

  const GURL kDifferentDomainUrl = GURL(kUnrelatedUrl);
  CredentialManagerPendingPreventSilentAccessTask task(&delegate_mock_);
  task.AddOrigin(PasswordFormDigest(PasswordForm::Scheme::kHtml,
                                    GetSignonRealm(kDifferentDomainUrl),
                                    kDifferentDomainUrl));
  ProcessPasswordStoreUpdates();

  PasswordStoreLoginsUpdateHelper helper(/*expected_logins_num=*/1,
                                         /*silent_access_disabled=*/false);
  profile_store_->GetLogins(PasswordFormDigest(form), helper.GetWeakPtr());
  ProcessPasswordStoreUpdates();
}

TEST_F(CredentialManagerPendingPreventSilentAccessTaskTest,
       IngoresGroupedCredentials) {
  ON_CALL(delegate_mock_, GetProfilePasswordStore)
      .WillByDefault(testing::Return(profile_store_.get()));
  ON_CALL(delegate_mock_, GetAccountPasswordStore)
      .WillByDefault(testing::Return(nullptr));

  PasswordForm form =
      CreateEntry("username", "password", GURL(kGroupedMatchUrl),
                  PasswordForm::MatchType::kExact);
  profile_store_->AddLogin(form);
  ProcessPasswordStoreUpdates();

  const PasswordFormDigest kDigest(PasswordForm::Scheme::kHtml,
                                   GetSignonRealm(GURL(kUrl)), GURL(kUrl));
  // Register `kGroupedMatchUrl` domain as weakly affiliated with `kUrl`.
  affiliated_match_helper().ExpectCallToGetAffiliatedAndGrouped(
      kDigest, {kUrl}, {kGroupedMatchUrl});
  CredentialManagerPendingPreventSilentAccessTask task(&delegate_mock_);
  task.AddOrigin(kDigest);
  ProcessPasswordStoreUpdates();
  // `AffiliatedMatchHelper` will be queried for the second time to check that
  // the credential was not modified. Need to verify and clear expectations
  // before that happens.
  testing::Mock::VerifyAndClearExpectations(&affiliated_match_helper());

  PasswordStoreLoginsUpdateHelper helper(/*expected_logins_num=*/1,
                                         /*silent_access_disabled=*/false);
  profile_store_->GetLogins(PasswordFormDigest(form), helper.GetWeakPtr());
  ProcessPasswordStoreUpdates();
}

TEST_F(CredentialManagerPendingPreventSilentAccessTaskTest,
       IngoresBlockedCredentials) {
  ON_CALL(delegate_mock_, GetProfilePasswordStore)
      .WillByDefault(testing::Return(profile_store_.get()));
  ON_CALL(delegate_mock_, GetAccountPasswordStore)
      .WillByDefault(testing::Return(nullptr));

  PasswordForm form;
  form.signon_realm = kUrl;
  form.url = GURL(kUrl);
  form.blocked_by_user = true;

  profile_store_->AddLogin(form);
  ProcessPasswordStoreUpdates();

  const PasswordFormDigest kDigest(PasswordForm::Scheme::kHtml,
                                   GetSignonRealm(GURL(kUrl)), GURL(kUrl));
  // Register `kGroupedMatchUrl` domain as weakly affiliated with `kUrl`.
  CredentialManagerPendingPreventSilentAccessTask task(&delegate_mock_);
  task.AddOrigin(kDigest);
  ProcessPasswordStoreUpdates();

  PasswordStoreLoginsUpdateHelper helper(/*expected_logins_num=*/1,
                                         /*silent_access_disabled=*/false);
  profile_store_->GetLogins(PasswordFormDigest(form), helper.GetWeakPtr());
  ProcessPasswordStoreUpdates();
}

}  // namespace password_manager
