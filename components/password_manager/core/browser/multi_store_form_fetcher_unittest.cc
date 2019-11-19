// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/multi_store_form_fetcher.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

using autofill::PasswordForm;
using base::ASCIIToUTF16;
using testing::_;
using testing::IsEmpty;
using testing::Pointee;
using testing::Return;
using testing::UnorderedElementsAre;

namespace password_manager {

namespace {

constexpr const char kTestHttpURL[] = "http://example.in/";

constexpr const char kTestHttpsURL[] = "https://example.in/";
constexpr const char kTestHttpsActionURL[] = "https://login.example.org/";

constexpr const char kTestFederatedRealm[] =
    "federation://example.in/accounts.google.com";
constexpr const char kTestFederationURL[] = "https://accounts.google.com/";

class MockConsumer : public FormFetcher::Consumer {
 public:
  MOCK_METHOD0(OnFetchCompleted, void());
};

class FakePasswordManagerClient : public StubPasswordManagerClient {
 public:
  FakePasswordManagerClient() = default;
  ~FakePasswordManagerClient() override = default;

  void set_profile_store(PasswordStore* store) { profile_store_ = store; }
  void set_account_store(PasswordStore* store) { account_store_ = store; }

 private:
  PasswordStore* GetProfilePasswordStore() const override {
    return profile_store_;
  }
  PasswordStore* GetAccountPasswordStore() const override {
    return account_store_;
  }

  PasswordStore* profile_store_ = nullptr;
  PasswordStore* account_store_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakePasswordManagerClient);
};

PasswordForm CreateHTMLForm(const std::string& origin_url,
                            const std::string& username_value,
                            const std::string& password_value,
                            base::Time date_last_used) {
  PasswordForm form;
  form.scheme = PasswordForm::Scheme::kHtml;
  form.origin = GURL(origin_url);
  form.signon_realm = origin_url;
  form.username_value = ASCIIToUTF16(username_value);
  form.password_value = ASCIIToUTF16(password_value);
  form.date_last_used = date_last_used;
  return form;
}

// Creates a dummy non-federated form with some basic arbitrary values.
PasswordForm CreateNonFederated(const std::string& username_value,
                                base::Time date_last_used) {
  PasswordForm form =
      CreateHTMLForm(kTestHttpsURL, username_value, "password", date_last_used);
  form.action = GURL(kTestHttpsActionURL);
  return form;
}

// Creates a dummy federated form with some basic arbitrary values.
PasswordForm CreateFederated(const std::string& username_value,
                             base::Time date_last_used) {
  PasswordForm form = CreateNonFederated(username_value, date_last_used);
  form.signon_realm = kTestFederatedRealm;
  form.password_value.clear();
  form.federation_origin = url::Origin::Create(GURL(kTestFederationURL));
  return form;
}

// Creates an Android federated credential.
PasswordForm CreateAndroidFederated(const std::string& username_value,
                                    base::Time date_last_used) {
  PasswordForm form =
      CreateHTMLForm("android://hash@com.example.android/", username_value,
                     /*password_value=*/"", date_last_used);
  form.federation_origin = url::Origin::Create(GURL(kTestFederationURL));
  form.is_affiliation_based_match = true;
  return form;
}

// Creates a dummy blacklisted form.
PasswordForm CreateBlacklisted() {
  PasswordForm form = CreateHTMLForm(kTestHttpsURL, /*username_value=*/"",
                                     /*password_value=*/"",
                                     /*date_last_used=*/base::Time::Now());
  form.blacklisted_by_user = true;
  return form;
}

}  // namespace

class MultiStoreFormFetcherTest : public testing::Test {
 public:
  MultiStoreFormFetcherTest()
      : form_digest_(PasswordForm::Scheme::kHtml,
                     kTestHttpURL,
                     GURL(kTestHttpURL)) {
    profile_mock_store_ = new MockPasswordStore;
    profile_mock_store_->Init(syncer::SyncableService::StartSyncFlare(),
                              /*prefs=*/nullptr);
    client_.set_profile_store(profile_mock_store_.get());

    account_mock_store_ = new MockPasswordStore;
    account_mock_store_->Init(syncer::SyncableService::StartSyncFlare(),
                              /*prefs=*/nullptr);
    client_.set_account_store(account_mock_store_.get());

    form_fetcher_ = std::make_unique<MultiStoreFormFetcher>(
        form_digest_, &client_, /*should_migrate_http_passwords=*/false);
  }

  ~MultiStoreFormFetcherTest() override {
    profile_mock_store_->ShutdownOnUIThread();
    account_mock_store_->ShutdownOnUIThread();
  }

 protected:
  // A wrapper around form_fetcher_.Fetch(), adding the call expectations.
  void Fetch() {
#if !defined(OS_IOS) && !defined(OS_ANDROID)
    EXPECT_CALL(*profile_mock_store_, GetSiteStatsImpl(_))
        .WillOnce(Return(std::vector<InteractionsStats>()));
#endif
    EXPECT_CALL(*profile_mock_store_,
                GetLogins(form_digest_, form_fetcher_.get()));
    EXPECT_CALL(*account_mock_store_,
                GetLogins(form_digest_, form_fetcher_.get()));
    form_fetcher_->Fetch();
    task_environment_.RunUntilIdle();
    testing::Mock::VerifyAndClearExpectations(profile_mock_store_.get());
    testing::Mock::VerifyAndClearExpectations(account_mock_store_.get());
  }

  base::test::TaskEnvironment task_environment_;
  PasswordStore::FormDigest form_digest_;
  std::unique_ptr<MultiStoreFormFetcher> form_fetcher_;
  MockConsumer consumer_;
  scoped_refptr<MockPasswordStore> profile_mock_store_;
  scoped_refptr<MockPasswordStore> account_mock_store_;
  FakePasswordManagerClient client_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MultiStoreFormFetcherTest);
};

// Check that the absence of PasswordStore results is handled correctly.
TEST_F(MultiStoreFormFetcherTest, NoStoreResults) {
  Fetch();
  EXPECT_CALL(consumer_, OnFetchCompleted).Times(0);
  form_fetcher_->AddConsumer(&consumer_);
  EXPECT_EQ(FormFetcher::State::WAITING, form_fetcher_->GetState());
}

// Check that empty PasswordStore results are handled correctly.
TEST_F(MultiStoreFormFetcherTest, Empty) {
  Fetch();
  form_fetcher_->AddConsumer(&consumer_);
  EXPECT_CALL(consumer_, OnFetchCompleted);
  // Both profile and account respond with empty results.
  form_fetcher_->OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>>());
  // We should be still waiting for the second store to respond.
  EXPECT_EQ(FormFetcher::State::WAITING, form_fetcher_->GetState());
  form_fetcher_->OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>>());
  EXPECT_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
  EXPECT_THAT(form_fetcher_->GetNonFederatedMatches(), IsEmpty());
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(), IsEmpty());
  EXPECT_FALSE(form_fetcher_->IsBlacklisted());
}

// Check that results from both stores are merged.
TEST_F(MultiStoreFormFetcherTest, MergeFromBothStores) {
  const base::Time kLastUsedNow = base::Time::Now();
  const base::Time kLastUsedYesterday =
      kLastUsedNow - base::TimeDelta::FromDays(1);
  Fetch();
  PasswordForm federated1 = CreateFederated("user", kLastUsedNow);
  PasswordForm federated2 = CreateFederated("user_B", kLastUsedNow);
  PasswordForm federated3 = CreateAndroidFederated("user_B", kLastUsedNow);
  PasswordForm non_federated1 = CreateNonFederated("user", kLastUsedYesterday);
  PasswordForm non_federated2 =
      CreateNonFederated("user_C", kLastUsedYesterday);
  PasswordForm non_federated3 = CreateNonFederated("user_D", kLastUsedNow);
  PasswordForm blacklisted = CreateBlacklisted();

  form_fetcher_->AddConsumer(&consumer_);

  // Pass response from the first store.
  std::vector<std::unique_ptr<PasswordForm>> results;
  results.push_back(std::make_unique<PasswordForm>(federated1));
  results.push_back(std::make_unique<PasswordForm>(federated2));
  results.push_back(std::make_unique<PasswordForm>(non_federated1));
  results.push_back(std::make_unique<PasswordForm>(blacklisted));
  form_fetcher_->OnGetPasswordStoreResults(std::move(results));

  // We should be still waiting for the second store to respond.
  EXPECT_EQ(FormFetcher::State::WAITING, form_fetcher_->GetState());

  // Pass response from the second store.
  results.clear();
  results.push_back(std::make_unique<PasswordForm>(federated3));
  results.push_back(std::make_unique<PasswordForm>(non_federated2));
  results.push_back(std::make_unique<PasswordForm>(non_federated3));

  EXPECT_CALL(consumer_, OnFetchCompleted);
  form_fetcher_->OnGetPasswordStoreResults(std::move(results));

  EXPECT_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());

  // The results should be a merge of the response of both stores.
  EXPECT_THAT(
      form_fetcher_->GetNonFederatedMatches(),
      UnorderedElementsAre(Pointee(non_federated1), Pointee(non_federated2),
                           Pointee(non_federated3)));
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(),
              UnorderedElementsAre(Pointee(federated1), Pointee(federated2),
                                   Pointee(federated3)));
  EXPECT_TRUE(form_fetcher_->IsBlacklisted());
  EXPECT_THAT(form_fetcher_->GetPreferredMatch(), Pointee(non_federated3));
}

}  // namespace password_manager
