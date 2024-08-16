// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_fetcher_impl.h"

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/affiliation/affiliated_match_helper.h"
#include "components/password_manager/core/browser/affiliation/mock_affiliated_match_helper.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/interactions_stats.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "components/password_manager/core/browser/password_store/mock_smart_bubble_stats_store.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/password_store/password_store_util.h"
#include "components/password_manager/core/browser/stub_credentials_filter.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "services/network/test/test_network_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

using base::ASCIIToUTF16;
using signin::GaiaIdHash;
using testing::_;
using testing::IsEmpty;
using testing::Optional;
using testing::Pointee;
using testing::Property;
using testing::Return;
using testing::SaveArg;
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

constexpr const char kTestGroupedURL[] = "https://grouped.match.com/";

constexpr const char kTestAndroidFacetURI[] =
    "android://hash@com.example.android/";

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
  explicit NameFilter(std::string_view name) : name_(ASCIIToUTF16(name)) {}

  NameFilter(const NameFilter&) = delete;
  NameFilter& operator=(const NameFilter&) = delete;

  ~NameFilter() override = default;

  bool ShouldSave(const PasswordForm& form) const override {
    return form.username_value != name_;
  }

 private:
  const std::u16string name_;  // |username_value| to filter
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

  FakePasswordManagerClient(const FakePasswordManagerClient&) = delete;
  FakePasswordManagerClient& operator=(const FakePasswordManagerClient&) =
      delete;

  ~FakePasswordManagerClient() override = default;

  network::mojom::NetworkContext* GetNetworkContext() const override {
    return &network_context_;
  }

  void set_filter(std::unique_ptr<CredentialsFilter> filter) {
    filter_ = std::move(filter);
  }

  void set_profile_store(PasswordStoreInterface* store) {
    profile_store_ = store;
  }

  void set_account_store(PasswordStoreInterface* store) {
    account_store_ = store;
  }

 private:
  const CredentialsFilter* GetStoreResultFilter() const override {
    return filter_ ? filter_.get()
                   : StubPasswordManagerClient::GetStoreResultFilter();
  }

  PasswordStoreInterface* GetProfilePasswordStore() const override {
    return profile_store_;
  }
  PasswordStoreInterface* GetAccountPasswordStore() const override {
    return account_store_;
  }

  std::unique_ptr<CredentialsFilter> filter_;
  raw_ptr<PasswordStoreInterface> profile_store_ = nullptr;
  raw_ptr<PasswordStoreInterface> account_store_ = nullptr;
  mutable FakeNetworkContext network_context_;
};

PasswordForm CreateHTMLForm(const std::string& origin_url,
                            const std::string& username_value,
                            const std::string& password_value,
                            base::Time date_last_used = base::Time::Now()) {
  PasswordForm form;
  form.scheme = PasswordForm::Scheme::kHtml;
  form.url = GURL(origin_url);
  form.signon_realm = origin_url;
  form.username_value = ASCIIToUTF16(username_value);
  form.password_value = ASCIIToUTF16(password_value);
  form.date_last_used = date_last_used;
  form.match_type = PasswordForm::MatchType::kExact;
  return form;
}

PasswordForm CreateLeakedCredential(
    const PasswordForm& form,
    const InsecurityMetadata& insecurity_metadata,
    PasswordForm::Store store = PasswordForm::Store::kNotSet) {
  PasswordForm compromised = form;
  compromised.password_issues.clear();
  compromised.password_issues.insert(
      {InsecureType::kLeaked, insecurity_metadata});
  compromised.in_store = store;
  compromised.match_type = PasswordForm::MatchType::kExact;
  return compromised;
}

// Creates a dummy non-federated form with some basic arbitrary values.
PasswordForm CreateNonFederated(const std::string& username_value = "user",
                                base::Time date_last_used = base::Time::Now()) {
  PasswordForm form =
      CreateHTMLForm(kTestHttpsURL, username_value, "password", date_last_used);
  form.action = GURL(kTestHttpsActionURL);
  form.match_type = PasswordForm::MatchType::kExact;
  return form;
}

// Creates a dummy non-federated HTTP form with some basic arbitrary values.
PasswordForm CreateHTTPNonFederated() {
  PasswordForm form = CreateHTMLForm(kTestHttpURL, "user", "password");
  form.action = GURL(kTestHttpActionURL);
  form.match_type = PasswordForm::MatchType::kExact;
  return form;
}

// Creates a dummy federated form with some basic arbitrary values.
PasswordForm CreateFederated(const std::string& username_value = "user",
                             base::Time date_last_used = base::Time::Now()) {
  PasswordForm form = CreateNonFederated(username_value, date_last_used);
  form.signon_realm = kTestFederatedRealm;
  form.password_value.clear();
  form.federation_origin = url::SchemeHostPort(GURL(kTestFederationURL));
  form.match_type = PasswordForm::MatchType::kExact;
  return form;
}

// Creates an Android federated credential.
PasswordForm CreateAndroidFederated(
    const std::string& username_value = "user",
    base::Time date_last_used = base::Time::Now()) {
  PasswordForm form =
      CreateHTMLForm("android://hash@com.example.android/", username_value,
                     /*password_value=*/"", date_last_used);
  form.federation_origin = url::SchemeHostPort(GURL(kTestFederationURL));
  form.match_type = PasswordForm::MatchType::kAffiliated;
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
  form.match_type = PasswordForm::MatchType::kPSL;
  return form;
}

PasswordForm CreateGrouped() {
  PasswordForm form = CreateHTMLForm(kTestGroupedURL, "user", "password");
  form.match_type = PasswordForm::MatchType::kGrouped;
  return form;
}

PasswordForm CreateGroupedApp() {
  PasswordForm form = CreateHTMLForm(kTestAndroidFacetURI, "user", "password");
  form.match_type = PasswordForm::MatchType::kGrouped;
  return form;
}

// Accepts a WeakPtr<T> and succeeds WeakPtr<T>::get() matches |m|.
// Analogous to testing::Address.
template <typename T, typename Matcher>
auto WeakAddress(Matcher m) {
  return Property(&base::WeakPtr<T>::get, m);
}

}  // namespace

class FormFetcherImplTestBase : public testing::Test {
 public:
  explicit FormFetcherImplTestBase(bool create_profile_store,
                                   bool create_account_store)
      : form_digest_(PasswordForm::Scheme::kHtml,
                     kTestHttpURL,
                     GURL(kTestHttpURL)) {
    if (create_profile_store) {
      profile_mock_store_ = new testing::NiceMock<MockPasswordStoreInterface>;
      client_.set_profile_store(profile_mock_store_.get());
    } else {
      client_.set_profile_store(nullptr);
    }

    if (create_account_store) {
      account_mock_store_ = new testing::NiceMock<MockPasswordStoreInterface>;
      client_.set_account_store(account_mock_store_.get());
    }

    form_fetcher_ = std::make_unique<FormFetcherImpl>(
        form_digest_, &client_, false /* should_migrate_http_passwords */);
  }

  void SetUp() override {
    if (profile_mock_store_) {
      ON_CALL(*profile_mock_store_, GetSmartBubbleStatsStore)
          .WillByDefault(Return(&mock_smart_bubble_stats_store_));
    }
  }

  FormFetcherImplTestBase(const FormFetcherImplTestBase&) = delete;
  FormFetcherImplTestBase& operator=(const FormFetcherImplTestBase&) = delete;

  ~FormFetcherImplTestBase() override = default;

 protected:
  // A wrapper around form_fetcher_.Fetch(), adding the call expectations.
  void Fetch() {
    if (profile_mock_store_) {
      EXPECT_CALL(*profile_mock_store_,
                  GetLogins(form_digest_, WeakAddress<PasswordStoreConsumer>(
                                              form_fetcher_.get())));
    }
    if (account_mock_store_) {
      EXPECT_CALL(*account_mock_store_,
                  GetLogins(form_digest_, WeakAddress<PasswordStoreConsumer>(
                                              form_fetcher_.get())));
    }
    form_fetcher_->Fetch();
    task_environment_.RunUntilIdle();
    testing::Mock::VerifyAndClearExpectations(profile_mock_store_.get());
    if (account_mock_store_) {
      testing::Mock::VerifyAndClearExpectations(account_mock_store_.get());
    }
  }

  void DeliverPasswordStoreResults(LoginsResultOrError profile_store_results,
                                   LoginsResultOrError account_store_results) {
    store_consumer()->OnGetPasswordStoreResultsOrErrorFrom(
        profile_mock_store_.get(), std::move(profile_store_results));
    if (account_mock_store_) {
      store_consumer()->OnGetPasswordStoreResultsOrErrorFrom(
          account_mock_store_.get(), std::move(account_store_results));
    }
  }

  FakePasswordManagerClient* client() { return &client_; }

  PasswordStoreConsumer* store_consumer() { return form_fetcher_.get(); }

  base::test::TaskEnvironment task_environment_;
  PasswordFormDigest form_digest_;
  scoped_refptr<MockPasswordStoreInterface> profile_mock_store_;
  scoped_refptr<MockPasswordStoreInterface> account_mock_store_;
  std::unique_ptr<FormFetcherImpl> form_fetcher_;
  MockConsumer consumer_;
  testing::NiceMock<MockSmartBubbleStatsStore> mock_smart_bubble_stats_store_;
  FakePasswordManagerClient client_;
};

// The boolean test parameter maps to the `create_account_store` constructor
// parameter of the base class.
class FormFetcherImplTest : public FormFetcherImplTestBase,
                            public testing::WithParamInterface<bool> {
 public:
  FormFetcherImplTest()
      : FormFetcherImplTestBase(/*create_profile_store=*/true,
                                /*create_account_store=*/GetParam()) {}
};

// Check that the absence of PasswordStore results is handled correctly.
TEST_P(FormFetcherImplTest, NoStoreResults) {
  Fetch();
  EXPECT_CALL(consumer_, OnFetchCompleted).Times(0);
  form_fetcher_->AddConsumer(&consumer_);
  EXPECT_EQ(FormFetcher::State::WAITING, form_fetcher_->GetState());
  EXPECT_THAT(form_fetcher_->GetNonFederatedMatches(), IsEmpty());
  EXPECT_THAT(form_fetcher_->GetAllRelevantMatches(), IsEmpty());
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(), IsEmpty());
  EXPECT_FALSE(form_fetcher_->IsBlocklisted());
}

// Check that empty PasswordStore results are handled correctly.
TEST_P(FormFetcherImplTest, Empty) {
  Fetch();
  form_fetcher_->AddConsumer(&consumer_);
  EXPECT_CALL(consumer_, OnFetchCompleted);
  DeliverPasswordStoreResults(/*profile_store_results=*/{},
                              /*account_store_results=*/{});
  EXPECT_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
  EXPECT_THAT(form_fetcher_->GetNonFederatedMatches(), IsEmpty());
  EXPECT_THAT(form_fetcher_->GetAllRelevantMatches(), IsEmpty());
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(), IsEmpty());
  EXPECT_FALSE(form_fetcher_->IsBlocklisted());
}

// Check that non-federated PasswordStore results are handled correctly.
TEST_P(FormFetcherImplTest, NonFederated) {
  Fetch();
  PasswordForm non_federated = CreateNonFederated();
  form_fetcher_->AddConsumer(&consumer_);
  std::vector<PasswordForm> results = {non_federated};
  EXPECT_CALL(consumer_, OnFetchCompleted);
  DeliverPasswordStoreResults(/*profile_store_results=*/std::move(results),
                              /*account_store_results=*/{});
  EXPECT_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
  EXPECT_THAT(form_fetcher_->GetNonFederatedMatches(),
              UnorderedElementsAre(non_federated));
  EXPECT_THAT(form_fetcher_->GetAllRelevantMatches(),
              UnorderedElementsAre(non_federated));
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(), IsEmpty());
  EXPECT_FALSE(form_fetcher_->IsBlocklisted());
}

// Check that PasswordStore results not having non-federated same schema matches
// are handled correctly.
TEST_P(FormFetcherImplTest, NonFederatedOtherSchemasOnly) {
  Fetch();
  PasswordForm non_federated = CreateNonFederated();
  non_federated.scheme = PasswordForm::Scheme::kOther;
  form_fetcher_->AddConsumer(&consumer_);
  std::vector<PasswordForm> results = {non_federated};
  EXPECT_CALL(consumer_, OnFetchCompleted);
  DeliverPasswordStoreResults(/*profile_store_results=*/std::move(results),
                              /*account_store_results=*/{});
  EXPECT_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
  EXPECT_THAT(form_fetcher_->GetNonFederatedMatches(),
              UnorderedElementsAre(non_federated));
  EXPECT_THAT(form_fetcher_->GetAllRelevantMatches(), IsEmpty());
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(), IsEmpty());
  EXPECT_FALSE(form_fetcher_->IsBlocklisted());
}

// Check that federated PasswordStore results are handled correctly.
TEST_P(FormFetcherImplTest, Federated) {
  Fetch();
  PasswordForm federated = CreateFederated();
  PasswordForm android_federated = CreateAndroidFederated();
  form_fetcher_->AddConsumer(&consumer_);
  std::vector<PasswordForm> results = {federated, android_federated};
  EXPECT_CALL(consumer_, OnFetchCompleted);
  DeliverPasswordStoreResults(/*profile_store_results=*/std::move(results),
                              /*account_store_results=*/{});
  EXPECT_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
  EXPECT_THAT(form_fetcher_->GetNonFederatedMatches(), IsEmpty());
  EXPECT_THAT(form_fetcher_->GetAllRelevantMatches(), IsEmpty());
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(),
              UnorderedElementsAre(federated, android_federated));
  EXPECT_FALSE(form_fetcher_->IsBlocklisted());
}

// Check that blocked PasswordStore results are handled correctly.
TEST_P(FormFetcherImplTest, Blocked) {
  Fetch();
  PasswordForm blocked = CreateBlocked();
  form_fetcher_->AddConsumer(&consumer_);
  std::vector<PasswordForm> results = {blocked};
  EXPECT_CALL(consumer_, OnFetchCompleted);
  DeliverPasswordStoreResults(/*profile_store_results=*/std::move(results),
                              /*account_store_results=*/{});
  EXPECT_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
  EXPECT_THAT(form_fetcher_->GetNonFederatedMatches(), IsEmpty());
  EXPECT_THAT(form_fetcher_->GetAllRelevantMatches(), IsEmpty());
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(), IsEmpty());
  EXPECT_TRUE(form_fetcher_->IsBlocklisted());
}

// Blocked PSL matches in the store should be ignored.
TEST_P(FormFetcherImplTest, BlockedPSL) {
  Fetch();
  form_fetcher_->AddConsumer(&consumer_);
  std::vector<PasswordForm> results = {CreateBlockedPsl()};
  EXPECT_CALL(consumer_, OnFetchCompleted);
  DeliverPasswordStoreResults(/*profile_store_results=*/std::move(results),
                              /*account_store_results=*/{});
  EXPECT_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
  EXPECT_FALSE(form_fetcher_->IsBlocklisted());
}

// Blocked matches with a different scheme in the store should be ignored.
TEST_P(FormFetcherImplTest, BlockedDifferentScheme) {
  Fetch();
  form_fetcher_->AddConsumer(&consumer_);
  PasswordForm blocked_http_auth = CreateBlocked();
  blocked_http_auth.scheme = PasswordForm::Scheme::kBasic;
  std::vector<PasswordForm> results = {blocked_http_auth};
  EXPECT_CALL(consumer_, OnFetchCompleted);
  DeliverPasswordStoreResults(/*profile_store_results=*/std::move(results),
                              /*account_store_results=*/{});
  EXPECT_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
  EXPECT_FALSE(form_fetcher_->IsBlocklisted());
}

// Grouped credentials should be filtered out unless `FormFetcher` is configured
// explicitly to include them.
TEST_P(FormFetcherImplTest, FiltersGroupedCredentials) {
  EXPECT_FALSE(form_fetcher_->GetPreferredOrPotentialMatchedFormType());
  Fetch();
  form_fetcher_->AddConsumer(&consumer_);
  PasswordForm non_federated = CreateNonFederated();
  std::vector<PasswordForm> results = {non_federated, CreateGrouped()};
  EXPECT_CALL(consumer_, OnFetchCompleted);
  DeliverPasswordStoreResults(/*profile_store_results=*/std::move(results),
                              /*account_store_results=*/{});
  EXPECT_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
  EXPECT_THAT(form_fetcher_->GetNonFederatedMatches(),
              UnorderedElementsAre(non_federated));
  EXPECT_THAT(form_fetcher_->GetAllRelevantMatches(),
              UnorderedElementsAre(non_federated));
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(), IsEmpty());
  EXPECT_FALSE(form_fetcher_->IsBlocklisted());
  EXPECT_THAT(
      form_fetcher_->GetPreferredOrPotentialMatchedFormType(),
      Optional(PasswordFormMetricsRecorder::MatchedFormType::kExactMatch));
}

// Grouped credentials should be returned if `FormFetcher` is configured to do
// keep them in the result set.
TEST_P(FormFetcherImplTest, ReturnsGroupedCredentialsIfConfigured) {
  form_fetcher_->set_filter_grouped_credentials(false);
  EXPECT_FALSE(form_fetcher_->GetPreferredOrPotentialMatchedFormType());
  Fetch();
  form_fetcher_->AddConsumer(&consumer_);
  PasswordForm grouped_app = CreateGroupedApp();
  std::vector<PasswordForm> results = {grouped_app};
  EXPECT_CALL(consumer_, OnFetchCompleted);
  DeliverPasswordStoreResults(/*profile_store_results=*/std::move(results),
                              /*account_store_results=*/{});
  EXPECT_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
  EXPECT_THAT(form_fetcher_->GetNonFederatedMatches(),
              UnorderedElementsAre(grouped_app));
  EXPECT_THAT(form_fetcher_->GetAllRelevantMatches(),
              UnorderedElementsAre(grouped_app));
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(), IsEmpty());
  EXPECT_FALSE(form_fetcher_->IsBlocklisted());
  EXPECT_THAT(
      form_fetcher_->GetPreferredOrPotentialMatchedFormType(),
      Optional(PasswordFormMetricsRecorder::MatchedFormType::kGroupedApp));
}

// Grouped credentials should be returned if `FormFetcher` is configured to do
// keep them in the result set.
TEST_P(FormFetcherImplTest, ReturnsMultipleGroupedCredentialsIfConfigured) {
  form_fetcher_->set_filter_grouped_credentials(false);
  EXPECT_FALSE(form_fetcher_->GetPreferredOrPotentialMatchedFormType());
  Fetch();
  form_fetcher_->AddConsumer(&consumer_);
  PasswordForm grouped = CreateGrouped();
  PasswordForm grouped_app = CreateGroupedApp();
  std::vector<PasswordForm> results = {grouped_app, grouped};
  EXPECT_CALL(consumer_, OnFetchCompleted);
  DeliverPasswordStoreResults(/*profile_store_results=*/std::move(results),
                              /*account_store_results=*/{});
  EXPECT_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
  EXPECT_THAT(form_fetcher_->GetNonFederatedMatches(),
              UnorderedElementsAre(grouped, grouped_app));
  EXPECT_THAT(form_fetcher_->GetAllRelevantMatches(),
              UnorderedElementsAre(grouped, grouped_app));
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(), IsEmpty());
  EXPECT_FALSE(form_fetcher_->IsBlocklisted());
  EXPECT_THAT(
      form_fetcher_->GetPreferredOrPotentialMatchedFormType(),
      Optional(PasswordFormMetricsRecorder::MatchedFormType::kGroupedApp));
}

// Check that grouped website credentials are prioritized over grouped app
// credentials if the `FormFetcher` is configured to ignore grouped credentials.
TEST_P(
    FormFetcherImplTest,
    PrioritisesGroupedWebsiteOverGroupedAppWhenGroupedCredenetialsAreFiltered) {
  EXPECT_FALSE(form_fetcher_->GetPreferredOrPotentialMatchedFormType());
  Fetch();
  form_fetcher_->AddConsumer(&consumer_);
  PasswordForm grouped = CreateGrouped();
  PasswordForm grouped_app = CreateGroupedApp();
  std::vector<PasswordForm> results = {grouped_app, grouped};
  EXPECT_CALL(consumer_, OnFetchCompleted);
  DeliverPasswordStoreResults(/*profile_store_results=*/std::move(results),
                              /*account_store_results=*/{});
  EXPECT_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
  EXPECT_THAT(form_fetcher_->GetNonFederatedMatches(), IsEmpty());
  EXPECT_THAT(form_fetcher_->GetAllRelevantMatches(), IsEmpty());
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(), IsEmpty());
  EXPECT_FALSE(form_fetcher_->IsBlocklisted());
  EXPECT_THAT(
      form_fetcher_->GetPreferredOrPotentialMatchedFormType(),
      Optional(PasswordFormMetricsRecorder::MatchedFormType::kGroupedWebsites));
}

// Check that mixed PasswordStore results are handled correctly.
TEST_P(FormFetcherImplTest, Mixed) {
  Fetch();
  PasswordForm federated1 = CreateFederated();
  federated1.username_value = u"user";
  PasswordForm federated2 = CreateFederated();
  federated2.username_value = u"user_B";
  PasswordForm federated3 = CreateAndroidFederated();
  federated3.username_value = u"user_B";
  PasswordForm non_federated1 = CreateNonFederated();
  non_federated1.username_value = u"user";
  PasswordForm non_federated2 = CreateNonFederated();
  non_federated2.username_value = u"user_C";
  PasswordForm non_federated3 = CreateNonFederated();
  non_federated3.username_value = u"user_D";
  non_federated3.scheme = PasswordForm::Scheme::kOther;
  PasswordForm blocked = CreateBlocked();

  form_fetcher_->AddConsumer(&consumer_);
  std::vector<PasswordForm> results = {
      federated1,     federated2,     federated3, non_federated1,
      non_federated2, non_federated3, blocked};
  EXPECT_CALL(consumer_, OnFetchCompleted);
  DeliverPasswordStoreResults(/*profile_store_results=*/std::move(results),
                              /*account_store_results=*/{});
  EXPECT_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
  EXPECT_THAT(
      form_fetcher_->GetNonFederatedMatches(),
      UnorderedElementsAre(non_federated1, non_federated2, non_federated3));
  EXPECT_THAT(form_fetcher_->GetAllRelevantMatches(),
              UnorderedElementsAre(non_federated1, non_federated2));
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(),
              UnorderedElementsAre(federated1, federated2, federated3));
  EXPECT_TRUE(form_fetcher_->IsBlocklisted());
}

// Check that PasswordStore results are filtered correctly.
TEST_P(FormFetcherImplTest, Filtered) {
  Fetch();
  PasswordForm federated = CreateFederated();
  federated.username_value = u"user";
  PasswordForm non_federated1 = CreateNonFederated();
  non_federated1.username_value = u"user";
  PasswordForm non_federated2 = CreateNonFederated();
  non_federated2.username_value = u"user_C";

  // Set up a filter to remove all credentials with the username "user".
  client_.set_filter(std::make_unique<NameFilter>("user"));

  form_fetcher_->AddConsumer(&consumer_);
  std::vector<PasswordForm> results = {federated, non_federated1,
                                       non_federated2};
  EXPECT_CALL(consumer_, OnFetchCompleted);
  DeliverPasswordStoreResults(/*profile_store_results=*/std::move(results),
                              /*account_store_results=*/{});
  EXPECT_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
  // Expect that nothing got filtered out, since CredentialsFilter no longer
  // filters things out:
  EXPECT_THAT(form_fetcher_->GetNonFederatedMatches(),
              UnorderedElementsAre(non_federated1, non_federated2));
  EXPECT_THAT(form_fetcher_->GetAllRelevantMatches(),
              UnorderedElementsAre(non_federated1, non_federated2));
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(),
              UnorderedElementsAre(federated));
}

// Check that stats from PasswordStore are handled correctly.
TEST_P(FormFetcherImplTest, Stats) {
  Fetch();
  form_fetcher_->AddConsumer(&consumer_);
  std::vector<InteractionsStats> stats(1);
  store_consumer()->OnGetSiteStatistics(std::move(stats));
  EXPECT_EQ(1u, form_fetcher_->GetInteractionsStats().size());
}

TEST_P(FormFetcherImplTest, InsecureCredentials) {
  Fetch();
  form_fetcher_->AddConsumer(&consumer_);
  PasswordForm form = CreateNonFederated();
  InsecurityMetadata leaked_metadata{base::Time(), IsMuted(false),
                                     TriggerBackendNotification(true)};
  form.password_issues.insert({InsecureType::kLeaked, leaked_metadata});
  std::vector<PasswordForm> results = {form};
  DeliverPasswordStoreResults(/*profile_store_results=*/std::move(results),
                              /*account_store_results=*/{});
  EXPECT_THAT(
      form_fetcher_->GetInsecureCredentials(),
      UnorderedElementsAre(CreateLeakedCredential(form, leaked_metadata)));
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
  form_a.username_value = u"a@gmail.com";
  std::vector<PasswordForm> old_results;
  old_results.push_back(form_a);
  // Because of the pending updates, the old PasswordStore results are not
  // forwarded to the consumers.
  EXPECT_CALL(consumer_, OnFetchCompleted).Times(0);
  // Delivering the first results will trigger the new GetLogins call, because
  // of the Fetch() above.
  EXPECT_CALL(*profile_mock_store_,
              GetLogins(form_digest_, WeakAddress<PasswordStoreConsumer>(
                                          form_fetcher_.get())));
  DeliverPasswordStoreResults(/*profile_store_results=*/std::move(old_results),
                              /*account_store_results=*/{});

  // Second response from the store should not be ignored.
  PasswordForm form_b = CreateNonFederated();
  form_b.username_value = u"b@gmail.com";

  PasswordForm form_c = CreateNonFederated();
  form_c.username_value = u"c@gmail.com";

  EXPECT_CALL(consumer_, OnFetchCompleted);
  std::vector<PasswordForm> results = {form_b, form_c};
  DeliverPasswordStoreResults(/*profile_store_results=*/std::move(results),
                              /*account_store_results=*/{});
  EXPECT_THAT(form_fetcher_->GetNonFederatedMatches(),
              UnorderedElementsAre(form_b, form_c));
}

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
TEST_P(FormFetcherImplTest, FetchStatistics) {
  InteractionsStats stats;
  stats.origin_domain = form_digest_.url.DeprecatedGetOriginAsURL();
  stats.username_value = u"some username";
  stats.dismissal_count = 5;
  std::vector<InteractionsStats> db_stats = {stats};
  EXPECT_CALL(*profile_mock_store_,
              GetLogins(form_digest_, WeakAddress<PasswordStoreConsumer>(
                                          form_fetcher_.get())));
  EXPECT_CALL(mock_smart_bubble_stats_store_,
              GetSiteStats(stats.origin_domain, _))
      .WillOnce(testing::WithArg<1>(
          [db_stats](base::WeakPtr<PasswordStoreConsumer> consumer) {
            base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, base::BindOnce(
                               [](base::WeakPtr<PasswordStoreConsumer> con,
                                  const std::vector<InteractionsStats>& stats) {
                                 con->OnGetSiteStatistics(
                                     std::vector<InteractionsStats>(stats));
                               },
                               consumer, db_stats));
          }));
  form_fetcher_->Fetch();
  task_environment_.RunUntilIdle();

  EXPECT_THAT(form_fetcher_->GetInteractionsStats(),
              UnorderedElementsAre(stats));
}
#else
TEST_P(FormFetcherImplTest, DontFetchStatistics) {
  EXPECT_CALL(*profile_mock_store_,
              GetLogins(form_digest_, WeakAddress<PasswordStoreConsumer>(
                                          form_fetcher_.get())));
  EXPECT_CALL(mock_smart_bubble_stats_store_, GetSiteStats).Times(0);
  form_fetcher_->Fetch();
  task_environment_.RunUntilIdle();
}
#endif

// Test that ensures HTTP passwords are not migrated on HTTP sites.
TEST_P(FormFetcherImplTest, DoNotTryToMigrateHTTPPasswordsOnHTTPSites) {
  GURL::Replacements http_rep;
  http_rep.SetSchemeStr(url::kHttpScheme);
  const GURL http_url = form_digest_.url.ReplaceComponents(http_rep);
  form_digest_ =
      PasswordFormDigest(PasswordForm::Scheme::kHtml,
                         http_url.DeprecatedGetOriginAsURL().spec(), http_url);

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
  EXPECT_CALL(*profile_mock_store_, GetLogins(_, _)).Times(0);
  EXPECT_CALL(*profile_mock_store_, AddLogin).Times(0);
  EXPECT_CALL(consumer_, OnFetchCompleted);
  DeliverPasswordStoreResults(
      /*profile_store_results=*/std::vector<PasswordForm>(empty_forms),
      /*account_store_results=*/{});
  EXPECT_THAT(form_fetcher_->GetNonFederatedMatches(), IsEmpty());
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(), IsEmpty());
  EXPECT_FALSE(form_fetcher_->IsBlocklisted());

  Fetch();
  EXPECT_CALL(consumer_, OnFetchCompleted);
  DeliverPasswordStoreResults(
      /*profile_store_results=*/std::vector<PasswordForm>({http_form}),
      /*account_store_results=*/{});
  EXPECT_THAT(form_fetcher_->GetNonFederatedMatches(),
              UnorderedElementsAre(http_form));
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(), IsEmpty());
  EXPECT_FALSE(form_fetcher_->IsBlocklisted());

  Fetch();
  EXPECT_CALL(consumer_, OnFetchCompleted);
  DeliverPasswordStoreResults(
      /*profile_store_results=*/std::vector<PasswordForm>(
          {http_form, federated_form}),
      /*account_store_results=*/{});
  EXPECT_THAT(form_fetcher_->GetNonFederatedMatches(),
              UnorderedElementsAre(http_form));
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(),
              UnorderedElementsAre(federated_form));
  EXPECT_FALSE(form_fetcher_->IsBlocklisted());
}

// Test that ensures HTTP passwords are not migrated on non HTML forms.
TEST_P(FormFetcherImplTest, DoNotTryToMigrateHTTPPasswordsOnNonHTMLForms) {
  GURL::Replacements https_rep;
  https_rep.SetSchemeStr(url::kHttpsScheme);
  const GURL https_url = form_digest_.url.ReplaceComponents(https_rep);
  form_digest_ = PasswordFormDigest(PasswordForm::Scheme::kBasic,
                                    https_url.DeprecatedGetOriginAsURL().spec(),
                                    https_url);

  // A new form fetcher is created to be able to set the form digest and
  // migration flag.
  form_fetcher_ = std::make_unique<FormFetcherImpl>(
      form_digest_, &client_, true /* should_migrate_http_passwords */);
  EXPECT_CALL(consumer_, OnFetchCompleted);
  form_fetcher_->AddConsumer(&consumer_);

  Fetch();
  // No migration takes places upon receiving empty results from the store, and
  // hence no data are read/added from/to the store.
  EXPECT_CALL(*profile_mock_store_, GetLogins).Times(0);
  EXPECT_CALL(*profile_mock_store_, AddLogin).Times(0);
  EXPECT_CALL(consumer_, OnFetchCompleted);
  std::vector<PasswordForm> empty_forms;
  DeliverPasswordStoreResults(
      /*profile_store_results=*/std::vector<PasswordForm>(empty_forms),
      /*account_store_results=*/{});
  EXPECT_THAT(form_fetcher_->GetNonFederatedMatches(), IsEmpty());
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(), IsEmpty());
  EXPECT_FALSE(form_fetcher_->IsBlocklisted());
}

// Test that ensures HTTP passwords are only migrated on HTTPS sites when no
// HTTPS credentials are available.
TEST_P(FormFetcherImplTest, TryToMigrateHTTPPasswordsOnHTTPSSites) {
  GURL::Replacements https_rep;
  https_rep.SetSchemeStr(url::kHttpsScheme);
  const GURL https_url = form_digest_.url.ReplaceComponents(https_rep);
  form_digest_ = PasswordFormDigest(PasswordForm::Scheme::kHtml,
                                    https_url.DeprecatedGetOriginAsURL().spec(),
                                    https_url);

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
  http_form.signon_realm = http_form.url.DeprecatedGetOriginAsURL().spec();

  // Tests that there is only an attempt to migrate credentials on HTTPS origins
  // when no other credentials are available.
  const GURL form_digest_http_url =
      form_digest_.url.ReplaceComponents(http_rep);
  PasswordFormDigest http_form_digest(
      PasswordForm::Scheme::kHtml,
      form_digest_http_url.DeprecatedGetOriginAsURL().spec(),
      form_digest_http_url);
  Fetch();
  base::WeakPtr<PasswordStoreConsumer> profile_store_migrator;
  base::WeakPtr<PasswordStoreConsumer> account_store_migrator;
  EXPECT_CALL(*profile_mock_store_, GetLogins(http_form_digest, _))
      .WillOnce(SaveArg<1>(&profile_store_migrator));
  if (account_mock_store_) {
    EXPECT_CALL(*account_mock_store_, GetLogins(http_form_digest, _))
        .WillOnce(SaveArg<1>(&account_store_migrator));
  }
  DeliverPasswordStoreResults(/*profile_store_results=*/{},
                              /*account_store_results=*/{});
  ASSERT_TRUE(profile_store_migrator);
  if (account_mock_store_) {
    ASSERT_TRUE(account_store_migrator);
  }
  // Now perform the actual migration.
  EXPECT_CALL(*profile_mock_store_, AddLogin(https_form, _));
  EXPECT_CALL(consumer_, OnFetchCompleted);
  profile_store_migrator->OnGetPasswordStoreResultsOrErrorFrom(
      profile_mock_store_.get(), std::vector<PasswordForm>({http_form}));
  if (account_mock_store_) {
    account_store_migrator->OnGetPasswordStoreResultsOrErrorFrom(
        account_mock_store_.get(), {});
  }
  EXPECT_THAT(form_fetcher_->GetNonFederatedMatches(),
              UnorderedElementsAre(https_form));
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(), IsEmpty());
  EXPECT_FALSE(form_fetcher_->IsBlocklisted());

  // No migration should happen when results are present.
  Fetch();
  EXPECT_CALL(*profile_mock_store_, GetLogins(_, _)).Times(0);
  EXPECT_CALL(*profile_mock_store_, AddLogin).Times(0);
  EXPECT_CALL(consumer_, OnFetchCompleted);
  DeliverPasswordStoreResults(
      /*profile_store_results=*/std::vector<PasswordForm>({https_form}),
      /*account_store_results=*/std::vector<PasswordForm>({https_form}));
  if (account_mock_store_) {
    EXPECT_THAT(form_fetcher_->GetNonFederatedMatches(),
                UnorderedElementsAre(https_form, https_form));
  } else {
    EXPECT_THAT(form_fetcher_->GetNonFederatedMatches(),
                UnorderedElementsAre(https_form));
  }
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(), IsEmpty());
  EXPECT_FALSE(form_fetcher_->IsBlocklisted());

  const PasswordForm federated_form = CreateFederated();
  Fetch();
  EXPECT_CALL(consumer_, OnFetchCompleted);
  DeliverPasswordStoreResults(
      /*profile_store_results=*/std::vector<PasswordForm>(
          {https_form, federated_form}),
      /*account_store_results=*/std::vector<PasswordForm>(
          {https_form, federated_form}));
  if (account_mock_store_) {
    EXPECT_THAT(form_fetcher_->GetNonFederatedMatches(),
                UnorderedElementsAre(https_form, https_form));
    EXPECT_THAT(form_fetcher_->GetFederatedMatches(),
                UnorderedElementsAre(federated_form, federated_form));
  } else {
    EXPECT_THAT(form_fetcher_->GetNonFederatedMatches(),
                UnorderedElementsAre(https_form));
    EXPECT_THAT(form_fetcher_->GetFederatedMatches(),
                UnorderedElementsAre(federated_form));
  }
  EXPECT_FALSE(form_fetcher_->IsBlocklisted());
}

// When the FormFetcher delegates to the HttpPasswordMigrator, its state should
// be WAITING until the migrator passes the results.
TEST_P(FormFetcherImplTest, StateIsWaitingDuringMigration) {
  GURL::Replacements https_rep;
  https_rep.SetSchemeStr(url::kHttpsScheme);
  const GURL https_url = form_digest_.url.ReplaceComponents(https_rep);
  form_digest_ = PasswordFormDigest(PasswordForm::Scheme::kHtml,
                                    https_url.DeprecatedGetOriginAsURL().spec(),
                                    https_url);

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
  http_form.signon_realm = http_form.url.DeprecatedGetOriginAsURL().spec();

  // Ensure there is an attempt to migrate credentials on HTTPS origins and
  // extract the migrator.
  const GURL form_digest_http_url =
      form_digest_.url.ReplaceComponents(http_rep);
  PasswordFormDigest http_form_digest(
      PasswordForm::Scheme::kHtml,
      form_digest_http_url.DeprecatedGetOriginAsURL().spec(),
      form_digest_http_url);
  Fetch();
  // First the FormFetcher is waiting for the initial response from the
  // PasswordStore(s).
  EXPECT_EQ(FormFetcher::State::WAITING, form_fetcher_->GetState());
  base::WeakPtr<PasswordStoreConsumer> profile_store_migrator;
  base::WeakPtr<PasswordStoreConsumer> account_store_migrator;
  EXPECT_CALL(*profile_mock_store_, GetLogins(http_form_digest, _))
      .WillOnce(SaveArg<1>(&profile_store_migrator));
  if (account_mock_store_) {
    EXPECT_CALL(*account_mock_store_, GetLogins(http_form_digest, _))
        .WillOnce(SaveArg<1>(&account_store_migrator));
  }
  DeliverPasswordStoreResults(/*profile_store_results=*/{},
                              /*account_store_results=*/{});
  ASSERT_TRUE(profile_store_migrator);
  if (account_mock_store_) {
    ASSERT_TRUE(account_store_migrator);
  }
  // While the initial results from PasswordStore arrived to the FormFetcher, it
  // should be still waiting for the migrator(s).
  EXPECT_EQ(FormFetcher::State::WAITING, form_fetcher_->GetState());

  // Now perform the actual migration.
  EXPECT_CALL(*profile_mock_store_, AddLogin(https_form, _));
  profile_store_migrator->OnGetPasswordStoreResultsOrErrorFrom(
      profile_mock_store_.get(), std::vector<PasswordForm>({http_form}));
  if (account_mock_store_) {
    account_store_migrator->OnGetPasswordStoreResultsOrErrorFrom(
        account_mock_store_.get(), {});
  }
  EXPECT_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
}

// Cloning a FormFetcherImpl with empty results should result in an
// instance with empty results.
TEST_P(FormFetcherImplTest, Clone_EmptyResults) {
  Fetch();
  DeliverPasswordStoreResults(/*profile_store_results=*/{},
                              /*account_store_results=*/{});
  ASSERT_TRUE(
      ::testing::Mock::VerifyAndClearExpectations(profile_mock_store_.get()));

  // Clone() should not cause re-fetching from PasswordStore.
  EXPECT_CALL(*profile_mock_store_, GetLogins(_, _)).Times(0);
  auto clone = form_fetcher_->Clone();
  EXPECT_EQ(FormFetcher::State::NOT_WAITING, clone->GetState());
  EXPECT_THAT(clone->GetInteractionsStats(), IsEmpty());
  EXPECT_THAT(clone->GetInsecureCredentials(), IsEmpty());
  EXPECT_THAT(clone->GetFederatedMatches(), IsEmpty());
  EXPECT_THAT(form_fetcher_->GetNonFederatedMatches(), IsEmpty());
  EXPECT_THAT(form_fetcher_->GetAllRelevantMatches(), IsEmpty());
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
  InsecurityMetadata leaked_metadata{base::Time(), IsMuted(false),
                                     TriggerBackendNotification(true)};
  federated.password_issues.insert({InsecureType::kLeaked, leaked_metadata});
  PasswordForm android_federated = CreateAndroidFederated();
  std::vector<PasswordForm> results = {non_federated, federated,
                                       android_federated};

  DeliverPasswordStoreResults(/*profile_store_results=*/std::move(results),
                              /*account_store_results=*/{});
  EXPECT_THAT(form_fetcher_->GetNonFederatedMatches(),
              UnorderedElementsAre(non_federated));
  EXPECT_THAT(form_fetcher_->GetAllRelevantMatches(),
              UnorderedElementsAre(non_federated));
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(),
              UnorderedElementsAre(federated, android_federated));
  EXPECT_THAT(form_fetcher_->GetInsecureCredentials(),
              UnorderedElementsAre(federated));
  EXPECT_FALSE(form_fetcher_->IsBlocklisted());

  ASSERT_TRUE(
      ::testing::Mock::VerifyAndClearExpectations(profile_mock_store_.get()));

  // Clone() should not cause re-fetching from PasswordStore.
  EXPECT_CALL(*profile_mock_store_, GetLogins(_, _)).Times(0);
  auto clone = form_fetcher_->Clone();

  // Additionally, destroy the original FormFetcher. This should not invalidate
  // the data in |clone|.
  form_fetcher_.reset();

  EXPECT_EQ(FormFetcher::State::NOT_WAITING, clone->GetState());
  EXPECT_THAT(clone->GetInteractionsStats(), IsEmpty());
  EXPECT_THAT(clone->GetNonFederatedMatches(),
              UnorderedElementsAre(non_federated));
  EXPECT_THAT(clone->GetFederatedMatches(),
              UnorderedElementsAre(federated, android_federated));
  EXPECT_THAT(clone->GetInsecureCredentials(), UnorderedElementsAre(federated));
  MockConsumer consumer;
  EXPECT_CALL(consumer, OnFetchCompleted);
  clone->AddConsumer(&consumer);
}

// Cloning a FormFetcherImpl with some stats should result in an instance with
// the same stats.
TEST_P(FormFetcherImplTest, Clone_Stats) {
  Fetch();
  // Pass empty results to make the state NOT_WAITING.
  DeliverPasswordStoreResults(/*profile_store_results=*/{},
                              /*account_store_results=*/{});
  std::vector<InteractionsStats> stats(1);
  store_consumer()->OnGetSiteStatistics(std::move(stats));

  auto clone = form_fetcher_->Clone();
  EXPECT_EQ(1u, clone->GetInteractionsStats().size());
}

TEST_P(FormFetcherImplTest, Clone_Insecure) {
  Fetch();
  // Pass empty results to make the state NOT_WAITING.
  PasswordForm form = CreateNonFederated();
  InsecurityMetadata leaked_metadata{base::Time(), IsMuted(false),
                                     TriggerBackendNotification(true)};
  form.password_issues.insert({InsecureType::kLeaked, leaked_metadata});
  std::vector<PasswordForm> results = {form};
  DeliverPasswordStoreResults(/*profile_store_results=*/std::move(results),
                              /*account_store_results=*/{});

  auto clone = form_fetcher_->Clone();
  EXPECT_THAT(clone->GetInsecureCredentials(), UnorderedElementsAre(form));
}

// Check that removing consumers stops them from receiving store updates.
TEST_P(FormFetcherImplTest, RemoveConsumer) {
  Fetch();
  form_fetcher_->AddConsumer(&consumer_);
  form_fetcher_->RemoveConsumer(&consumer_);
  EXPECT_CALL(consumer_, OnFetchCompleted).Times(0);
  DeliverPasswordStoreResults(/*profile_store_results=*/{},
                              /*account_store_results=*/{});
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
      ->OnGetPasswordStoreResultsOrErrorFrom(profile_mock_store_.get(), {});
  if (account_mock_store_) {
    static_cast<PasswordStoreConsumer*>(form_fetcher)
        ->OnGetPasswordStoreResultsOrErrorFrom(account_mock_store_.get(), {});
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         FormFetcherImplTest,
                         testing::Values(false, true),
                         [](const ::testing::TestParamInfo<bool>& info) {
                           return info.param ? "ProfileAndAccountStore"
                                             : "ProfileStoreOnly";
                         });

class MultiStoreFormFetcherTest : public FormFetcherImplTestBase {
 public:
  MultiStoreFormFetcherTest()
      : FormFetcherImplTestBase(/*create_profile_store=*/true,
                                /*create_account_store=*/true) {}
};

TEST_F(MultiStoreFormFetcherTest, CloningMultiStoreFetcherClonesState) {
  Fetch();
  // Simulate a user in the account mode.
  ON_CALL(*client()->GetPasswordFeatureManager(), IsOptedInForAccountStorage())
      .WillByDefault(Return(true));
  ON_CALL(*client()->GetPasswordFeatureManager(), GetDefaultPasswordStore())
      .WillByDefault(Return(PasswordForm::Store::kAccountStore));

  // Create and push a blocked account store entry to complete the fetch.
  PasswordForm blocked = CreateBlocked();
  blocked.in_store = PasswordForm::Store::kAccountStore;
  std::vector<PasswordForm> results = {blocked};
  DeliverPasswordStoreResults({}, std::move(results));

  EXPECT_EQ(form_fetcher_->GetState(), FormFetcher::State::NOT_WAITING);
  EXPECT_TRUE(form_fetcher_->IsBlocklisted());

  // Cloning a fetcher that is done fetching keeps blocklisting information.
  form_fetcher_.reset(
      static_cast<FormFetcherImpl*>(form_fetcher_->Clone().release()));
  EXPECT_EQ(form_fetcher_->GetState(), FormFetcher::State::NOT_WAITING);
  EXPECT_TRUE(form_fetcher_->IsBlocklisted());
}

TEST_F(MultiStoreFormFetcherTest, CloningMultiStoreFetcherResumesFetch) {
  Fetch();
  // Simulate a user in the account mode.
  ON_CALL(*client()->GetPasswordFeatureManager(), IsOptedInForAccountStorage())
      .WillByDefault(Return(true));
  ON_CALL(*client()->GetPasswordFeatureManager(), GetDefaultPasswordStore())
      .WillByDefault(Return(PasswordForm::Store::kAccountStore));

  // A cloned multi-store fetcher must be a multi-store fetcher itself and
  // continue the fetching.
  form_fetcher_.reset(
      static_cast<FormFetcherImpl*>(form_fetcher_->Clone().release()));
  EXPECT_EQ(form_fetcher_->GetState(), FormFetcher::State::WAITING);
  EXPECT_FALSE(form_fetcher_->IsBlocklisted());

  // Create and push a blocked account store entry to complete the fetch.
  PasswordForm blocked = CreateBlocked();
  blocked.in_store = PasswordForm::Store::kAccountStore;
  std::vector<PasswordForm> results = {blocked};
  DeliverPasswordStoreResults({}, std::move(results));

  EXPECT_EQ(form_fetcher_->GetState(), FormFetcher::State::NOT_WAITING);
  EXPECT_TRUE(form_fetcher_->IsBlocklisted());
}

// Check that results from both stores are merged.
TEST_F(MultiStoreFormFetcherTest, MergeFromBothStores) {
  const base::Time kLastUsedNow = base::Time();
  const base::Time kLastUsedYesterday = kLastUsedNow - base::Days(1);
  Fetch();
  PasswordForm federated1 = CreateFederated("user", kLastUsedNow);
  PasswordForm federated2 = CreateFederated("user_B", kLastUsedNow);
  PasswordForm federated3 = CreateAndroidFederated("user_B", kLastUsedNow);
  PasswordForm non_federated1 = CreateNonFederated("user", kLastUsedYesterday);
  PasswordForm non_federated2 =
      CreateNonFederated("user_C", kLastUsedYesterday);
  PasswordForm non_federated3 = CreateNonFederated("user_D", kLastUsedNow);
  PasswordForm blocked = CreateBlocked();

  form_fetcher_->AddConsumer(&consumer_);

  // Pass response from the first store.
  std::vector<PasswordForm> results = {federated1, federated2, non_federated1,
                                       blocked};
  store_consumer()->OnGetPasswordStoreResultsOrErrorFrom(
      profile_mock_store_.get(), std::move(results));

  // We should be still waiting for the second store to respond.
  EXPECT_EQ(FormFetcher::State::WAITING, form_fetcher_->GetState());

  // Pass response from the second store.
  results.clear();
  results.push_back(federated3);
  results.push_back(non_federated2);
  results.push_back(non_federated3);

  EXPECT_CALL(consumer_, OnFetchCompleted);
  store_consumer()->OnGetPasswordStoreResultsOrErrorFrom(
      account_mock_store_.get(), std::move(results));

  EXPECT_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());

  // The results should be a merge of the response of both stores.
  EXPECT_THAT(
      form_fetcher_->GetNonFederatedMatches(),
      UnorderedElementsAre(non_federated1, non_federated2, non_federated3));
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(),
              UnorderedElementsAre(federated1, federated2, federated3));
  EXPECT_TRUE(form_fetcher_->IsBlocklisted());
  EXPECT_THAT(form_fetcher_->GetPreferredMatch(), Pointee(non_federated3));
}

TEST_F(MultiStoreFormFetcherTest, BlockedEntryInTheAccountStore) {
  Fetch();
  PasswordForm blocked = CreateBlocked();
  blocked.in_store = PasswordForm::Store::kAccountStore;

  // Deliver response from profile store and empty response from account.
  std::vector<PasswordForm> results = {blocked};
  DeliverPasswordStoreResults(std::move(results), {});

  // Simulate a user in the account mode.
  ON_CALL(*client()->GetPasswordFeatureManager(), IsOptedInForAccountStorage())
      .WillByDefault(Return(true));
  ON_CALL(*client()->GetPasswordFeatureManager(), GetDefaultPasswordStore())
      .WillByDefault(Return(PasswordForm::Store::kAccountStore));
  EXPECT_TRUE(form_fetcher_->IsBlocklisted());

  // Simulate a user in the profile mode.
  ON_CALL(*client()->GetPasswordFeatureManager(), GetDefaultPasswordStore())
      .WillByDefault(Return(PasswordForm::Store::kProfileStore));
  EXPECT_FALSE(form_fetcher_->IsBlocklisted());

  // Now simulate a user who isn't opted in for the account storage. In this
  // case, the blocked entry in the account store shouldn't matter,
  // independent of the mode.
  ON_CALL(*client()->GetPasswordFeatureManager(), IsOptedInForAccountStorage())
      .WillByDefault(Return(false));

  ON_CALL(*client()->GetPasswordFeatureManager(), GetDefaultPasswordStore())
      .WillByDefault(Return(PasswordForm::Store::kAccountStore));
  EXPECT_FALSE(form_fetcher_->IsBlocklisted());

  ON_CALL(*client()->GetPasswordFeatureManager(), GetDefaultPasswordStore())
      .WillByDefault(Return(PasswordForm::Store::kProfileStore));
  EXPECT_FALSE(form_fetcher_->IsBlocklisted());
}

TEST_F(MultiStoreFormFetcherTest, BlockedEntryInTheProfileStore) {
  Fetch();
  PasswordForm blocked = CreateBlocked();
  blocked.in_store = PasswordForm::Store::kProfileStore;

  // Deliver response from profile store and empty response from account.
  std::vector<PasswordForm> results = {blocked};
  DeliverPasswordStoreResults(std::move(results), {});

  // Simulate a user in the account mode.
  ON_CALL(*client()->GetPasswordFeatureManager(), IsOptedInForAccountStorage())
      .WillByDefault(Return(true));
  ON_CALL(*client()->GetPasswordFeatureManager(), GetDefaultPasswordStore())
      .WillByDefault(Return(PasswordForm::Store::kAccountStore));
  EXPECT_FALSE(form_fetcher_->IsBlocklisted());

  // Simulate a user in the profile mode.
  ON_CALL(*client()->GetPasswordFeatureManager(), GetDefaultPasswordStore())
      .WillByDefault(Return(PasswordForm::Store::kProfileStore));
  EXPECT_TRUE(form_fetcher_->IsBlocklisted());

  // Now simulate a user who isn't opted in for the account storage. In this
  // case, the blocked entry in the profile store should take effect, whatever
  // the mode is.
  ON_CALL(*client()->GetPasswordFeatureManager(), IsOptedInForAccountStorage())
      .WillByDefault(Return(false));

  ON_CALL(*client()->GetPasswordFeatureManager(), GetDefaultPasswordStore())
      .WillByDefault(Return(PasswordForm::Store::kAccountStore));
  EXPECT_TRUE(form_fetcher_->IsBlocklisted());

  ON_CALL(*client()->GetPasswordFeatureManager(), GetDefaultPasswordStore())
      .WillByDefault(Return(PasswordForm::Store::kProfileStore));
  EXPECT_TRUE(form_fetcher_->IsBlocklisted());
}

TEST_F(MultiStoreFormFetcherTest, MovingToAccountStoreIsBlocked) {
  Fetch();
  const GaiaIdHash kUser = GaiaIdHash::FromGaiaId("user");
  const GaiaIdHash kAnotherUser = GaiaIdHash::FromGaiaId("another_user");

  // Form that's blocked for |kUser| for "username1".
  PasswordForm blocked_form =
      CreateHTMLForm("www.url.com", "username1", "pass");
  blocked_form.in_store = PasswordForm::Store::kProfileStore;
  blocked_form.moving_blocked_for_list.push_back(kUser);

  // Form that not blocked for "username2".
  PasswordForm unblocked_form =
      CreateHTMLForm("www.url.com", "username2", "pass");
  unblocked_form.in_store = PasswordForm::Store::kProfileStore;

  // PSL form that's blocked for |kUser| for "psl_username".
  PasswordForm psl_form = CreateHTMLForm("psl.url.com", "psl_username", "pass");
  psl_form.match_type = PasswordForm::MatchType::kPSL;
  psl_form.in_store = PasswordForm::Store::kProfileStore;
  psl_form.moving_blocked_for_list.push_back(kUser);

  // Pass response from the local store.
  std::vector<PasswordForm> results = {blocked_form, unblocked_form, psl_form};

  // Deliver response from profile store and empty response from account.
  DeliverPasswordStoreResults(std::move(results), {});

  // Moving should be blocked for |kUser| and |form1|.
  EXPECT_TRUE(
      form_fetcher_->IsMovingBlocked(kUser, blocked_form.username_value));
  // Moving shouldn't be blocked for other usernames.
  EXPECT_FALSE(
      form_fetcher_->IsMovingBlocked(kUser, unblocked_form.username_value));
  // Moving shouldn't be blocked for other users.
  EXPECT_FALSE(form_fetcher_->IsMovingBlocked(kAnotherUser,
                                              blocked_form.username_value));
  // PSL match entries should be ignored when computing the moving blocklist
  // entries.
  EXPECT_FALSE(form_fetcher_->IsMovingBlocked(kUser, psl_form.username_value));
}

TEST_F(MultiStoreFormFetcherTest, InsecureCredentials) {
  Fetch();
  PasswordForm profile_form_insecure_credential =
      CreateHTMLForm("www.url.com", "username1", "pass");
  InsecurityMetadata leaked_metadata{base::Time(), IsMuted(false),
                                     TriggerBackendNotification(true)};
  profile_form_insecure_credential.password_issues.insert(
      {InsecureType::kLeaked, leaked_metadata});
  profile_form_insecure_credential.in_store =
      PasswordForm::Store::kProfileStore;
  std::vector<PasswordForm> profile_results;
  profile_results.push_back(profile_form_insecure_credential);

  PasswordForm account_form_insecure_credential =
      CreateHTMLForm("www.url.com", "username1", "pass");
  account_form_insecure_credential.password_issues.insert(
      {InsecureType::kLeaked, leaked_metadata});
  std::vector<PasswordForm> account_results;
  account_form_insecure_credential.in_store =
      PasswordForm::Store::kAccountStore;
  account_results.push_back(account_form_insecure_credential);

  DeliverPasswordStoreResults(std::move(profile_results),
                              std::move(account_results));

  EXPECT_THAT(form_fetcher_->GetInsecureCredentials(),
              testing::UnorderedElementsAre(profile_form_insecure_credential,
                                            account_form_insecure_credential));
}

TEST_P(FormFetcherImplTest, ProfileBackendErrorResetsOnNewFetch) {
  ASSERT_EQ(form_fetcher_->GetProfileStoreBackendError(), std::nullopt);

  Fetch();

  PasswordStoreBackendError error_results = PasswordStoreBackendError(
      PasswordStoreBackendErrorType::kAuthErrorResolvable);
  DeliverPasswordStoreResults(
      /*profile_store_results=*/std::move(error_results),
      /*account_store_results=*/{});

  EXPECT_EQ(form_fetcher_->GetProfileStoreBackendError().value(),
            PasswordStoreBackendError(
                PasswordStoreBackendErrorType::kAuthErrorResolvable));

  Fetch();

  PasswordForm form = CreateNonFederated();
  std::vector<PasswordForm> form_results;
  form_results.push_back(form);

  DeliverPasswordStoreResults(/*profile_store_results=*/std::move(form_results),
                              /*account_store_results=*/{});

  EXPECT_EQ(form_fetcher_->GetProfileStoreBackendError(), std::nullopt);
}

TEST_F(MultiStoreFormFetcherTest, AccountBackendErrorResetsOnNewFetch) {
  ASSERT_EQ(form_fetcher_->GetProfileStoreBackendError(), std::nullopt);

  Fetch();

  PasswordStoreBackendError error_results = PasswordStoreBackendError(
      PasswordStoreBackendErrorType::kAuthErrorResolvable);
  DeliverPasswordStoreResults(
      /*profile_store_results=*/{},
      /*account_store_results=*/std::move(error_results));

  EXPECT_EQ(form_fetcher_->GetAccountStoreBackendError().value(),
            PasswordStoreBackendError(
                PasswordStoreBackendErrorType::kAuthErrorResolvable));

  Fetch();

  PasswordForm form = CreateNonFederated();
  std::vector<PasswordForm> form_results;
  form_results.push_back(form);

  DeliverPasswordStoreResults(
      /*profile_store_results=*/{},
      /*account_store_results=*/std::move(form_results));

  EXPECT_EQ(form_fetcher_->GetProfileStoreBackendError(), std::nullopt);
}

class NoStoreFormFetcherTest : public FormFetcherImplTestBase {
 public:
  NoStoreFormFetcherTest()
      : FormFetcherImplTestBase(/*create_profile_store=*/false,
                                /*create_account_store=*/false) {}
};

TEST_F(NoStoreFormFetcherTest, NoStoreTest) {
  form_fetcher_->AddConsumer(&consumer_);
  EXPECT_CALL(consumer_, OnFetchCompleted);
  Fetch();
  EXPECT_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
  EXPECT_THAT(form_fetcher_->GetNonFederatedMatches(), IsEmpty());
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(), IsEmpty());
  EXPECT_FALSE(form_fetcher_->IsBlocklisted());
  EXPECT_EQ(form_fetcher_->GetProfileStoreBackendError(), std::nullopt);
  EXPECT_EQ(form_fetcher_->GetAccountStoreBackendError(), std::nullopt);
}

}  // namespace password_manager
