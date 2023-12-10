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
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/affiliation/mock_affiliation_service.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#include "components/webauthn/core/browser/test_passkey_model.h"  // nogncheck
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

namespace password_manager {

namespace {

constexpr base::TimeDelta kInitializationDelayOnStartup = base::Seconds(30);

using StrategyOnCacheMiss = AffiliationService::StrategyOnCacheMiss;

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

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

const char kTestRpIdFacetURIAlpha1[] = "one.alpha.example.com";
const char kTestRpIdFacetURIAlpha2[] = "two.alpha.example.com";
const char kTestExtensionRpId[] = "chrome-extension://test-extension-id";

sync_pb::WebauthnCredentialSpecifics GetTestPasskey(const char* rp_id) {
  sync_pb::WebauthnCredentialSpecifics passkey;
  passkey.set_rp_id(rp_id);
  passkey.set_credential_id(base::RandBytesAsString(16));
  passkey.set_sync_id(base::RandBytesAsString(16));
  return passkey;
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace

// Boolean parameters indicates whether affiliation service should support
// affiliated websites.
class AffiliationsPrefetcherTest : public testing::Test {
 protected:
  // testing::Test:
  void SetUp() override {
    mock_affiliation_service_ =
        std::make_unique<testing::StrictMock<MockAffiliationService>>();
    password_store()->Init(/*prefs=*/nullptr,
                           /*affiliated_match_helper=*/nullptr);

    prefetcher_ =
        std::make_unique<AffiliationsPrefetcher>(mock_affiliation_service());
  }

  void TearDown() override {
    (static_cast<KeyedService*>(prefetcher()))->Shutdown();
    prefetcher_.reset();
    if (password_store_) {
      DestroyPasswordStore();
    }
    mock_affiliation_service_.reset();
    // Clean up on the background thread.
    RunUntilIdle();
  }

  void RunDeferredInitialization() {
    task_environment_.RunUntilIdle();
    ExpectCallToTrimUnusedCache();
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

  void ExpectCallToPrefetch(std::string_view expected_facet_uri_spec) {
    EXPECT_CALL(*mock_affiliation_service(),
                Prefetch(FacetURI::FromCanonicalSpec(
                             std::string(expected_facet_uri_spec)),
                         base::Time::Max()))
        .RetiresOnSaturation();
  }

  void ExpectCallToCancelPrefetch(std::string_view expected_facet_uri_spec) {
    EXPECT_CALL(*mock_affiliation_service(),
                CancelPrefetch(FacetURI::FromCanonicalSpec(
                                   std::string(expected_facet_uri_spec)),
                               base::Time::Max()))
        .RetiresOnSaturation();
  }

  void ExpectCallToTrimCacheForFacetURI(
      std::string_view expected_facet_uri_spec) {
    EXPECT_CALL(*mock_affiliation_service(),
                TrimCacheForFacetURI(FacetURI::FromCanonicalSpec(
                    std::string(expected_facet_uri_spec))))
        .RetiresOnSaturation();
  }

  void ExpectCallToTrimUnusedCache() {
    EXPECT_CALL(*mock_affiliation_service(), TrimUnusedCache)
        .RetiresOnSaturation();
  }

  void ExpectKeepPrefetchForFacets(
      const std::vector<FacetURI>& expected_facets) {
    EXPECT_CALL(*mock_affiliation_service(),
                KeepPrefetchForFacets(expected_facets))
        .RetiresOnSaturation();
  }

  void ExpectPrefetchForTestLogins() {
    ExpectCallToPrefetch(kTestAndroidFacetURIAlpha3);
    ExpectCallToPrefetch(kTestAndroidFacetURIBeta2);
    ExpectCallToPrefetch(kTestAndroidFacetURIBeta3);
    ExpectCallToPrefetch(kTestAndroidFacetURIGamma);

    if (IsFillingAcrossAffiliatedWebsitesAllowed()) {
      ExpectCallToPrefetch(kTestWebFacetURIAlpha1);
      ExpectCallToPrefetch(kTestWebFacetURIAlpha2);
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

    if (IsFillingAcrossAffiliatedWebsitesAllowed()) {
      expected_facets.push_back(
          FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha1));
      expected_facets.push_back(
          FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha2));
    }

    ExpectKeepPrefetchForFacets(expected_facets);
  }

  void ExpectCancelPrefetchForTestLogins() {
    ExpectCallToCancelPrefetch(kTestAndroidFacetURIAlpha3);
    ExpectCallToCancelPrefetch(kTestAndroidFacetURIBeta2);
    ExpectCallToCancelPrefetch(kTestAndroidFacetURIBeta3);
    ExpectCallToCancelPrefetch(kTestAndroidFacetURIGamma);

    if (IsFillingAcrossAffiliatedWebsitesAllowed()) {
      ExpectCallToCancelPrefetch(kTestWebFacetURIAlpha1);
      ExpectCallToCancelPrefetch(kTestWebFacetURIAlpha2);
    }
  }

  void ExpectTrimCacheForTestLogins() {
    ExpectCallToTrimCacheForFacetURI(kTestAndroidFacetURIAlpha3);
    ExpectCallToTrimCacheForFacetURI(kTestAndroidFacetURIBeta2);
    ExpectCallToTrimCacheForFacetURI(kTestAndroidFacetURIBeta3);
    ExpectCallToTrimCacheForFacetURI(kTestAndroidFacetURIGamma);

    if (IsFillingAcrossAffiliatedWebsitesAllowed()) {
      ExpectCallToTrimCacheForFacetURI(kTestWebFacetURIAlpha1);
      ExpectCallToTrimCacheForFacetURI(kTestWebFacetURIAlpha2);
    }
  }

  void DestroyPasswordStore() {
    password_store_->ShutdownOnUIThread();
    password_store_ = nullptr;
  }

  TestPasswordStore* password_store() { return password_store_.get(); }

  MockAffiliationService* mock_affiliation_service() {
    return mock_affiliation_service_.get();
  }

  AffiliationsPrefetcher* prefetcher() { return prefetcher_.get(); }

  constexpr bool IsFillingAcrossAffiliatedWebsitesAllowed() {
#if BUILDFLAG(IS_ANDROID)
    return false;
#else
    return true;
#endif
  }

 private:
  void OnAffiliatedRealmsCallback(
      const std::vector<std::string>& affiliated_realms) {
    EXPECT_TRUE(expecting_result_callback_);
    expecting_result_callback_ = false;
    last_result_realms_ = affiliated_realms;
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::vector<std::string> last_result_realms_;
  bool expecting_result_callback_ = false;

  scoped_refptr<TestPasswordStore> password_store_ =
      base::MakeRefCounted<TestPasswordStore>();
  std::unique_ptr<AffiliationsPrefetcher> prefetcher_;

  std::unique_ptr<MockAffiliationService> mock_affiliation_service_;
};

// Verifies that affiliations for Android applications with pre-existing
// credentials on start-up are prefetched.
TEST_F(
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
TEST_F(AffiliationsPrefetcherTest,
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
TEST_F(AffiliationsPrefetcherTest,
       PrefetchAffiliationsForAndroidCredentialsAddedAfterInitialization) {
  ExpectKeepPrefetchForFacets({});
  ASSERT_NO_FATAL_FAILURE(RunDeferredInitialization());

  ExpectPrefetchForTestLogins();
  AddAndroidAndNonAndroidTestLogins();
}

TEST_F(AffiliationsPrefetcherTest,
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
TEST_F(AffiliationsPrefetcherTest, PrefetchBeforeTrimForPrimaryKeyUpdates) {
  AddAndroidAndNonAndroidTestLogins();

  ExpectKeepPrefetchForTestLogins();
  ASSERT_NO_FATAL_FAILURE(RunDeferredInitialization());

  ExpectCallToCancelPrefetch(kTestAndroidFacetURIAlpha3);

  {
    testing::InSequence in_sequence;
    ExpectCallToPrefetch(kTestAndroidFacetURIAlpha3);
    ExpectCallToTrimCacheForFacetURI(kTestAndroidFacetURIAlpha3);
  }

  PasswordForm old_form(GetTestAndroidCredentials(kTestAndroidRealmAlpha3));
  PasswordForm new_form(old_form);
  new_form.username_value = u"NewUserName";
  UpdateLoginWithPrimaryKey(new_form, old_form);
}

// Stores and removes four credentials for the same an Android application, and
// expects that Prefetch() and CancelPrefetch() will each be called four times.
TEST_F(AffiliationsPrefetcherTest,
       DuplicateCredentialsArePrefetchWithMultiplicity) {
  ExpectKeepPrefetchForFacets(
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
    ExpectCallToCancelPrefetch(kTestAndroidFacetURIAlpha3);
    ExpectCallToTrimCacheForFacetURI(kTestAndroidFacetURIAlpha3);
  }

  RemoveLogin(android_form);
  RemoveLogin(android_form2);
  RemoveLogin(android_form3);
  RemoveLogin(android_form4);
}

TEST_F(AffiliationsPrefetcherTest, OnLoginsRetained) {
  ExpectKeepPrefetchForFacets({});
  ASSERT_NO_FATAL_FAILURE(RunDeferredInitialization());

  std::vector<PasswordForm> forms = {
      GetTestAndroidCredentials(kTestWebFacetURIAlpha1),
      GetTestAndroidCredentials(kTestAndroidFacetURIBeta2)};
  std::vector<FacetURI> expected_facets;

  if (IsFillingAcrossAffiliatedWebsitesAllowed()) {
    expected_facets = {FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha1),
                       FacetURI::FromCanonicalSpec(kTestAndroidFacetURIBeta2)};
  } else {
    expected_facets = {FacetURI::FromCanonicalSpec(kTestAndroidFacetURIBeta2)};
  }

  ExpectKeepPrefetchForFacets(expected_facets);

  (static_cast<PasswordStoreInterface::Observer*>(prefetcher()))
      ->OnLoginsRetained(nullptr, forms);
}

TEST_F(AffiliationsPrefetcherTest, TestDisablePrefetch) {
  AddLoginAndWait(password_store(),
                  GetTestAndroidCredentials(kTestWebFacetURIAlpha1));
  prefetcher()->RegisterPasswordStore(password_store());

  ExpectKeepPrefetchForFacets({});
  prefetcher()->DisablePrefetching();

  // KeepPrefetchForFacets is no longer called even if calling
  // DisablePrefetching() again.
  EXPECT_CALL(*mock_affiliation_service(), KeepPrefetchForFacets).Times(0);
  prefetcher()->DisablePrefetching();

  RunUntilIdle();
  FastForwardBy(kInitializationDelayOnStartup);
}

TEST_F(AffiliationsPrefetcherTest, TestDisablePrefetchWithLoginsChanges) {
  prefetcher()->RegisterPasswordStore(password_store());

  ExpectKeepPrefetchForFacets({});
  prefetcher()->DisablePrefetching();

  RunUntilIdle();
  FastForwardBy(kInitializationDelayOnStartup);

  EXPECT_CALL(*mock_affiliation_service(), Prefetch).Times(0);
  AddLoginAndWait(password_store(),
                  GetTestAndroidCredentials(kTestWebFacetURIAlpha1));
}

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
    ExpectCallToTrimUnusedCache();
    prefetcher()->RegisterPasswordStore(password_store());
    prefetcher()->RegisterPasswordStore(account_password_store());
    FastForwardBy(kInitializationDelayOnStartup);
  }

 private:
  scoped_refptr<TestPasswordStore> account_password_store_ =
      base::MakeRefCounted<TestPasswordStore>();
};

TEST_F(AffiliationsPrefetcherWithTwoStoresTest, TestInitialPrefetch) {
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
  ExpectKeepPrefetchForFacets(expected_facets);

  ASSERT_NO_FATAL_FAILURE(RunDeferredInitialization());
}

TEST_F(AffiliationsPrefetcherWithTwoStoresTest, TestDuplicatesPrefetch) {
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
  ExpectKeepPrefetchForFacets(expected_facets);

  ASSERT_NO_FATAL_FAILURE(RunDeferredInitialization());
}

TEST_F(AffiliationsPrefetcherWithTwoStoresTest, TestLoginsChanged) {
  AddLoginAndWait(password_store(),
                  GetTestAndroidCredentials(kTestAndroidRealmAlpha3));

  ExpectKeepPrefetchForFacets(
      {FacetURI::FromCanonicalSpec(kTestAndroidFacetURIAlpha3)});

  ASSERT_NO_FATAL_FAILURE(RunDeferredInitialization());

  EXPECT_CALL(*mock_affiliation_service(),
              Prefetch(FacetURI::FromCanonicalSpec(kTestAndroidFacetURIBeta2),
                       base::Time::Max()));
  account_password_store()->AddLogin(
      GetTestAndroidCredentials(kTestAndroidRealmBeta2));
  RunUntilIdle();

  ExpectCallToTrimCacheForFacetURI(kTestAndroidFacetURIAlpha3);
  ExpectCallToCancelPrefetch(kTestAndroidFacetURIAlpha3);
  password_store()->RemoveLogin(
      GetTestAndroidCredentials(kTestAndroidRealmAlpha3));
  RunUntilIdle();
}

TEST_F(AffiliationsPrefetcherWithTwoStoresTest, TestStoreRegisteredLater) {
  AddLoginAndWait(password_store(),
                  GetTestAndroidCredentials(kTestAndroidRealmAlpha3));
  AddLoginAndWait(account_password_store(),
                  GetTestAndroidCredentials(kTestAndroidRealmBeta2));

  std::vector<FacetURI> expected_facets;
  expected_facets.push_back(
      FacetURI::FromCanonicalSpec(kTestAndroidFacetURIAlpha3));

  ExpectKeepPrefetchForFacets(expected_facets);
  ExpectCallToTrimUnusedCache();

  prefetcher()->RegisterPasswordStore(password_store());
  FastForwardBy(kInitializationDelayOnStartup);

  expected_facets.push_back(
      FacetURI::FromCanonicalSpec(kTestAndroidFacetURIBeta2));
  ExpectKeepPrefetchForFacets(expected_facets);
  ExpectCallToTrimUnusedCache();

  prefetcher()->RegisterPasswordStore(account_password_store());
  RunUntilIdle();
}

TEST_F(AffiliationsPrefetcherWithTwoStoresTest,
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
  ExpectKeepPrefetchForFacets(expected_facets);
  ExpectCallToTrimUnusedCache();

  prefetcher()->RegisterPasswordStore(password_store());
  prefetcher()->RegisterPasswordStore(account_password_store());
  RunUntilIdle();
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

class AffiliationsPrefetcherWithPasskeysTest
    : public AffiliationsPrefetcherTest {
 protected:
  webauthn::TestPasskeyModel* test_passkey_model() {
    return test_passkey_model_.get();
  }

  void RunDeferredInitialization() {
    RunUntilIdle();
    ExpectCallToTrimUnusedCache();
    prefetcher()->RegisterPasswordStore(password_store());
    prefetcher()->RegisterPasskeyModel(test_passkey_model_.get());
    FastForwardBy(kInitializationDelayOnStartup);
  }

 private:
  std::unique_ptr<webauthn::TestPasskeyModel> test_passkey_model_ =
      std::make_unique<webauthn::TestPasskeyModel>();
};

TEST_F(AffiliationsPrefetcherWithPasskeysTest, TestInitialPrefetch) {
  AddLoginAndWait(password_store(),
                  GetTestAndroidCredentials(kTestWebFacetURIAlpha1));
  test_passkey_model()->AddNewPasskeyForTesting(
      GetTestPasskey(kTestRpIdFacetURIAlpha2));

  std::vector<FacetURI> expected_facets;
  expected_facets.push_back(
      FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha1));
  expected_facets.push_back(
      FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha2));

  // Expect prefetch for passwords and passkeys.
  ExpectKeepPrefetchForFacets(expected_facets);

  ASSERT_NO_FATAL_FAILURE(RunDeferredInitialization());
}

TEST_F(AffiliationsPrefetcherWithPasskeysTest,
       TestPasskeyModelRegisteredLater) {
  AddLoginAndWait(password_store(),
                  GetTestAndroidCredentials(kTestWebFacetURIAlpha1));
  test_passkey_model()->AddNewPasskeyForTesting(
      GetTestPasskey(kTestRpIdFacetURIAlpha2));

  std::vector<FacetURI> expected_facets;
  expected_facets.push_back(
      FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha1));

  ExpectKeepPrefetchForFacets(expected_facets);
  ExpectCallToTrimUnusedCache();

  prefetcher()->RegisterPasswordStore(password_store());
  FastForwardBy(kInitializationDelayOnStartup);

  expected_facets.push_back(
      FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha2));
  ExpectCallToPrefetch(kTestWebFacetURIAlpha2);

  prefetcher()->RegisterPasskeyModel(test_passkey_model());
  RunUntilIdle();
}

TEST_F(AffiliationsPrefetcherWithPasskeysTest, TestNewPasskeyDownloaded) {
  prefetcher()->RegisterPasskeyModel(test_passkey_model());
  ExpectKeepPrefetchForFacets({});
  ExpectCallToTrimUnusedCache();
  FastForwardBy(kInitializationDelayOnStartup);

  std::vector<FacetURI> expected_facets{
      FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha1)};
  ExpectCallToPrefetch(kTestWebFacetURIAlpha1);
  test_passkey_model()->AddNewPasskeyForTesting(
      GetTestPasskey(kTestRpIdFacetURIAlpha1));
}

TEST_F(AffiliationsPrefetcherWithPasskeysTest, TestPasskeyDeleted) {
  sync_pb::WebauthnCredentialSpecifics passkey =
      GetTestPasskey(kTestRpIdFacetURIAlpha1);
  test_passkey_model()->AddNewPasskeyForTesting(passkey);

  std::vector<FacetURI> expected_facets{
      FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha1)};
  ExpectKeepPrefetchForFacets(expected_facets);
  ASSERT_NO_FATAL_FAILURE(RunDeferredInitialization());

  ExpectCallToCancelPrefetch(kTestWebFacetURIAlpha1);
  test_passkey_model()->DeletePasskey(passkey.credential_id());
  RunUntilIdle();
}

TEST_F(AffiliationsPrefetcherWithPasskeysTest, TestInvalidFacetsIgnored) {
  test_passkey_model()->AddNewPasskeyForTesting(
      GetTestPasskey(kTestRpIdFacetURIAlpha1));
  test_passkey_model()->AddNewPasskeyForTesting(
      GetTestPasskey(kTestExtensionRpId));

  std::vector<FacetURI> expected_facets{
      FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha1)};
  ExpectKeepPrefetchForFacets(expected_facets);
  ASSERT_NO_FATAL_FAILURE(RunDeferredInitialization());
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace password_manager
