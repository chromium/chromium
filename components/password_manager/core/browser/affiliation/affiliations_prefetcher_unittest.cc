// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/affiliation/affiliations_prefetcher.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/affiliation/mock_affiliation_service.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

constexpr base::TimeDelta kInitializationDelayOnStartup = base::Seconds(30);

using StrategyOnCacheMiss = AffiliationService::StrategyOnCacheMiss;

class OverloadedMockAffiliationService : public MockAffiliationService {
 public:
  OverloadedMockAffiliationService() {
    testing::DefaultValue<AffiliatedFacets>::Set(AffiliatedFacets());
  }

  MOCK_METHOD(AffiliatedFacets,
              OnGetAffiliationsAndBrandingCalled,
              (const FacetURI&, StrategyOnCacheMiss));

  void GetAffiliationsAndBranding(const FacetURI& facet_uri,
                                  StrategyOnCacheMiss cache_miss_strategy,
                                  ResultCallback result_callback) override {
    AffiliatedFacets affiliation =
        OnGetAffiliationsAndBrandingCalled(facet_uri, cache_miss_strategy);
    std::move(result_callback).Run(affiliation, !affiliation.empty());
  }

  void ExpectCallToGetAffiliationsAndBrandingAndSucceedWithResult(
      const FacetURI& expected_facet_uri,
      StrategyOnCacheMiss expected_cache_miss_strategy,
      const AffiliatedFacets& affiliations_to_return) {
    EXPECT_CALL(*this, OnGetAffiliationsAndBrandingCalled(
                           expected_facet_uri, expected_cache_miss_strategy))
        .WillOnce(testing::Return(affiliations_to_return));
  }

  void ExpectCallToGetAffiliationsAndBrandingAndEmulateFailure(
      const FacetURI& expected_facet_uri,
      StrategyOnCacheMiss expected_cache_miss_strategy) {
    EXPECT_CALL(*this, OnGetAffiliationsAndBrandingCalled(
                           expected_facet_uri, expected_cache_miss_strategy))
        .WillOnce(testing::Return(AffiliatedFacets()));
  }

  void ExpectCallToPrefetch(const char* expected_facet_uri_spec) {
    EXPECT_CALL(*this,
                Prefetch(FacetURI::FromCanonicalSpec(expected_facet_uri_spec),
                         base::Time::Max()))
        .RetiresOnSaturation();
  }

  void ExpectCallToCancelPrefetch(const char* expected_facet_uri_spec) {
    EXPECT_CALL(*this, CancelPrefetch(
                           FacetURI::FromCanonicalSpec(expected_facet_uri_spec),
                           base::Time::Max()))
        .RetiresOnSaturation();
  }

  void ExpectCallToTrimCacheForFacetURI(const char* expected_facet_uri_spec) {
    EXPECT_CALL(*this, TrimCacheForFacetURI(FacetURI::FromCanonicalSpec(
                           expected_facet_uri_spec)))
        .RetiresOnSaturation();
  }

  void ExpectCallToTrimUnusedCache() {
    EXPECT_CALL(*this, TrimUnusedCache).RetiresOnSaturation();
  }

  void ExpectKeepPrefetchForFacets(
      const std::vector<FacetURI>& expected_facets) {
    EXPECT_CALL(*this, KeepPrefetchForFacets(expected_facets))
        .RetiresOnSaturation();
  }
};

const char kTestWebFacetURIAlpha1[] = "https://one.alpha.example.com";
const char kTestWebFacetURIAlpha2[] = "https://two.alpha.example.com";
const char kTestAndroidFacetURIAlpha3[] =
    "android://hash@com.example.alpha.android";
const char kTestWebRealmAlpha1[] = "https://one.alpha.example.com/";
const char kTestWebRealmAlpha2[] = "https://two.alpha.example.com/";
const char kTestAndroidRealmAlpha3[] =
    "android://hash@com.example.alpha.android/";

const char kTestAndroidFacetURIBeta2[] =
    "android://hash@com.example.beta.android";
const char kTestAndroidFacetURIBeta3[] =
    "android://hash@com.yetanother.beta.android";
const char kTestAndroidRealmBeta2[] =
    "android://hash@com.example.beta.android/";
const char kTestAndroidRealmBeta3[] =
    "android://hash@com.yetanother.beta.android/";

const char kTestAndroidFacetURIGamma[] =
    "android://hash@com.example.gamma.android";
const char kTestAndroidRealmGamma[] =
    "android://hash@com.example.gamma.android";

const char16_t kTestUsername[] = u"JohnDoe";
const char16_t kTestPassword[] = u"secret";

PasswordForm GetTestAndroidCredentials(const char* signon_realm) {
  PasswordForm form;
  form.scheme = PasswordForm::Scheme::kHtml;
  form.signon_realm = signon_realm;
  form.username_value = kTestUsername;
  form.password_value = kTestPassword;
  return form;
}

PasswordForm GetTestBlocklistedAndroidCredentials(const char* signon_realm) {
  PasswordForm form = GetTestAndroidCredentials(signon_realm);
  form.blocked_by_user = true;
  form.username_value.clear();
  form.password_value.clear();
  return form;
}

}  // namespace

// Boolean parameters indicates whether affiliation service should support
// affiliated websites.
class AffiliationsPrefetcherTest : public testing::Test,
                                   public ::testing::WithParamInterface<bool> {
 public:
  AffiliationsPrefetcherTest() {
    feature_list_.InitWithFeatureState(
        features::kFillingAcrossAffiliatedWebsites, GetParam());
  }

 protected:
  // testing::Test:
  void SetUp() override {
    mock_affiliation_service_ = std::make_unique<
        testing::StrictMock<OverloadedMockAffiliationService>>();
    password_store()->Init(/*prefs=*/nullptr,
                           /*affiliated_match_helper=*/nullptr);

    prefetcher_ =
        std::make_unique<AffiliationsPrefetcher>(mock_affiliation_service());
  }

  void TearDown() override {
    (static_cast<KeyedService*>(prefetcher()))->Shutdown();
    if (password_store_) {
      DestroyPasswordStore();
    }
    mock_affiliation_service_.reset();
    // Clean up on the background thread.
    RunUntilIdle();
  }

  void RunDeferredInitialization() {
    task_environment_.RunUntilIdle();
    mock_affiliation_service()->ExpectCallToTrimUnusedCache();
    prefetcher_->RegisterPasswordStore(password_store());
    task_environment_.FastForwardBy(kInitializationDelayOnStartup);
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  void AddLoginAndWait(PasswordStoreInterface* store,
                       const PasswordForm& form) {
    store->AddLogin(form);
    RunUntilIdle();
  }

  void UpdateLoginWithPrimaryKey(const PasswordForm& new_form,
                                 const PasswordForm& old_primary_key) {
    password_store_->UpdateLoginWithPrimaryKey(new_form, old_primary_key);
    RunUntilIdle();
  }

  void RemoveLogin(const PasswordForm& form) {
    password_store_->RemoveLogin(form);
    RunUntilIdle();
  }

  void AddAndroidAndNonAndroidTestLogins() {
    AddLoginAndWait(password_store(),
                    GetTestAndroidCredentials(kTestAndroidRealmAlpha3));
    AddLoginAndWait(password_store(),
                    GetTestAndroidCredentials(kTestAndroidRealmBeta2));
    AddLoginAndWait(password_store(), GetTestBlocklistedAndroidCredentials(
                                          kTestAndroidRealmBeta3));
    AddLoginAndWait(password_store(),
                    GetTestAndroidCredentials(kTestAndroidRealmGamma));

    AddLoginAndWait(password_store(),
                    GetTestAndroidCredentials(kTestWebRealmAlpha1));
    AddLoginAndWait(password_store(),
                    GetTestAndroidCredentials(kTestWebRealmAlpha2));
  }

  void RemoveAndroidAndNonAndroidTestLogins() {
    RemoveLogin(GetTestAndroidCredentials(kTestAndroidRealmAlpha3));
    RemoveLogin(GetTestAndroidCredentials(kTestAndroidRealmBeta2));
    RemoveLogin(GetTestBlocklistedAndroidCredentials(kTestAndroidRealmBeta3));
    RemoveLogin(GetTestAndroidCredentials(kTestAndroidRealmGamma));

    RemoveLogin(GetTestAndroidCredentials(kTestWebRealmAlpha1));
    RemoveLogin(GetTestAndroidCredentials(kTestWebRealmAlpha2));
  }

  void ExpectPrefetchForTestLogins() {
    mock_affiliation_service()->ExpectCallToPrefetch(
        kTestAndroidFacetURIAlpha3);
    mock_affiliation_service()->ExpectCallToPrefetch(kTestAndroidFacetURIBeta2);
    mock_affiliation_service()->ExpectCallToPrefetch(kTestAndroidFacetURIBeta3);
    mock_affiliation_service()->ExpectCallToPrefetch(kTestAndroidFacetURIGamma);

    if (base::FeatureList::IsEnabled(
            features::kFillingAcrossAffiliatedWebsites)) {
      mock_affiliation_service()->ExpectCallToPrefetch(kTestWebFacetURIAlpha1);
      mock_affiliation_service()->ExpectCallToPrefetch(kTestWebFacetURIAlpha2);
    }
  }

  void ExpectKeepPrefetchForTestLogins() {
    std::vector<FacetURI> expected_facets;
    expected_facets.push_back(
        FacetURI::FromCanonicalSpec(kTestAndroidFacetURIAlpha3));
    expected_facets.push_back(
        FacetURI::FromCanonicalSpec(kTestAndroidFacetURIBeta2));
    expected_facets.push_back(
        FacetURI::FromCanonicalSpec(kTestAndroidFacetURIGamma));
    expected_facets.push_back(
        FacetURI::FromCanonicalSpec(kTestAndroidFacetURIBeta3));

    if (base::FeatureList::IsEnabled(
            features::kFillingAcrossAffiliatedWebsites)) {
      expected_facets.push_back(
          FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha1));
      expected_facets.push_back(
          FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha2));
    }

    mock_affiliation_service()->ExpectKeepPrefetchForFacets(expected_facets);
  }

  void ExpectCancelPrefetchForTestLogins() {
    mock_affiliation_service()->ExpectCallToCancelPrefetch(
        kTestAndroidFacetURIAlpha3);
    mock_affiliation_service()->ExpectCallToCancelPrefetch(
        kTestAndroidFacetURIBeta2);
    mock_affiliation_service()->ExpectCallToCancelPrefetch(
        kTestAndroidFacetURIBeta3);
    mock_affiliation_service()->ExpectCallToCancelPrefetch(
        kTestAndroidFacetURIGamma);

    if (base::FeatureList::IsEnabled(
            features::kFillingAcrossAffiliatedWebsites)) {
      mock_affiliation_service()->ExpectCallToCancelPrefetch(
          kTestWebFacetURIAlpha1);
      mock_affiliation_service()->ExpectCallToCancelPrefetch(
          kTestWebFacetURIAlpha2);
    }
  }

  void ExpectTrimCacheForTestLogins() {
    mock_affiliation_service()->ExpectCallToTrimCacheForFacetURI(
        kTestAndroidFacetURIAlpha3);
    mock_affiliation_service()->ExpectCallToTrimCacheForFacetURI(
        kTestAndroidFacetURIBeta2);
    mock_affiliation_service()->ExpectCallToTrimCacheForFacetURI(
        kTestAndroidFacetURIBeta3);
    mock_affiliation_service()->ExpectCallToTrimCacheForFacetURI(
        kTestAndroidFacetURIGamma);

    if (base::FeatureList::IsEnabled(
            features::kFillingAcrossAffiliatedWebsites)) {
      mock_affiliation_service()->ExpectCallToTrimCacheForFacetURI(
          kTestWebFacetURIAlpha1);
      mock_affiliation_service()->ExpectCallToTrimCacheForFacetURI(
          kTestWebFacetURIAlpha2);
    }
  }

  void DestroyPasswordStore() {
    password_store_->ShutdownOnUIThread();
    password_store_ = nullptr;
  }

  TestPasswordStore* password_store() { return password_store_.get(); }

  OverloadedMockAffiliationService* mock_affiliation_service() {
    return mock_affiliation_service_.get();
  }

  AffiliationsPrefetcher* prefetcher() { return prefetcher_.get(); }

 private:
  void OnAffiliatedRealmsCallback(
      const std::vector<std::string>& affiliated_realms) {
    EXPECT_TRUE(expecting_result_callback_);
    expecting_result_callback_ = false;
    last_result_realms_ = affiliated_realms;
  }

  base::test::ScopedFeatureList feature_list_;
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::vector<std::string> last_result_realms_;
  bool expecting_result_callback_ = false;

  scoped_refptr<TestPasswordStore> password_store_ =
      base::MakeRefCounted<TestPasswordStore>();
  std::unique_ptr<AffiliationsPrefetcher> prefetcher_;

  std::unique_ptr<OverloadedMockAffiliationService> mock_affiliation_service_;
};

// Verifies that affiliations for Android applications with pre-existing
// credentials on start-up are prefetched.
TEST_P(
    AffiliationsPrefetcherTest,
    PrefetchAffiliationsAndBrandingForPreexistingAndroidCredentialsOnStartup) {
  RunUntilIdle();

  AddAndroidAndNonAndroidTestLogins();
  RunUntilIdle();

  ExpectKeepPrefetchForTestLogins();
  ASSERT_NO_FATAL_FAILURE(RunDeferredInitialization());
}

// Stores credentials for Android applications between Initialize() and
// DoDeferredInitialization(). Verifies that corresponding affiliation
// information gets prefetched.
TEST_P(AffiliationsPrefetcherTest,
       PrefetchAffiliationsForAndroidCredentialsAddedInInitializationDelay) {
  // Wait until PasswordStore initialisation is complete and
  // AffiliationsPrefetcher::Initialize is called.
  RunUntilIdle();

  AddAndroidAndNonAndroidTestLogins();

  ExpectKeepPrefetchForTestLogins();
  ASSERT_NO_FATAL_FAILURE(RunDeferredInitialization());
}

// Stores credentials for Android applications after DoDeferredInitialization().
// Verifies that corresponding affiliation information gets prefetched.
TEST_P(AffiliationsPrefetcherTest,
       PrefetchAffiliationsForAndroidCredentialsAddedAfterInitialization) {
  mock_affiliation_service()->ExpectKeepPrefetchForFacets({});
  ASSERT_NO_FATAL_FAILURE(RunDeferredInitialization());

  ExpectPrefetchForTestLogins();
  AddAndroidAndNonAndroidTestLogins();
}

TEST_P(AffiliationsPrefetcherTest,
       CancelPrefetchingAffiliationsAndBrandingForRemovedAndroidCredentials) {
  AddAndroidAndNonAndroidTestLogins();

  ExpectKeepPrefetchForTestLogins();
  ASSERT_NO_FATAL_FAILURE(RunDeferredInitialization());

  ExpectCancelPrefetchForTestLogins();
  ExpectTrimCacheForTestLogins();

  RemoveAndroidAndNonAndroidTestLogins();
}

// Verify that whenever the primary key is updated for a credential (in which
// case both REMOVE and ADD change notifications are sent out), then Prefetch()
// is called in response to the addition before the call to
// TrimCacheForFacetURI() in response to the removal, so that cached data is not
// deleted and then immediately re-fetched.
TEST_P(AffiliationsPrefetcherTest, PrefetchBeforeTrimForPrimaryKeyUpdates) {
  AddAndroidAndNonAndroidTestLogins();

  ExpectKeepPrefetchForTestLogins();
  ASSERT_NO_FATAL_FAILURE(RunDeferredInitialization());

  mock_affiliation_service()->ExpectCallToCancelPrefetch(
      kTestAndroidFacetURIAlpha3);

  {
    testing::InSequence in_sequence;
    mock_affiliation_service()->ExpectCallToPrefetch(
        kTestAndroidFacetURIAlpha3);
    mock_affiliation_service()->ExpectCallToTrimCacheForFacetURI(
        kTestAndroidFacetURIAlpha3);
  }

  PasswordForm old_form(GetTestAndroidCredentials(kTestAndroidRealmAlpha3));
  PasswordForm new_form(old_form);
  new_form.username_value = u"NewUserName";
  UpdateLoginWithPrimaryKey(new_form, old_form);
}

// Stores and removes four credentials for the same an Android application, and
// expects that Prefetch() and CancelPrefetch() will each be called four times.
TEST_P(AffiliationsPrefetcherTest,
       DuplicateCredentialsArePrefetchWithMultiplicity) {
  mock_affiliation_service()->ExpectKeepPrefetchForFacets(
      {FacetURI::FromCanonicalSpec(kTestAndroidFacetURIAlpha3),
       FacetURI::FromCanonicalSpec(kTestAndroidFacetURIAlpha3),
       FacetURI::FromCanonicalSpec(kTestAndroidFacetURIAlpha3)});

  PasswordForm android_form(GetTestAndroidCredentials(kTestAndroidRealmAlpha3));
  password_store()->AddLogin(android_form);

  // Store two credentials before initialization.
  PasswordForm android_form2(android_form);
  android_form2.username_value = u"JohnDoe2";
  password_store()->AddLogin(android_form2);

  // Wait until PasswordStore initializes AffiliationsPrefetcher and processes
  // added logins.
  RunUntilIdle();

  // Store one credential between initialization and deferred initialization.
  PasswordForm android_form3(android_form);
  android_form3.username_value = u"JohnDoe3";
  password_store()->AddLogin(android_form3);

  ASSERT_NO_FATAL_FAILURE(RunDeferredInitialization());

  EXPECT_CALL(*mock_affiliation_service(),
              Prefetch(FacetURI::FromCanonicalSpec(kTestAndroidFacetURIAlpha3),
                       base::Time::Max()))
      .Times(1);

  // Store one credential after deferred initialization.
  PasswordForm android_form4(android_form);
  android_form4.username_value = u"JohnDoe4";
  AddLoginAndWait(password_store(), android_form4);

  for (size_t i = 0; i < 4; ++i) {
    mock_affiliation_service()->ExpectCallToCancelPrefetch(
        kTestAndroidFacetURIAlpha3);
    mock_affiliation_service()->ExpectCallToTrimCacheForFacetURI(
        kTestAndroidFacetURIAlpha3);
  }

  RemoveLogin(android_form);
  RemoveLogin(android_form2);
  RemoveLogin(android_form3);
  RemoveLogin(android_form4);
}

TEST_P(AffiliationsPrefetcherTest, OnLoginsRetained) {
  mock_affiliation_service()->ExpectKeepPrefetchForFacets({});
  ASSERT_NO_FATAL_FAILURE(RunDeferredInitialization());

  std::vector<PasswordForm> forms = {
      GetTestAndroidCredentials(kTestWebFacetURIAlpha1),
      GetTestAndroidCredentials(kTestAndroidFacetURIBeta2)};
  std::vector<FacetURI> expected_facets;

  if (base::FeatureList::IsEnabled(
          features::kFillingAcrossAffiliatedWebsites)) {
    expected_facets = {FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha1),
                       FacetURI::FromCanonicalSpec(kTestAndroidFacetURIBeta2)};
  } else {
    expected_facets = {FacetURI::FromCanonicalSpec(kTestAndroidFacetURIBeta2)};
  }

  mock_affiliation_service()->ExpectKeepPrefetchForFacets(expected_facets);

  (static_cast<PasswordStoreInterface::Observer*>(prefetcher()))
      ->OnLoginsRetained(nullptr, forms);
}

INSTANTIATE_TEST_SUITE_P(FillingAcrossAffiliatedWebsites,
                         AffiliationsPrefetcherTest,
                         ::testing::Bool());

class AffiliationsPrefetcherWithTwoStoresTest
    : public AffiliationsPrefetcherTest {
 protected:
  void SetUp() override {
    AffiliationsPrefetcherTest::SetUp();
    account_password_store_->Init(/*prefs=*/nullptr, nullptr);
  }

  void TearDown() override {
    AffiliationsPrefetcherTest::TearDown();
    account_password_store_->ShutdownOnUIThread();
    account_password_store_ = nullptr;
  }

  TestPasswordStore* account_password_store() {
    return account_password_store_.get();
  }

  void RunDeferredInitialization() {
    RunUntilIdle();
    mock_affiliation_service()->ExpectCallToTrimUnusedCache();
    prefetcher()->RegisterPasswordStore(password_store());
    prefetcher()->RegisterPasswordStore(account_password_store());
    FastForwardBy(kInitializationDelayOnStartup);
  }

 private:
  scoped_refptr<TestPasswordStore> account_password_store_ =
      base::MakeRefCounted<TestPasswordStore>();
};

INSTANTIATE_TEST_SUITE_P(FillingAcrossAffiliatedWebsites,
                         AffiliationsPrefetcherWithTwoStoresTest,
                         ::testing::Bool());

TEST_P(AffiliationsPrefetcherWithTwoStoresTest, TestInitialPrefetch) {
  AddLoginAndWait(password_store(),
                  GetTestAndroidCredentials(kTestAndroidRealmAlpha3));
  AddLoginAndWait(account_password_store(),
                  GetTestAndroidCredentials(kTestAndroidRealmBeta2));

  std::vector<FacetURI> expected_facets;
  expected_facets.push_back(
      FacetURI::FromCanonicalSpec(kTestAndroidFacetURIAlpha3));
  expected_facets.push_back(
      FacetURI::FromCanonicalSpec(kTestAndroidFacetURIBeta2));

  // Expect prefetch for passwords from both stores.
  mock_affiliation_service()->ExpectKeepPrefetchForFacets(expected_facets);

  ASSERT_NO_FATAL_FAILURE(RunDeferredInitialization());
}

TEST_P(AffiliationsPrefetcherWithTwoStoresTest, TestDuplicatesPrefetch) {
  AddLoginAndWait(password_store(),
                  GetTestAndroidCredentials(kTestAndroidRealmAlpha3));
  AddLoginAndWait(account_password_store(),
                  GetTestAndroidCredentials(kTestAndroidRealmAlpha3));

  std::vector<FacetURI> expected_facets;
  expected_facets.push_back(
      FacetURI::FromCanonicalSpec(kTestAndroidFacetURIAlpha3));
  expected_facets.push_back(
      FacetURI::FromCanonicalSpec(kTestAndroidFacetURIAlpha3));

  // Expect prefetch for passwords from both stores.
  mock_affiliation_service()->ExpectKeepPrefetchForFacets(expected_facets);

  ASSERT_NO_FATAL_FAILURE(RunDeferredInitialization());
}

TEST_P(AffiliationsPrefetcherWithTwoStoresTest, TestLoginsChanged) {
  AddLoginAndWait(password_store(),
                  GetTestAndroidCredentials(kTestAndroidRealmAlpha3));

  mock_affiliation_service()->ExpectKeepPrefetchForFacets(
      {FacetURI::FromCanonicalSpec(kTestAndroidFacetURIAlpha3)});

  ASSERT_NO_FATAL_FAILURE(RunDeferredInitialization());

  EXPECT_CALL(*mock_affiliation_service(),
              Prefetch(FacetURI::FromCanonicalSpec(kTestAndroidFacetURIBeta2),
                       base::Time::Max()));
  account_password_store()->AddLogin(
      GetTestAndroidCredentials(kTestAndroidRealmBeta2));
  RunUntilIdle();

  mock_affiliation_service()->ExpectCallToTrimCacheForFacetURI(
      kTestAndroidFacetURIAlpha3);
  mock_affiliation_service()->ExpectCallToCancelPrefetch(
      kTestAndroidFacetURIAlpha3);
  password_store()->RemoveLogin(
      GetTestAndroidCredentials(kTestAndroidRealmAlpha3));
  RunUntilIdle();
}

TEST_P(AffiliationsPrefetcherWithTwoStoresTest, TestStoreRegisteredLater) {
  AddLoginAndWait(password_store(),
                  GetTestAndroidCredentials(kTestAndroidRealmAlpha3));
  AddLoginAndWait(account_password_store(),
                  GetTestAndroidCredentials(kTestAndroidRealmBeta2));

  std::vector<FacetURI> expected_facets;
  expected_facets.push_back(
      FacetURI::FromCanonicalSpec(kTestAndroidFacetURIAlpha3));

  mock_affiliation_service()->ExpectKeepPrefetchForFacets(expected_facets);
  mock_affiliation_service()->ExpectCallToTrimUnusedCache();

  prefetcher()->RegisterPasswordStore(password_store());
  FastForwardBy(kInitializationDelayOnStartup);

  expected_facets.push_back(
      FacetURI::FromCanonicalSpec(kTestAndroidFacetURIBeta2));
  mock_affiliation_service()->ExpectKeepPrefetchForFacets(expected_facets);
  mock_affiliation_service()->ExpectCallToTrimUnusedCache();

  prefetcher()->RegisterPasswordStore(account_password_store());
  RunUntilIdle();
}

TEST_P(AffiliationsPrefetcherWithTwoStoresTest,
       TestStoresRegisteredAfterDelay) {
  AddLoginAndWait(password_store(),
                  GetTestAndroidCredentials(kTestAndroidRealmAlpha3));
  AddLoginAndWait(account_password_store(),
                  GetTestAndroidCredentials(kTestAndroidRealmBeta2));

  FastForwardBy(kInitializationDelayOnStartup);

  std::vector<FacetURI> expected_facets;
  expected_facets.push_back(
      FacetURI::FromCanonicalSpec(kTestAndroidFacetURIAlpha3));
  expected_facets.push_back(
      FacetURI::FromCanonicalSpec(kTestAndroidFacetURIBeta2));
  mock_affiliation_service()->ExpectKeepPrefetchForFacets(expected_facets);
  mock_affiliation_service()->ExpectCallToTrimUnusedCache();

  prefetcher()->RegisterPasswordStore(password_store());
  prefetcher()->RegisterPasswordStore(account_password_store());
  RunUntilIdle();
}

}  // namespace password_manager
