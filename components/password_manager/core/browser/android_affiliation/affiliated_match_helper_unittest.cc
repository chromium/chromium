// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/android_affiliation/affiliated_match_helper.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/android_affiliation/android_affiliation_service.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

using StrategyOnCacheMiss = AndroidAffiliationService::StrategyOnCacheMiss;

class MockAndroidAffiliationService : public AndroidAffiliationService {
 public:
  MockAndroidAffiliationService() : AndroidAffiliationService(nullptr) {
    testing::DefaultValue<AffiliatedFacets>::Set(AffiliatedFacets());
  }

  MOCK_METHOD2(OnGetAffiliationsAndBrandingCalled,
               AffiliatedFacets(const FacetURI&, StrategyOnCacheMiss));
  MOCK_METHOD2(Prefetch, void(const FacetURI&, const base::Time&));
  MOCK_METHOD2(CancelPrefetch, void(const FacetURI&, const base::Time&));
  MOCK_METHOD1(TrimCacheForFacetURI, void(const FacetURI&));

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
};

const char kTestWebFacetURIAlpha1[] = "https://one.alpha.example.com";
const char kTestWebFacetURIAlpha2[] = "https://two.alpha.example.com";
const char kTestAndroidFacetURIAlpha3[] =
    "android://hash@com.example.alpha.android";
const char kTestAndroidFacetNameAlpha3[] = "Facet Name Alpha 3";
const char kTestAndroidFacetIconURLAlpha3[] = "https://example.com/alpha_3.png";
const char kTestWebRealmAlpha1[] = "https://one.alpha.example.com/";
const char kTestWebRealmAlpha2[] = "https://two.alpha.example.com/";
const char kTestAndroidRealmAlpha3[] =
    "android://hash@com.example.alpha.android/";

const char kTestWebFacetURIBeta1[] = "https://one.beta.example.com";
const char kTestAndroidFacetURIBeta2[] =
    "android://hash@com.example.beta.android";
const char kTestAndroidFacetNameBeta2[] = "Facet Name Beta 2";
const char kTestAndroidFacetIconURLBeta2[] = "https://example.com/beta_2.png";
const char kTestAndroidFacetURIBeta3[] =
    "android://hash@com.yetanother.beta.android";
const char kTestAndroidFacetNameBeta3[] = "Facet Name Beta 3";
const char kTestAndroidFacetIconURLBeta3[] = "https://example.com/beta_3.png";
const char kTestWebRealmBeta1[] = "https://one.beta.example.com/";
const char kTestAndroidRealmBeta2[] =
    "android://hash@com.example.beta.android/";
const char kTestAndroidRealmBeta3[] =
    "android://hash@com.yetanother.beta.android/";

const char kTestAndroidFacetURIGamma[] =
    "android://hash@com.example.gamma.android";
const char kTestAndroidRealmGamma[] =
    "android://hash@com.example.gamma.android";

const char kTestUsername[] = "JohnDoe";
const char kTestPassword[] = "secret";

AffiliatedFacets GetTestEquivalenceClassAlpha() {
  return {
      {FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha1)},
      {FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha2)},
      {FacetURI::FromCanonicalSpec(kTestAndroidFacetURIAlpha3),
       FacetBrandingInfo{kTestAndroidFacetNameAlpha3,
                         GURL(kTestAndroidFacetIconURLAlpha3)}},
  };
}

AffiliatedFacets GetTestEquivalenceClassBeta() {
  return {
      {FacetURI::FromCanonicalSpec(kTestWebFacetURIBeta1)},
      {FacetURI::FromCanonicalSpec(kTestAndroidFacetURIBeta2),
       FacetBrandingInfo{kTestAndroidFacetNameBeta2,
                         GURL(kTestAndroidFacetIconURLBeta2)}},
      {FacetURI::FromCanonicalSpec(kTestAndroidFacetURIBeta3),
       FacetBrandingInfo{kTestAndroidFacetNameBeta3,
                         GURL(kTestAndroidFacetIconURLBeta3)}},
  };
}

PasswordForm GetTestAndroidCredentials(const char* signon_realm) {
  PasswordForm form;
  form.scheme = PasswordForm::Scheme::kHtml;
  form.signon_realm = signon_realm;
  form.username_value = base::ASCIIToUTF16(kTestUsername);
  form.password_value = base::ASCIIToUTF16(kTestPassword);
  return form;
}

PasswordForm GetTestBlacklistedAndroidCredentials(const char* signon_realm) {
  PasswordForm form = GetTestAndroidCredentials(signon_realm);
  form.blocked_by_user = true;
  return form;
}

PasswordStore::FormDigest GetTestObservedWebForm(const char* signon_realm,
                                                 const char* origin) {
  return {PasswordForm::Scheme::kHtml, signon_realm,
          origin ? GURL(origin) : GURL()};
}

}  // namespace

class AffiliatedMatchHelperTest : public testing::Test {
 public:
  AffiliatedMatchHelperTest() = default;

 protected:
  void RunDeferredInitialization() {
    mock_time_task_runner_->RunUntilIdle();
    ASSERT_EQ(AffiliatedMatchHelper::kInitializationDelayOnStartup,
              mock_time_task_runner_->NextPendingTaskDelay());
    mock_time_task_runner_->FastForwardBy(
        AffiliatedMatchHelper::kInitializationDelayOnStartup);
  }

  void ExpectNoDeferredTasks() {
    mock_time_task_runner_->RunUntilIdle();
    ASSERT_FALSE(mock_time_task_runner_->HasPendingTask());
  }

  void RunUntilIdle() {
    // TODO(gab): Add support for base::RunLoop().RunUntilIdle() in scope of
    // ScopedMockTimeMessageLoopTaskRunner and use it instead of this helper
    // method.
    mock_time_task_runner_->RunUntilIdle();
  }

  void AddLogin(const PasswordForm& form) {
    password_store_->AddLogin(form);
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
    AddLogin(GetTestAndroidCredentials(kTestAndroidRealmAlpha3));
    AddLogin(GetTestAndroidCredentials(kTestAndroidRealmBeta2));
    AddLogin(GetTestBlacklistedAndroidCredentials(kTestAndroidRealmBeta3));
    AddLogin(GetTestAndroidCredentials(kTestAndroidRealmGamma));

    AddLogin(GetTestAndroidCredentials(kTestWebRealmAlpha1));
    AddLogin(GetTestAndroidCredentials(kTestWebRealmAlpha2));
  }

  void RemoveAndroidAndNonAndroidTestLogins() {
    RemoveLogin(GetTestAndroidCredentials(kTestAndroidRealmAlpha3));
    RemoveLogin(GetTestAndroidCredentials(kTestAndroidRealmBeta2));
    RemoveLogin(GetTestBlacklistedAndroidCredentials(kTestAndroidRealmBeta3));
    RemoveLogin(GetTestAndroidCredentials(kTestAndroidRealmGamma));

    RemoveLogin(GetTestAndroidCredentials(kTestWebRealmAlpha1));
    RemoveLogin(GetTestAndroidCredentials(kTestWebRealmAlpha2));
  }

  void ExpectPrefetchForAndroidTestLogins() {
    mock_affiliation_service()->ExpectCallToPrefetch(
        kTestAndroidFacetURIAlpha3);
    mock_affiliation_service()->ExpectCallToPrefetch(kTestAndroidFacetURIBeta2);
    mock_affiliation_service()->ExpectCallToPrefetch(kTestAndroidFacetURIBeta3);
    mock_affiliation_service()->ExpectCallToPrefetch(kTestAndroidFacetURIGamma);
  }

  void ExpectCancelPrefetchForAndroidTestLogins() {
    mock_affiliation_service()->ExpectCallToCancelPrefetch(
        kTestAndroidFacetURIAlpha3);
    mock_affiliation_service()->ExpectCallToCancelPrefetch(
        kTestAndroidFacetURIBeta2);
    mock_affiliation_service()->ExpectCallToCancelPrefetch(
        kTestAndroidFacetURIBeta3);
    mock_affiliation_service()->ExpectCallToCancelPrefetch(
        kTestAndroidFacetURIGamma);
  }

  void ExpectTrimCacheForAndroidTestLogins() {
    mock_affiliation_service()->ExpectCallToTrimCacheForFacetURI(
        kTestAndroidFacetURIAlpha3);
    mock_affiliation_service()->ExpectCallToTrimCacheForFacetURI(
        kTestAndroidFacetURIBeta2);
    mock_affiliation_service()->ExpectCallToTrimCacheForFacetURI(
        kTestAndroidFacetURIBeta3);
    mock_affiliation_service()->ExpectCallToTrimCacheForFacetURI(
        kTestAndroidFacetURIGamma);
  }

  std::vector<std::string> GetAffiliatedAndroidRealms(
      const PasswordStore::FormDigest& observed_form) {
    expecting_result_callback_ = true;
    match_helper()->GetAffiliatedAndroidRealms(
        observed_form,
        base::BindOnce(&AffiliatedMatchHelperTest::OnAffiliatedRealmsCallback,
                       base::Unretained(this)));
    RunUntilIdle();
    EXPECT_FALSE(expecting_result_callback_);
    return last_result_realms_;
  }

  std::vector<std::string> GetAffiliatedWebRealms(
      const PasswordStore::FormDigest& android_form) {
    expecting_result_callback_ = true;
    match_helper()->GetAffiliatedWebRealms(
        android_form,
        base::BindOnce(&AffiliatedMatchHelperTest::OnAffiliatedRealmsCallback,
                       base::Unretained(this)));
    RunUntilIdle();
    EXPECT_FALSE(expecting_result_callback_);
    return last_result_realms_;
  }

  std::vector<std::unique_ptr<PasswordForm>>
  InjectAffiliationAndBrandingInformation(
      std::vector<std::unique_ptr<PasswordForm>> forms) {
    expecting_result_callback_ = true;
    match_helper()->InjectAffiliationAndBrandingInformation(
        std::move(forms), AndroidAffiliationService::StrategyOnCacheMiss::FAIL,
        base::BindOnce(&AffiliatedMatchHelperTest::OnFormsCallback,
                       base::Unretained(this)));
    RunUntilIdle();
    EXPECT_FALSE(expecting_result_callback_);
    return std::move(last_result_forms_);
  }

  void DestroyMatchHelper() { match_helper_.reset(); }

  TestPasswordStore* password_store() { return password_store_.get(); }

  MockAndroidAffiliationService* mock_affiliation_service() {
    return mock_affiliation_service_;
  }

  AffiliatedMatchHelper* match_helper() { return match_helper_.get(); }

 private:
  void OnAffiliatedRealmsCallback(
      const std::vector<std::string>& affiliated_realms) {
    EXPECT_TRUE(expecting_result_callback_);
    expecting_result_callback_ = false;
    last_result_realms_ = affiliated_realms;
  }

  void OnFormsCallback(std::vector<std::unique_ptr<PasswordForm>> forms) {
    EXPECT_TRUE(expecting_result_callback_);
    expecting_result_callback_ = false;
    last_result_forms_.swap(forms);
  }

  // testing::Test:
  void SetUp() override {
    auto service =
        std::make_unique<testing::StrictMock<MockAndroidAffiliationService>>();
    mock_affiliation_service_ = service.get();

    password_store_->Init(nullptr);

    match_helper_ = std::make_unique<AffiliatedMatchHelper>(
        password_store_.get(), std::move(service));
  }

  void TearDown() override {
    match_helper_.reset();
    password_store_->ShutdownOnUIThread();
    password_store_ = nullptr;
    // Clean up on the background thread.
    RunUntilIdle();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::ScopedMockTimeMessageLoopTaskRunner mock_time_task_runner_;

  std::vector<std::string> last_result_realms_;
  std::vector<std::unique_ptr<PasswordForm>> last_result_forms_;
  bool expecting_result_callback_ = false;

  scoped_refptr<TestPasswordStore> password_store_ =
      base::MakeRefCounted<TestPasswordStore>();
  std::unique_ptr<AffiliatedMatchHelper> match_helper_;

  // Owned by |match_helper_|.
  MockAndroidAffiliationService* mock_affiliation_service_ = nullptr;
};

// GetAffiliatedAndroidRealm* tests verify that GetAffiliatedAndroidRealms()
// returns the realms of affiliated Android applications, but only Android
// applications, and only if the observed form is a secure HTML login form.

TEST_F(AffiliatedMatchHelperTest, GetAffiliatedAndroidRealmsYieldsResults) {
  mock_affiliation_service()
      ->ExpectCallToGetAffiliationsAndBrandingAndSucceedWithResult(
          FacetURI::FromCanonicalSpec(kTestWebFacetURIBeta1),
          StrategyOnCacheMiss::FAIL, GetTestEquivalenceClassBeta());
  EXPECT_THAT(GetAffiliatedAndroidRealms(
                  GetTestObservedWebForm(kTestWebRealmBeta1, nullptr)),
              testing::UnorderedElementsAre(kTestAndroidRealmBeta2,
                                            kTestAndroidRealmBeta3));
}

TEST_F(AffiliatedMatchHelperTest,
       GetAffiliatedAndroidRealmsYieldsOnlyAndroidApps) {
  mock_affiliation_service()
      ->ExpectCallToGetAffiliationsAndBrandingAndSucceedWithResult(
          FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha1),
          StrategyOnCacheMiss::FAIL, GetTestEquivalenceClassAlpha());
  // This verifies that |kTestWebRealmAlpha2| is not returned.
  EXPECT_THAT(GetAffiliatedAndroidRealms(
                  GetTestObservedWebForm(kTestWebRealmAlpha1, nullptr)),
              testing::UnorderedElementsAre(kTestAndroidRealmAlpha3));
}

TEST_F(AffiliatedMatchHelperTest,
       GetAffiliatedAndroidRealmsYieldsEmptyResultsForHTTPBasicAuthForms) {
  PasswordStore::FormDigest http_auth_observed_form(
      GetTestObservedWebForm(kTestWebRealmAlpha1, nullptr));
  http_auth_observed_form.scheme = PasswordForm::Scheme::kBasic;
  EXPECT_THAT(GetAffiliatedAndroidRealms(http_auth_observed_form),
              testing::IsEmpty());
}

TEST_F(AffiliatedMatchHelperTest,
       GetAffiliatedAndroidRealmsYieldsEmptyResultsForHTTPDigestAuthForms) {
  PasswordStore::FormDigest http_auth_observed_form(
      GetTestObservedWebForm(kTestWebRealmAlpha1, nullptr));
  http_auth_observed_form.scheme = PasswordForm::Scheme::kDigest;
  EXPECT_THAT(GetAffiliatedAndroidRealms(http_auth_observed_form),
              testing::IsEmpty());
}

TEST_F(AffiliatedMatchHelperTest,
       GetAffiliatedAndroidRealmsYieldsEmptyResultsForAndroidKeyedForms) {
  PasswordStore::FormDigest android_observed_form(
      GetTestAndroidCredentials(kTestAndroidRealmBeta2));
  EXPECT_THAT(GetAffiliatedAndroidRealms(android_observed_form),
              testing::IsEmpty());
}

TEST_F(AffiliatedMatchHelperTest,
       GetAffiliatedAndroidRealmsYieldsEmptyResultsWhenNoPrefetch) {
  mock_affiliation_service()
      ->ExpectCallToGetAffiliationsAndBrandingAndEmulateFailure(
          FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha1),
          StrategyOnCacheMiss::FAIL);
  EXPECT_THAT(GetAffiliatedAndroidRealms(
                  GetTestObservedWebForm(kTestWebRealmAlpha1, nullptr)),
              testing::IsEmpty());
}

// GetAffiliatedWebRealms* tests verify that GetAffiliatedWebRealms() returns
// the realms of web sites affiliated with the given Android application, but
// only web sites, and only if an Android application is queried.

TEST_F(AffiliatedMatchHelperTest, GetAffiliatedWebRealmsYieldsResults) {
  mock_affiliation_service()
      ->ExpectCallToGetAffiliationsAndBrandingAndSucceedWithResult(
          FacetURI::FromCanonicalSpec(kTestAndroidFacetURIAlpha3),
          StrategyOnCacheMiss::FETCH_OVER_NETWORK,
          GetTestEquivalenceClassAlpha());
  PasswordStore::FormDigest android_form(
      GetTestAndroidCredentials(kTestAndroidRealmAlpha3));
  EXPECT_THAT(
      GetAffiliatedWebRealms(android_form),
      testing::UnorderedElementsAre(kTestWebRealmAlpha1, kTestWebRealmAlpha2));
}

TEST_F(AffiliatedMatchHelperTest, GetAffiliatedWebRealmsYieldsOnlyWebsites) {
  mock_affiliation_service()
      ->ExpectCallToGetAffiliationsAndBrandingAndSucceedWithResult(
          FacetURI::FromCanonicalSpec(kTestAndroidFacetURIBeta2),
          StrategyOnCacheMiss::FETCH_OVER_NETWORK,
          GetTestEquivalenceClassBeta());
  PasswordStore::FormDigest android_form(
      GetTestAndroidCredentials(kTestAndroidRealmBeta2));
  // This verifies that |kTestAndroidRealmBeta3| is not returned.
  EXPECT_THAT(GetAffiliatedWebRealms(android_form),
              testing::UnorderedElementsAre(kTestWebRealmBeta1));
}

TEST_F(AffiliatedMatchHelperTest,
       GetAffiliatedWebRealmsYieldsEmptyResultsForWebKeyedForms) {
  EXPECT_THAT(GetAffiliatedWebRealms(
                  GetTestObservedWebForm(kTestWebRealmBeta1, nullptr)),
              testing::IsEmpty());
}

// Verifies that InjectAffiliationAndBrandingInformation() injects the realms of
// web sites affiliated with the given Android application into the password
// forms, as well as branding information corresponding to the application, if
// any.
TEST_F(AffiliatedMatchHelperTest, InjectAffiliationAndBrandingInformation) {
  std::vector<std::unique_ptr<PasswordForm>> forms;

  forms.push_back(std::make_unique<PasswordForm>(
      GetTestAndroidCredentials(kTestAndroidRealmAlpha3)));
  mock_affiliation_service()
      ->ExpectCallToGetAffiliationsAndBrandingAndSucceedWithResult(
          FacetURI::FromCanonicalSpec(kTestAndroidFacetURIAlpha3),
          StrategyOnCacheMiss::FAIL, GetTestEquivalenceClassAlpha());

  forms.push_back(std::make_unique<PasswordForm>(
      GetTestAndroidCredentials(kTestAndroidRealmBeta2)));
  mock_affiliation_service()
      ->ExpectCallToGetAffiliationsAndBrandingAndSucceedWithResult(
          FacetURI::FromCanonicalSpec(kTestAndroidFacetURIBeta2),
          StrategyOnCacheMiss::FAIL, GetTestEquivalenceClassBeta());

  forms.push_back(std::make_unique<PasswordForm>(
      GetTestAndroidCredentials(kTestAndroidRealmBeta3)));
  mock_affiliation_service()
      ->ExpectCallToGetAffiliationsAndBrandingAndSucceedWithResult(
          FacetURI::FromCanonicalSpec(kTestAndroidFacetURIBeta3),
          StrategyOnCacheMiss::FAIL, GetTestEquivalenceClassBeta());

  forms.push_back(std::make_unique<PasswordForm>(
      GetTestAndroidCredentials(kTestAndroidRealmGamma)));
  mock_affiliation_service()
      ->ExpectCallToGetAffiliationsAndBrandingAndEmulateFailure(
          FacetURI::FromCanonicalSpec(kTestAndroidFacetURIGamma),
          StrategyOnCacheMiss::FAIL);

  PasswordStore::FormDigest digest =
      GetTestObservedWebForm(kTestWebRealmBeta1, nullptr);
  PasswordForm web_form;
  web_form.scheme = digest.scheme;
  web_form.signon_realm = digest.signon_realm;
  web_form.url = digest.url;
  forms.push_back(std::make_unique<PasswordForm>(web_form));

  size_t expected_form_count = forms.size();
  std::vector<std::unique_ptr<PasswordForm>> results(
      InjectAffiliationAndBrandingInformation(std::move(forms)));
  ASSERT_EQ(expected_form_count, results.size());
  EXPECT_THAT(results[0]->affiliated_web_realm,
              testing::AnyOf(kTestWebRealmAlpha1, kTestWebRealmAlpha2));
  EXPECT_EQ(kTestAndroidFacetNameAlpha3, results[0]->app_display_name);
  EXPECT_EQ(kTestAndroidFacetIconURLAlpha3,
            results[0]->app_icon_url.possibly_invalid_spec());
  EXPECT_THAT(results[1]->affiliated_web_realm,
              testing::Eq(kTestWebRealmBeta1));
  EXPECT_EQ(kTestAndroidFacetNameBeta2, results[1]->app_display_name);
  EXPECT_EQ(kTestAndroidFacetIconURLBeta2,
            results[1]->app_icon_url.possibly_invalid_spec());
  EXPECT_THAT(results[2]->affiliated_web_realm,
              testing::Eq(kTestWebRealmBeta1));
  EXPECT_EQ(kTestAndroidFacetNameBeta3, results[2]->app_display_name);
  EXPECT_EQ(kTestAndroidFacetIconURLBeta3,
            results[2]->app_icon_url.possibly_invalid_spec());
  EXPECT_THAT(results[3]->affiliated_web_realm, testing::IsEmpty());
  EXPECT_THAT(results[4]->affiliated_web_realm, testing::IsEmpty());
}

// Note: IsValidWebCredential() is tested as part of GetAffiliatedAndroidRealms
// tests above.
TEST_F(AffiliatedMatchHelperTest, IsValidAndroidCredential) {
  EXPECT_FALSE(AffiliatedMatchHelper::IsValidAndroidCredential(
      GetTestObservedWebForm(kTestWebRealmBeta1, nullptr)));
  PasswordStore::FormDigest android_credential(
      GetTestAndroidCredentials(kTestAndroidRealmBeta2));
  EXPECT_TRUE(
      AffiliatedMatchHelper::IsValidAndroidCredential(android_credential));
}

// Verifies that affiliations for Android applications with pre-existing
// credentials on start-up are prefetched.
TEST_F(
    AffiliatedMatchHelperTest,
    PrefetchAffiliationsAndBrandingForPreexistingAndroidCredentialsOnStartup) {
  AddAndroidAndNonAndroidTestLogins();

  match_helper()->Initialize();
  RunUntilIdle();

  ExpectPrefetchForAndroidTestLogins();
  ASSERT_NO_FATAL_FAILURE(RunDeferredInitialization());
}

// Stores credentials for Android applications between Initialize() and
// DoDeferredInitialization(). Verifies that corresponding affiliation
// information gets prefetched.
TEST_F(AffiliatedMatchHelperTest,
       PrefetchAffiliationsForAndroidCredentialsAddedInInitializationDelay) {
  match_helper()->Initialize();
  RunUntilIdle();

  AddAndroidAndNonAndroidTestLogins();

  ExpectPrefetchForAndroidTestLogins();
  ASSERT_NO_FATAL_FAILURE(RunDeferredInitialization());
}

// Stores credentials for Android applications after DoDeferredInitialization().
// Verifies that corresponding affiliation information gets prefetched.
TEST_F(AffiliatedMatchHelperTest,
       PrefetchAffiliationsForAndroidCredentialsAddedAfterInitialization) {
  match_helper()->Initialize();
  ASSERT_NO_FATAL_FAILURE(RunDeferredInitialization());

  ExpectPrefetchForAndroidTestLogins();
  AddAndroidAndNonAndroidTestLogins();
}

TEST_F(AffiliatedMatchHelperTest,
       CancelPrefetchingAffiliationsAndBrandingForRemovedAndroidCredentials) {
  AddAndroidAndNonAndroidTestLogins();
  match_helper()->Initialize();
  ExpectPrefetchForAndroidTestLogins();
  ASSERT_NO_FATAL_FAILURE(RunDeferredInitialization());

  ExpectCancelPrefetchForAndroidTestLogins();
  ExpectTrimCacheForAndroidTestLogins();
  RemoveAndroidAndNonAndroidTestLogins();
}

// Verify that whenever the primary key is updated for a credential (in which
// case both REMOVE and ADD change notifications are sent out), then Prefetch()
// is called in response to the addition before the call to
// TrimCacheForFacetURI() in response to the removal, so that cached data is not
// deleted and then immediately re-fetched.
TEST_F(AffiliatedMatchHelperTest, PrefetchBeforeTrimForPrimaryKeyUpdates) {
  AddAndroidAndNonAndroidTestLogins();
  match_helper()->Initialize();
  ExpectPrefetchForAndroidTestLogins();
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
  new_form.username_value = base::ASCIIToUTF16("NewUserName");
  UpdateLoginWithPrimaryKey(new_form, old_form);
}

// Stores and removes four credentials for the same an Android application, and
// expects that Prefetch() and CancelPrefetch() will each be called four times.
TEST_F(AffiliatedMatchHelperTest,
       DuplicateCredentialsArePrefetchWithMultiplicity) {
  EXPECT_CALL(*mock_affiliation_service(),
              Prefetch(FacetURI::FromCanonicalSpec(kTestAndroidFacetURIAlpha3),
                       base::Time::Max()))
      .Times(4);

  PasswordForm android_form(GetTestAndroidCredentials(kTestAndroidRealmAlpha3));
  AddLogin(android_form);

  // Store two credentials before initialization.
  PasswordForm android_form2(android_form);
  android_form2.username_value = base::ASCIIToUTF16("JohnDoe2");
  AddLogin(android_form2);

  match_helper()->Initialize();
  RunUntilIdle();

  // Store one credential between initialization and deferred initialization.
  PasswordForm android_form3(android_form);
  android_form3.username_value = base::ASCIIToUTF16("JohnDoe3");
  AddLogin(android_form3);

  ASSERT_NO_FATAL_FAILURE(RunDeferredInitialization());

  // Store one credential after deferred initialization.
  PasswordForm android_form4(android_form);
  android_form4.username_value = base::ASCIIToUTF16("JohnDoe4");
  AddLogin(android_form4);

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

TEST_F(AffiliatedMatchHelperTest, DestroyBeforeDeferredInitialization) {
  match_helper()->Initialize();
  RunUntilIdle();
  DestroyMatchHelper();
  ASSERT_NO_FATAL_FAILURE(ExpectNoDeferredTasks());
}

}  // namespace password_manager
