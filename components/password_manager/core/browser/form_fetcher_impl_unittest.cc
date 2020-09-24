// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_fetcher_impl.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/multi_store_form_fetcher.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "components/password_manager/core/browser/stub_credentials_filter.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "services/network/test/test_network_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

using base::ASCIIToUTF16;
using base::StringPiece;
using testing::_;
using testing::IsEmpty;
using testing::Pointee;
using testing::Return;
using testing::UnorderedElementsAre;
using testing::UnorderedElementsAreArray;
using testing::WithArg;

namespace password_manager {

namespace {

constexpr const char kTestHttpURL[] = "http://example.in/";
constexpr const char kTestHttpActionURL[] = "http://login.example.org/";

constexpr const char kTestHttpsURL[] = "https://example.in/";
constexpr const char kTestHttpsActionURL[] = "https://login.example.org/";

constexpr const char kTestFederatedRealm[] =
    "federation://example.in/accounts.google.com";
constexpr const char kTestFederationURL[] = "https://accounts.google.com/";

class MockConsumer : public FormFetcher::Consumer {
 public:
  MOCK_METHOD0(OnFetchCompleted, void());
};

// MockConsumer that takes ownership of the FormFetcher itself.
class MockOwningConsumer : public FormFetcher::Consumer {
 public:
  explicit MockOwningConsumer(std::unique_ptr<FormFetcher> form_fetcher)
      : form_fetcher_(std::move(form_fetcher)) {}

  MOCK_METHOD0(OnFetchCompleted, void());

 private:
  std::unique_ptr<FormFetcher> form_fetcher_;
};

class NameFilter : public StubCredentialsFilter {
 public:
  // This class filters out all credentials which have |name| as
  // |username_value|.
  explicit NameFilter(StringPiece name) : name_(ASCIIToUTF16(name)) {}

  ~NameFilter() override = default;

  bool ShouldSave(const PasswordForm& form) const override {
    return form.username_value != name_;
  }

 private:
  const base::string16 name_;  // |username_value| to filter

  DISALLOW_COPY_AND_ASSIGN(NameFilter);
};

class FakeNetworkContext : public network::TestNetworkContext {
 public:
  FakeNetworkContext() = default;
  void IsHSTSActiveForHost(const std::string& host,
                           IsHSTSActiveForHostCallback callback) override {
    std::move(callback).Run(false);
  }
};

class FakePasswordManagerClient : public StubPasswordManagerClient {
 public:
  FakePasswordManagerClient() = default;
  ~FakePasswordManagerClient() override = default;

  network::mojom::NetworkContext* GetNetworkContext() const override {
    return &network_context_;
  }

  void set_filter(std::unique_ptr<CredentialsFilter> filter) {
    filter_ = std::move(filter);
  }

  void set_store(PasswordStore* store) { store_ = store; }

 private:
  const CredentialsFilter* GetStoreResultFilter() const override {
    return filter_ ? filter_.get()
                   : StubPasswordManagerClient::GetStoreResultFilter();
  }

  PasswordStore* GetProfilePasswordStore() const override { return store_; }
  PasswordStore* GetAccountPasswordStore() const override { return nullptr; }

  std::unique_ptr<CredentialsFilter> filter_;
  PasswordStore* store_ = nullptr;
  mutable FakeNetworkContext network_context_;

  DISALLOW_COPY_AND_ASSIGN(FakePasswordManagerClient);
};

PasswordForm CreateHTMLForm(const char* origin_url,
                            const char* username_value,
                            const char* password_value) {
  PasswordForm form;
  form.scheme = PasswordForm::Scheme::kHtml;
  form.url = GURL(origin_url);
  form.signon_realm = origin_url;
  form.username_value = ASCIIToUTF16(username_value);
  form.password_value = ASCIIToUTF16(password_value);
  return form;
}

// Creates a dummy non-federated form with some basic arbitrary values.
PasswordForm CreateNonFederated() {
  PasswordForm form = CreateHTMLForm(kTestHttpsURL, "user", "password");
  form.action = GURL(kTestHttpsActionURL);
  return form;
}

// Creates a dummy non-federated HTTP form with some basic arbitrary values.
PasswordForm CreateHTTPNonFederated() {
  PasswordForm form = CreateHTMLForm(kTestHttpURL, "user", "password");
  form.action = GURL(kTestHttpActionURL);
  return form;
}

// Creates a dummy federated form with some basic arbitrary values.
PasswordForm CreateFederated() {
  PasswordForm form = CreateNonFederated();
  form.signon_realm = kTestFederatedRealm;
  form.password_value.clear();
  form.federation_origin = url::Origin::Create(GURL(kTestFederationURL));
  return form;
}

// Creates an Android federated credential.
PasswordForm CreateAndroidFederated() {
  PasswordForm form =
      CreateHTMLForm("android://hash@com.example.android/", "user", "");
  form.federation_origin = url::Origin::Create(GURL(kTestFederationURL));
  form.is_affiliation_based_match = true;
  return form;
}

// Creates a dummy blocked form.
PasswordForm CreateBlocked() {
  PasswordForm form = CreateHTMLForm(kTestHttpsURL, "", "");
  form.blocked_by_user = true;
  return form;
}

PasswordForm CreateBlockedPsl() {
  PasswordForm form = CreateBlocked();
  form.is_public_suffix_match = true;
  return form;
}

// Small helper that wraps passed in forms in unique ptrs.
std::vector<std::unique_ptr<PasswordForm>> MakeResults(
    const std::vector<PasswordForm>& forms) {
  std::vector<std::unique_ptr<PasswordForm>> results;
  results.reserve(forms.size());
  for (const auto& form : forms)
    results.push_back(std::make_unique<PasswordForm>(form));
  return results;
}

ACTION_P(GetAndAssignWeakPtr, ptr) {
  *ptr = arg0->GetWeakPtr();
}

}  // namespace

class FormFetcherImplTest : public testing::Test,
                            public testing::WithParamInterface<bool> {
 public:
  FormFetcherImplTest()
      : form_digest_(PasswordForm::Scheme::kHtml,
                     kTestHttpURL,
                     GURL(kTestHttpURL)) {
    mock_store_ = new testing::NiceMock<MockPasswordStore>;
    mock_store_->Init(nullptr);
    client_.set_store(mock_store_.get());

    if (!GetParam()) {
      feature_list_.InitAndDisableFeature(
          password_manager::features::kEnablePasswordsAccountStorage);
      form_fetcher_ = std::make_unique<FormFetcherImpl>(
          form_digest_, &client_, false /* should_migrate_http_passwords */);
    } else {
      feature_list_.InitAndEnableFeature(
          password_manager::features::kEnablePasswordsAccountStorage);
      form_fetcher_ = std::make_unique<MultiStoreFormFetcher>(
          form_digest_, &client_, false /* should_migrate_http_passwords */);
    }
  }

  ~FormFetcherImplTest() override { mock_store_->ShutdownOnUIThread(); }

 protected:
  // A wrapper around form_fetcher_.Fetch(), adding the call expectations.
  void Fetch() {
#if !defined(OS_IOS) && !defined(OS_ANDROID)
    EXPECT_CALL(*mock_store_, GetSiteStatsImpl(_))
        .WillOnce(Return(std::vector<InteractionsStats>()));
#endif
    EXPECT_CALL(*mock_store_, GetLogins(form_digest_, form_fetcher_.get()));
    form_fetcher_->Fetch();
    task_environment_.RunUntilIdle();
    testing::Mock::VerifyAndClearExpectations(mock_store_.get());
  }

  PasswordStoreConsumer* store_consumer() { return form_fetcher_.get(); }

  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_;
  PasswordStore::FormDigest form_digest_;
  std::unique_ptr<FormFetcherImpl> form_fetcher_;
  MockConsumer consumer_;
  scoped_refptr<MockPasswordStore> mock_store_;
  FakePasswordManagerClient client_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FormFetcherImplTest);
};

// Check that the absence of PasswordStore results is handled correctly.
TEST_P(FormFetcherImplTest, NoStoreResults) {
  Fetch();
  EXPECT_CALL(consumer_, OnFetchCompleted).Times(0);
  form_fetcher_->AddConsumer(&consumer_);
  EXPECT_EQ(FormFetcher::State::WAITING, form_fetcher_->GetState());
}

// Check that empty PasswordStore results are handled correctly.
TEST_P(FormFetcherImplTest, Empty) {
  Fetch();
  form_fetcher_->AddConsumer(&consumer_);
  EXPECT_CALL(consumer_, OnFetchCompleted);
  store_consumer()->OnGetPasswordStoreResultsFrom(
      mock_store_.get(), std::vector<std::unique_ptr<PasswordForm>>());
  EXPECT_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
  EXPECT_THAT(form_fetcher_->GetNonFederatedMatches(), IsEmpty());
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(), IsEmpty());
  EXPECT_FALSE(form_fetcher_->IsBlacklisted());
}

// Check that non-federated PasswordStore results are handled correctly.
TEST_P(FormFetcherImplTest, NonFederated) {
  Fetch();
  PasswordForm non_federated = CreateNonFederated();
  form_fetcher_->AddConsumer(&consumer_);
  std::vector<std::unique_ptr<PasswordForm>> results;
  results.push_back(std::make_unique<PasswordForm>(non_federated));
  EXPECT_CALL(consumer_, OnFetchCompleted);
  store_consumer()->OnGetPasswordStoreResultsFrom(mock_store_.get(),
                                                  std::move(results));
  EXPECT_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
  EXPECT_THAT(form_fetcher_->GetNonFederatedMatches(),
              UnorderedElementsAre(Pointee(non_federated)));
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(), IsEmpty());
  EXPECT_FALSE(form_fetcher_->IsBlacklisted());
}

// Check that federated PasswordStore results are handled correctly.
TEST_P(FormFetcherImplTest, Federated) {
  Fetch();
  PasswordForm federated = CreateFederated();
  PasswordForm android_federated = CreateAndroidFederated();
  form_fetcher_->AddConsumer(&consumer_);
  std::vector<std::unique_ptr<PasswordForm>> results;
  results.push_back(std::make_unique<PasswordForm>(federated));
  results.push_back(std::make_unique<PasswordForm>(android_federated));
  EXPECT_CALL(consumer_, OnFetchCompleted);
  store_consumer()->OnGetPasswordStoreResultsFrom(mock_store_.get(),
                                                  std::move(results));
  EXPECT_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
  EXPECT_THAT(form_fetcher_->GetNonFederatedMatches(), IsEmpty());
  EXPECT_THAT(
      form_fetcher_->GetFederatedMatches(),
      UnorderedElementsAre(Pointee(federated), Pointee(android_federated)));
  EXPECT_FALSE(form_fetcher_->IsBlacklisted());
}

// Check that blocked PasswordStore results are handled correctly. Blocked PSL
// matches in the store should be ignored and not returned as a blocked match.
TEST_P(FormFetcherImplTest, Blocked) {
  Fetch();
  PasswordForm blocked = CreateBlocked();
  PasswordForm blocked_psl = CreateBlockedPsl();
  form_fetcher_->AddConsumer(&consumer_);
  std::vector<std::unique_ptr<PasswordForm>> results;
  results.push_back(std::make_unique<PasswordForm>(blocked));
  results.push_back(std::make_unique<PasswordForm>(blocked_psl));
  EXPECT_CALL(consumer_, OnFetchCompleted);
  store_consumer()->OnGetPasswordStoreResultsFrom(mock_store_.get(),
                                                  std::move(results));
  EXPECT_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
  EXPECT_THAT(form_fetcher_->GetNonFederatedMatches(), IsEmpty());
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(), IsEmpty());
  EXPECT_TRUE(form_fetcher_->IsBlacklisted());
}

// Check that mixed PasswordStore results are handled correctly.
TEST_P(FormFetcherImplTest, Mixed) {
  Fetch();
  PasswordForm federated1 = CreateFederated();
  federated1.username_value = ASCIIToUTF16("user");
  PasswordForm federated2 = CreateFederated();
  federated2.username_value = ASCIIToUTF16("user_B");
  PasswordForm federated3 = CreateAndroidFederated();
  federated3.username_value = ASCIIToUTF16("user_B");
  PasswordForm non_federated1 = CreateNonFederated();
  non_federated1.username_value = ASCIIToUTF16("user");
  PasswordForm non_federated2 = CreateNonFederated();
  non_federated2.username_value = ASCIIToUTF16("user_C");
  PasswordForm non_federated3 = CreateNonFederated();
  non_federated3.username_value = ASCIIToUTF16("user_D");
  PasswordForm blocked = CreateBlocked();

  form_fetcher_->AddConsumer(&consumer_);
  std::vector<std::unique_ptr<PasswordForm>> results;
  results.push_back(std::make_unique<PasswordForm>(federated1));
  results.push_back(std::make_unique<PasswordForm>(federated2));
  results.push_back(std::make_unique<PasswordForm>(federated3));
  results.push_back(std::make_unique<PasswordForm>(non_federated1));
  results.push_back(std::make_unique<PasswordForm>(non_federated2));
  results.push_back(std::make_unique<PasswordForm>(non_federated3));
  results.push_back(std::make_unique<PasswordForm>(blocked));
  EXPECT_CALL(consumer_, OnFetchCompleted);
  store_consumer()->OnGetPasswordStoreResultsFrom(mock_store_.get(),
                                                  std::move(results));
  EXPECT_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
  EXPECT_THAT(
      form_fetcher_->GetNonFederatedMatches(),
      UnorderedElementsAre(Pointee(non_federated1), Pointee(non_federated2),
                           Pointee(non_federated3)));
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(),
              UnorderedElementsAre(Pointee(federated1), Pointee(federated2),
                                   Pointee(federated3)));
  EXPECT_TRUE(form_fetcher_->IsBlacklisted());
}

// Check that PasswordStore results are filtered correctly.
TEST_P(FormFetcherImplTest, Filtered) {
  Fetch();
  PasswordForm federated = CreateFederated();
  federated.username_value = ASCIIToUTF16("user");
  PasswordForm non_federated1 = CreateNonFederated();
  non_federated1.username_value = ASCIIToUTF16("user");
  PasswordForm non_federated2 = CreateNonFederated();
  non_federated2.username_value = ASCIIToUTF16("user_C");

  // Set up a filter to remove all credentials with the username "user".
  client_.set_filter(std::make_unique<NameFilter>("user"));

  form_fetcher_->AddConsumer(&consumer_);
  std::vector<std::unique_ptr<PasswordForm>> results;
  results.push_back(std::make_unique<PasswordForm>(federated));
  results.push_back(std::make_unique<PasswordForm>(non_federated1));
  results.push_back(std::make_unique<PasswordForm>(non_federated2));
  EXPECT_CALL(consumer_, OnFetchCompleted);
  store_consumer()->OnGetPasswordStoreResultsFrom(mock_store_.get(),
                                                  std::move(results));
  EXPECT_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
  // Expect that nothing got filtered out, since CredentialsFilter no longer
  // filters things out:
  EXPECT_THAT(
      form_fetcher_->GetNonFederatedMatches(),
      UnorderedElementsAre(Pointee(non_federated1), Pointee(non_federated2)));
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(),
              UnorderedElementsAre(Pointee(federated)));
}

// Check that stats from PasswordStore are handled correctly.
TEST_P(FormFetcherImplTest, Stats) {
  Fetch();
  form_fetcher_->AddConsumer(&consumer_);
  std::vector<InteractionsStats> stats(1);
  store_consumer()->OnGetSiteStatistics(std::move(stats));
  EXPECT_EQ(1u, form_fetcher_->GetInteractionsStats().size());
}

TEST_P(FormFetcherImplTest, CompromisedCredentials) {
  Fetch();
  form_fetcher_->AddConsumer(&consumer_);
  const std::vector<CompromisedCredentials> credentials = {
      {form_digest_.signon_realm, base::ASCIIToUTF16("username_value"),
       base::Time::FromTimeT(1), CompromiseType::kLeaked}};
  static_cast<CompromisedCredentialsConsumer*>(form_fetcher_.get())
      ->OnGetCompromisedCredentials(credentials);
  EXPECT_THAT(form_fetcher_->GetCompromisedCredentials(),
              UnorderedElementsAreArray(credentials));
}

// Test that multiple calls of Fetch() are handled gracefully, and that they
// always result in passing the most up-to-date information to the consumers.
TEST_P(FormFetcherImplTest, Update_Reentrance) {
  Fetch();
  form_fetcher_->AddConsumer(&consumer_);
  // The fetcher is currently waiting for a store response, after it fired a
  // GetLogins request during the Fetch() above. The second and third Fetch
  // (below) won't cause a GetLogins right now, but will ensure that a second
  // GetLogins will be called later.
  form_fetcher_->Fetch();
  form_fetcher_->Fetch();

  // First response from the store, should be ignored.
  PasswordForm form_a = CreateNonFederated();
  form_a.username_value = ASCIIToUTF16("a@gmail.com");
  std::vector<std::unique_ptr<PasswordForm>> old_results;
  old_results.push_back(std::make_unique<PasswordForm>(form_a));
  // Because of the pending updates, the old PasswordStore results are not
  // forwarded to the consumers.
  EXPECT_CALL(consumer_, OnFetchCompleted).Times(0);
  // Delivering the first results will trigger the new GetLogins call, because
  // of the Fetch() above.
  EXPECT_CALL(*mock_store_, GetLogins(form_digest_, form_fetcher_.get()));
  store_consumer()->OnGetPasswordStoreResultsFrom(mock_store_.get(),
                                                  std::move(old_results));

  // Second response from the store should not be ignored.
  PasswordForm form_b = CreateNonFederated();
  form_b.username_value = ASCIIToUTF16("b@gmail.com");

  PasswordForm form_c = CreateNonFederated();
  form_c.username_value = ASCIIToUTF16("c@gmail.com");

  EXPECT_CALL(consumer_, OnFetchCompleted);
  std::vector<std::unique_ptr<PasswordForm>> results;
  results.push_back(std::make_unique<PasswordForm>(form_b));
  results.push_back(std::make_unique<PasswordForm>(form_c));
  store_consumer()->OnGetPasswordStoreResultsFrom(mock_store_.get(),
                                                  std::move(results));
  EXPECT_THAT(form_fetcher_->GetNonFederatedMatches(),
              UnorderedElementsAre(Pointee(form_b), Pointee(form_c)));
}

#if !defined(OS_IOS) && !defined(OS_ANDROID)
TEST_P(FormFetcherImplTest, FetchStatistics) {
  InteractionsStats stats;
  stats.origin_domain = form_digest_.url.GetOrigin();
  stats.username_value = ASCIIToUTF16("some username");
  stats.dismissal_count = 5;
  std::vector<InteractionsStats> db_stats = {stats};
  EXPECT_CALL(*mock_store_, GetLogins(form_digest_, form_fetcher_.get()));
  EXPECT_CALL(*mock_store_, GetSiteStatsImpl(stats.origin_domain))
      .WillOnce(Return(db_stats));
  form_fetcher_->Fetch();
  task_environment_.RunUntilIdle();

  EXPECT_THAT(form_fetcher_->GetInteractionsStats(),
              UnorderedElementsAre(stats));
}

TEST_P(FormFetcherImplTest, FetchCompromised) {
  CompromisedCredentials credentials = {
      form_digest_.signon_realm, base::ASCIIToUTF16("username_value"),
      base::Time::FromTimeT(1), CompromiseType::kLeaked};
  std::vector<CompromisedCredentials> list = {credentials};
  EXPECT_CALL(*mock_store_,
              GetMatchingCompromisedCredentialsImpl(form_digest_.signon_realm))
      .WillOnce(Return(list));
  form_fetcher_->Fetch();
  task_environment_.RunUntilIdle();

  EXPECT_THAT(form_fetcher_->GetCompromisedCredentials(),
              UnorderedElementsAreArray(list));
}
#else
TEST_P(FormFetcherImplTest, DontFetchStatistics) {
  EXPECT_CALL(*mock_store_, GetLogins(form_digest_, form_fetcher_.get()));
  EXPECT_CALL(*mock_store_, GetSiteStatsImpl(_)).Times(0);
  form_fetcher_->Fetch();
  task_environment_.RunUntilIdle();
}

TEST_P(FormFetcherImplTest, DontFetchCompromised) {
  EXPECT_CALL(*mock_store_, GetMatchingCompromisedCredentialsImpl).Times(0);
  form_fetcher_->Fetch();
  task_environment_.RunUntilIdle();
}
#endif

// Test that ensures HTTP passwords are not migrated on HTTP sites.
TEST_P(FormFetcherImplTest, DoNotTryToMigrateHTTPPasswordsOnHTTPSites) {
  GURL::Replacements http_rep;
  http_rep.SetSchemeStr(url::kHttpScheme);
  const GURL http_url = form_digest_.url.ReplaceComponents(http_rep);
  form_digest_ = PasswordStore::FormDigest(
      PasswordForm::Scheme::kHtml, http_url.GetOrigin().spec(), http_url);

  // A new form fetcher is created to be able to set the form digest and
  // migration flag.
  form_fetcher_ = std::make_unique<FormFetcherImpl>(
      form_digest_, &client_, true /* should_migrate_http_passwords */);
  EXPECT_CALL(consumer_, OnFetchCompleted);
  form_fetcher_->AddConsumer(&consumer_);

  std::vector<PasswordForm> empty_forms;
  const PasswordForm http_form = CreateHTTPNonFederated();
  const PasswordForm federated_form = CreateFederated();

  Fetch();
  EXPECT_CALL(*mock_store_, GetLogins(_, _)).Times(0);
  EXPECT_CALL(*mock_store_, AddLogin(_)).Times(0);
  EXPECT_CALL(consumer_, OnFetchCompleted);
  store_consumer()->OnGetPasswordStoreResultsFrom(mock_store_.get(),
                                                  MakeResults(empty_forms));
  EXPECT_THAT(form_fetcher_->GetNonFederatedMatches(), IsEmpty());
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(), IsEmpty());
  EXPECT_FALSE(form_fetcher_->IsBlacklisted());

  Fetch();
  EXPECT_CALL(consumer_, OnFetchCompleted);
  store_consumer()->OnGetPasswordStoreResultsFrom(mock_store_.get(),
                                                  MakeResults({http_form}));
  EXPECT_THAT(form_fetcher_->GetNonFederatedMatches(),
              UnorderedElementsAre(Pointee(http_form)));
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(), IsEmpty());
  EXPECT_FALSE(form_fetcher_->IsBlacklisted());

  Fetch();
  EXPECT_CALL(consumer_, OnFetchCompleted);
  store_consumer()->OnGetPasswordStoreResultsFrom(
      mock_store_.get(), MakeResults({http_form, federated_form}));
  EXPECT_THAT(form_fetcher_->GetNonFederatedMatches(),
              UnorderedElementsAre(Pointee(http_form)));
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(),
              UnorderedElementsAre(Pointee(federated_form)));
  EXPECT_FALSE(form_fetcher_->IsBlacklisted());
}

// Test that ensures HTTP passwords are not migrated on non HTML forms.
TEST_P(FormFetcherImplTest, DoNotTryToMigrateHTTPPasswordsOnNonHTMLForms) {
  GURL::Replacements https_rep;
  https_rep.SetSchemeStr(url::kHttpsScheme);
  const GURL https_url = form_digest_.url.ReplaceComponents(https_rep);
  form_digest_ = PasswordStore::FormDigest(
      PasswordForm::Scheme::kBasic, https_url.GetOrigin().spec(), https_url);

  // A new form fetcher is created to be able to set the form digest and
  // migration flag.
  form_fetcher_ = std::make_unique<FormFetcherImpl>(
      form_digest_, &client_, true /* should_migrate_http_passwords */);
  EXPECT_CALL(consumer_, OnFetchCompleted);
  form_fetcher_->AddConsumer(&consumer_);

  Fetch();
  // No migration takes places upon receiving empty results from the store, and
  // hence no data are read/added from/to the store.
  EXPECT_CALL(*mock_store_, GetLogins).Times(0);
  EXPECT_CALL(*mock_store_, AddLogin).Times(0);
  EXPECT_CALL(consumer_, OnFetchCompleted);
  std::vector<PasswordForm> empty_forms;
  store_consumer()->OnGetPasswordStoreResultsFrom(mock_store_.get(),
                                                  MakeResults(empty_forms));
  EXPECT_THAT(form_fetcher_->GetNonFederatedMatches(), IsEmpty());
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(), IsEmpty());
  EXPECT_FALSE(form_fetcher_->IsBlacklisted());
}

// Test that ensures HTTP passwords are only migrated on HTTPS sites when no
// HTTPS credentials are available.
TEST_P(FormFetcherImplTest, TryToMigrateHTTPPasswordsOnHTTPSSites) {
  GURL::Replacements https_rep;
  https_rep.SetSchemeStr(url::kHttpsScheme);
  const GURL https_url = form_digest_.url.ReplaceComponents(https_rep);
  form_digest_ = PasswordStore::FormDigest(
      PasswordForm::Scheme::kHtml, https_url.GetOrigin().spec(), https_url);

  // A new form fetcher is created to be able to set the form digest and
  // migration flag.
  form_fetcher_ = std::make_unique<FormFetcherImpl>(
      form_digest_, &client_, true /* should_migrate_http_passwords */);
  EXPECT_CALL(consumer_, OnFetchCompleted);
  form_fetcher_->AddConsumer(&consumer_);

  PasswordForm https_form = CreateNonFederated();

  // Create HTTP form for the same orgin (except scheme), which will be passed
  // to the migrator.
  GURL::Replacements http_rep;
  http_rep.SetSchemeStr(url::kHttpScheme);
  PasswordForm http_form = https_form;
  http_form.url = https_form.url.ReplaceComponents(http_rep);
  http_form.signon_realm = http_form.url.GetOrigin().spec();

  std::vector<PasswordForm> empty_forms;

  // Tests that there is only an attempt to migrate credentials on HTTPS origins
  // when no other credentials are available.
  const GURL form_digest_http_url =
      form_digest_.url.ReplaceComponents(http_rep);
  PasswordStore::FormDigest http_form_digest(
      PasswordForm::Scheme::kHtml, form_digest_http_url.GetOrigin().spec(),
      form_digest_http_url);
  Fetch();
  base::WeakPtr<PasswordStoreConsumer> migrator_ptr;
  EXPECT_CALL(*mock_store_, GetLogins(http_form_digest, _))
      .WillOnce(WithArg<1>(GetAndAssignWeakPtr(&migrator_ptr)));
  store_consumer()->OnGetPasswordStoreResultsFrom(mock_store_.get(),
                                                  MakeResults(empty_forms));
  ASSERT_TRUE(migrator_ptr);

  // Now perform the actual migration.
  EXPECT_CALL(*mock_store_, AddLogin(https_form));
  EXPECT_CALL(consumer_, OnFetchCompleted);
  static_cast<HttpPasswordStoreMigrator*>(migrator_ptr.get())
      ->OnGetPasswordStoreResultsFrom(mock_store_.get(),
                                      MakeResults({http_form}));
  EXPECT_THAT(form_fetcher_->GetNonFederatedMatches(),
              UnorderedElementsAre(Pointee(https_form)));
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(), IsEmpty());
  EXPECT_FALSE(form_fetcher_->IsBlacklisted());

  // No migration should happen when results are present.
  Fetch();
  EXPECT_CALL(*mock_store_, GetLogins(_, _)).Times(0);
  EXPECT_CALL(*mock_store_, AddLogin(_)).Times(0);
  EXPECT_CALL(consumer_, OnFetchCompleted);
  store_consumer()->OnGetPasswordStoreResultsFrom(mock_store_.get(),
                                                  MakeResults({https_form}));
  EXPECT_THAT(form_fetcher_->GetNonFederatedMatches(),
              UnorderedElementsAre(Pointee(https_form)));
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(), IsEmpty());
  EXPECT_FALSE(form_fetcher_->IsBlacklisted());

  const PasswordForm federated_form = CreateFederated();
  Fetch();
  EXPECT_CALL(consumer_, OnFetchCompleted);
  store_consumer()->OnGetPasswordStoreResultsFrom(
      mock_store_.get(), MakeResults({https_form, federated_form}));
  EXPECT_THAT(form_fetcher_->GetNonFederatedMatches(),
              UnorderedElementsAre(Pointee(https_form)));
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(),
              UnorderedElementsAre(Pointee(federated_form)));
  EXPECT_FALSE(form_fetcher_->IsBlacklisted());
}

// When the FormFetcher delegates to the HttpPasswordMigrator, its state should
// be WAITING until the migrator passes the results.
TEST_P(FormFetcherImplTest, StateIsWaitingDuringMigration) {
  GURL::Replacements https_rep;
  https_rep.SetSchemeStr(url::kHttpsScheme);
  const GURL https_url = form_digest_.url.ReplaceComponents(https_rep);
  form_digest_ = PasswordStore::FormDigest(
      PasswordForm::Scheme::kHtml, https_url.GetOrigin().spec(), https_url);

  // A new form fetcher is created to be able to set the form digest and
  // migration flag.
  form_fetcher_ = std::make_unique<FormFetcherImpl>(
      form_digest_, &client_, true /* should_migrate_http_passwords */);

  PasswordForm https_form = CreateNonFederated();

  // Create HTTP form for the same orgin (except scheme), which will be passed
  // to the migrator.
  GURL::Replacements http_rep;
  http_rep.SetSchemeStr(url::kHttpScheme);
  PasswordForm http_form = https_form;
  http_form.url = https_form.url.ReplaceComponents(http_rep);
  http_form.signon_realm = http_form.url.GetOrigin().spec();

  std::vector<PasswordForm> empty_forms;

  // Ensure there is an attempt to migrate credentials on HTTPS origins and
  // extract the migrator.
  const GURL form_digest_http_url =
      form_digest_.url.ReplaceComponents(http_rep);
  PasswordStore::FormDigest http_form_digest(
      PasswordForm::Scheme::kHtml, form_digest_http_url.GetOrigin().spec(),
      form_digest_http_url);
  Fetch();
  // First the FormFetcher is waiting for the initial response from
  // PasswordStore.
  EXPECT_EQ(FormFetcher::State::WAITING, form_fetcher_->GetState());
  base::WeakPtr<PasswordStoreConsumer> migrator_ptr;
  EXPECT_CALL(*mock_store_, GetLogins(http_form_digest, _))
      .WillOnce(WithArg<1>(GetAndAssignWeakPtr(&migrator_ptr)));
  store_consumer()->OnGetPasswordStoreResultsFrom(mock_store_.get(),
                                                  MakeResults(empty_forms));
  ASSERT_TRUE(migrator_ptr);
  // While the initial results from PasswordStore arrived to the FormFetcher, it
  // should be still waiting for the migrator.
  EXPECT_EQ(FormFetcher::State::WAITING, form_fetcher_->GetState());

  // Now perform the actual migration.
  EXPECT_CALL(*mock_store_, AddLogin(https_form));
  static_cast<HttpPasswordStoreMigrator*>(migrator_ptr.get())
      ->OnGetPasswordStoreResultsFrom(mock_store_.get(),
                                      MakeResults({http_form}));
  EXPECT_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
}

// Cloning a FormFetcherImpl with empty results should result in an
// instance with empty results.
TEST_P(FormFetcherImplTest, Clone_EmptyResults) {
  Fetch();
  store_consumer()->OnGetPasswordStoreResultsFrom(
      mock_store_.get(), std::vector<std::unique_ptr<PasswordForm>>());
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(mock_store_.get()));

  // Clone() should not cause re-fetching from PasswordStore.
  EXPECT_CALL(*mock_store_, GetLogins(_, _)).Times(0);
  auto clone = form_fetcher_->Clone();
  EXPECT_EQ(FormFetcher::State::NOT_WAITING, clone->GetState());
  EXPECT_THAT(clone->GetInteractionsStats(), IsEmpty());
  EXPECT_THAT(clone->GetCompromisedCredentials(), IsEmpty());
  EXPECT_THAT(clone->GetFederatedMatches(), IsEmpty());
  MockConsumer consumer;
  EXPECT_CALL(consumer, OnFetchCompleted);
  clone->AddConsumer(&consumer);
}

// Cloning a FormFetcherImpl with non-empty results should result in an
// instance with the same results.
TEST_P(FormFetcherImplTest, Clone_NonEmptyResults) {
  Fetch();
  PasswordForm non_federated = CreateNonFederated();
  PasswordForm federated = CreateFederated();
  PasswordForm android_federated = CreateAndroidFederated();
  std::vector<std::unique_ptr<PasswordForm>> results;
  results.push_back(std::make_unique<PasswordForm>(non_federated));
  results.push_back(std::make_unique<PasswordForm>(federated));
  results.push_back(std::make_unique<PasswordForm>(android_federated));

  store_consumer()->OnGetPasswordStoreResultsFrom(mock_store_.get(),
                                                  std::move(results));
  EXPECT_THAT(form_fetcher_->GetNonFederatedMatches(),
              UnorderedElementsAre(Pointee(non_federated)));
  EXPECT_THAT(
      form_fetcher_->GetFederatedMatches(),
      UnorderedElementsAre(Pointee(federated), Pointee(android_federated)));
  EXPECT_FALSE(form_fetcher_->IsBlacklisted());

  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(mock_store_.get()));

  // Clone() should not cause re-fetching from PasswordStore.
  EXPECT_CALL(*mock_store_, GetLogins(_, _)).Times(0);
  auto clone = form_fetcher_->Clone();

  // Additionally, destroy the original FormFetcher. This should not invalidate
  // the data in |clone|.
  form_fetcher_.reset();

  EXPECT_EQ(FormFetcher::State::NOT_WAITING, clone->GetState());
  EXPECT_THAT(clone->GetInteractionsStats(), IsEmpty());
  EXPECT_THAT(clone->GetNonFederatedMatches(),
              UnorderedElementsAre(Pointee(non_federated)));
  EXPECT_THAT(
      clone->GetFederatedMatches(),
      UnorderedElementsAre(Pointee(federated), Pointee(android_federated)));
  MockConsumer consumer;
  EXPECT_CALL(consumer, OnFetchCompleted);
  clone->AddConsumer(&consumer);
}

// Cloning a FormFetcherImpl with some stats should result in an instance with
// the same stats.
TEST_P(FormFetcherImplTest, Clone_Stats) {
  Fetch();
  // Pass empty results to make the state NOT_WAITING.
  store_consumer()->OnGetPasswordStoreResultsFrom(
      mock_store_.get(), std::vector<std::unique_ptr<PasswordForm>>());
  std::vector<InteractionsStats> stats(1);
  store_consumer()->OnGetSiteStatistics(std::move(stats));

  auto clone = form_fetcher_->Clone();
  EXPECT_EQ(1u, clone->GetInteractionsStats().size());
}

TEST_P(FormFetcherImplTest, Clone_Compromised) {
  Fetch();
  // Pass empty results to make the state NOT_WAITING.
  store_consumer()->OnGetPasswordStoreResultsFrom(
      mock_store_.get(), std::vector<std::unique_ptr<PasswordForm>>());
  const std::vector<CompromisedCredentials> credentials = {
      {form_digest_.signon_realm, base::ASCIIToUTF16("username_value"),
       base::Time::FromTimeT(1), CompromiseType::kLeaked}};
  static_cast<CompromisedCredentialsConsumer*>(form_fetcher_.get())
      ->OnGetCompromisedCredentials(credentials);

  auto clone = form_fetcher_->Clone();
  EXPECT_THAT(clone->GetCompromisedCredentials(),
              UnorderedElementsAreArray(credentials));
}

// Check that removing consumers stops them from receiving store updates.
TEST_P(FormFetcherImplTest, RemoveConsumer) {
  Fetch();
  form_fetcher_->AddConsumer(&consumer_);
  form_fetcher_->RemoveConsumer(&consumer_);
  EXPECT_CALL(consumer_, OnFetchCompleted).Times(0);
  store_consumer()->OnGetPasswordStoreResultsFrom(
      mock_store_.get(), std::vector<std::unique_ptr<PasswordForm>>());
}

// Check that destroying the fetcher while notifying its consumers is handled
// gracefully.
TEST_P(FormFetcherImplTest, DestroyFetcherFromConsumer) {
  Fetch();

  // Construct an owning consumer and register it and a regular consumer.
  auto* form_fetcher = form_fetcher_.get();
  auto owning_consumer =
      std::make_unique<MockOwningConsumer>(std::move(form_fetcher_));
  form_fetcher->AddConsumer(owning_consumer.get());
  form_fetcher->AddConsumer(&consumer_);

  // Destroy the form fetcher when notifying the owning consumer. Make sure the
  // second consumer does not get notified anymore.
  EXPECT_CALL(*owning_consumer, OnFetchCompleted).WillOnce([&owning_consumer] {
    owning_consumer.reset();
  });

  EXPECT_CALL(consumer_, OnFetchCompleted).Times(0);
  static_cast<PasswordStoreConsumer*>(form_fetcher)
      ->OnGetPasswordStoreResultsFrom(
          mock_store_.get(), std::vector<std::unique_ptr<PasswordForm>>());
}

INSTANTIATE_TEST_SUITE_P(All,
                         FormFetcherImplTest,
                         testing::Values(false, true));

}  // namespace password_manager
