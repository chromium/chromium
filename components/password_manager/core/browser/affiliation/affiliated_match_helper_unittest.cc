// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/affiliation/affiliated_match_helper.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "components/password_manager/core/browser/affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/affiliation/mock_affiliation_service.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

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
    "android://hash@com.example.gamma.android/";

const char16_t kTestUsername[] = u"JohnDoe";
const char16_t kTestPassword[] = u"secret";

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
  form.username_value = kTestUsername;
  form.password_value = kTestPassword;
  return form;
}

PasswordFormDigest GetTestObservedWebForm(const char* signon_realm,
                                          const char* origin) {
  return {PasswordForm::Scheme::kHtml, signon_realm,
          origin ? GURL(origin) : GURL()};
}

}  // namespace

class AffiliatedMatchHelperTest : public testing::Test,
                                  public ::testing::WithParamInterface<bool> {
 public:
  AffiliatedMatchHelperTest() {
    feature_list_.InitWithFeatureState(
        features::kFillingAcrossAffiliatedWebsites, GetParam());
  }

 protected:
  void RunUntilIdle() {
    // TODO(gab): Add support for base::RunLoop().RunUntilIdle() in scope of
    // ScopedMockTimeMessageLoopTaskRunner and use it instead of this helper
    // method.
    mock_time_task_runner_->RunUntilIdle();
  }

  std::vector<std::string> GetAffiliatedAndroidAndWebRealms(
      const PasswordFormDigest& observed_form) {
    expecting_result_callback_ = true;
    match_helper()->GetAffiliatedAndroidAndWebRealms(
        observed_form,
        base::BindOnce(&AffiliatedMatchHelperTest::OnAffiliatedRealmsCallback,
                       base::Unretained(this)));
    RunUntilIdle();
    EXPECT_FALSE(expecting_result_callback_);
    return last_result_realms_;
  }

  OverloadedMockAffiliationService* mock_affiliation_service() {
    return mock_affiliation_service_.get();
  }

  AffiliatedMatchHelper* match_helper() { return match_helper_.get(); }

 private:
  void OnAffiliatedRealmsCallback(
      const std::vector<std::string>& affiliated_realms) {
    EXPECT_TRUE(expecting_result_callback_);
    expecting_result_callback_ = false;
    last_result_realms_ = affiliated_realms;
  }

  // testing::Test:
  void SetUp() override {
    mock_affiliation_service_ = std::make_unique<
        testing::StrictMock<OverloadedMockAffiliationService>>();
    match_helper_ =
        std::make_unique<AffiliatedMatchHelper>(mock_affiliation_service());
  }

  void TearDown() override {
    mock_affiliation_service_.reset();
    // Clean up on the background thread.
    RunUntilIdle();
  }

  base::test::ScopedFeatureList feature_list_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::ScopedMockTimeMessageLoopTaskRunner mock_time_task_runner_;

  std::vector<std::string> last_result_realms_;
  bool expecting_result_callback_ = false;

  std::unique_ptr<AffiliatedMatchHelper> match_helper_;

  std::unique_ptr<OverloadedMockAffiliationService> mock_affiliation_service_;
};

// GetAffiliatedAndroidRealm* tests verify that
// GetAffiliatedAndroidAndWebRealms() returns the realms of affiliated Android
// applications and web sites, but only if the observed form is a secure HTML
// login form.

TEST_P(AffiliatedMatchHelperTest, GetAffiliatedAndroidRealmsYieldsResults) {
  mock_affiliation_service()
      ->ExpectCallToGetAffiliationsAndBrandingAndSucceedWithResult(
          FacetURI::FromCanonicalSpec(kTestWebFacetURIBeta1),
          StrategyOnCacheMiss::FAIL, GetTestEquivalenceClassBeta());
  EXPECT_THAT(GetAffiliatedAndroidAndWebRealms(
                  GetTestObservedWebForm(kTestWebRealmBeta1, nullptr)),
              testing::UnorderedElementsAre(kTestAndroidRealmBeta2,
                                            kTestAndroidRealmBeta3));
}

TEST_P(AffiliatedMatchHelperTest,
       GetAffiliatedAndroidRealmsYieldsOnlyAndroidApps) {
  // Disable this test when filling across affiliated websites enabled.
  if (base::FeatureList::IsEnabled(features::kFillingAcrossAffiliatedWebsites))
    return;
  mock_affiliation_service()
      ->ExpectCallToGetAffiliationsAndBrandingAndSucceedWithResult(
          FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha1),
          StrategyOnCacheMiss::FAIL, GetTestEquivalenceClassAlpha());
  // This verifies that |kTestWebRealmAlpha2| is not returned.
  EXPECT_THAT(GetAffiliatedAndroidAndWebRealms(
                  GetTestObservedWebForm(kTestWebRealmAlpha1, nullptr)),
              testing::UnorderedElementsAre(kTestAndroidRealmAlpha3));
}

TEST_P(AffiliatedMatchHelperTest,
       GetAffiliatedAndroidRealmsYieldsEmptyResultsForHTTPBasicAuthForms) {
  PasswordFormDigest http_auth_observed_form(
      GetTestObservedWebForm(kTestWebRealmAlpha1, nullptr));
  http_auth_observed_form.scheme = PasswordForm::Scheme::kBasic;
  EXPECT_THAT(GetAffiliatedAndroidAndWebRealms(http_auth_observed_form),
              testing::IsEmpty());
}

TEST_P(AffiliatedMatchHelperTest,
       GetAffiliatedAndroidRealmsYieldsEmptyResultsForHTTPDigestAuthForms) {
  PasswordFormDigest http_auth_observed_form(
      GetTestObservedWebForm(kTestWebRealmAlpha1, nullptr));
  http_auth_observed_form.scheme = PasswordForm::Scheme::kDigest;
  EXPECT_THAT(GetAffiliatedAndroidAndWebRealms(http_auth_observed_form),
              testing::IsEmpty());
}

TEST_P(AffiliatedMatchHelperTest,
       GetAffiliatedAndroidRealmsYieldsEmptyResultsForAndroidKeyedForms) {
  PasswordFormDigest android_observed_form(
      GetTestAndroidCredentials(kTestAndroidRealmBeta2));
  EXPECT_THAT(GetAffiliatedAndroidAndWebRealms(android_observed_form),
              testing::IsEmpty());
}

TEST_P(AffiliatedMatchHelperTest,
       GetAffiliatedAndroidRealmsYieldsEmptyResultsWhenNoPrefetch) {
  mock_affiliation_service()
      ->ExpectCallToGetAffiliationsAndBrandingAndEmulateFailure(
          FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha1),
          StrategyOnCacheMiss::FAIL);
  EXPECT_THAT(GetAffiliatedAndroidAndWebRealms(
                  GetTestObservedWebForm(kTestWebRealmAlpha1, nullptr)),
              testing::IsEmpty());
}

TEST_P(AffiliatedMatchHelperTest, GetAffiliatedAndroidRealmsAndWebsites) {
  // Disable this test when filling across affiliated websites disabled.
  if (!base::FeatureList::IsEnabled(features::kFillingAcrossAffiliatedWebsites))
    return;
  mock_affiliation_service()
      ->ExpectCallToGetAffiliationsAndBrandingAndSucceedWithResult(
          FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha1),
          StrategyOnCacheMiss::FAIL, GetTestEquivalenceClassAlpha());
  // This verifies that |kTestWebRealmAlpha2| is returned.
  EXPECT_THAT(GetAffiliatedAndroidAndWebRealms(
                  GetTestObservedWebForm(kTestWebRealmAlpha1, nullptr)),
              testing::UnorderedElementsAre(kTestWebRealmAlpha2,
                                            kTestAndroidRealmAlpha3));
}

TEST_P(AffiliatedMatchHelperTest, InjectAffiliationAndBrandingInformation) {
  std::vector<std::unique_ptr<PasswordForm>> forms;
  forms.push_back(std::make_unique<PasswordForm>(
      GetTestAndroidCredentials(kTestAndroidRealmAlpha3)));
  forms.push_back(std::make_unique<PasswordForm>(
      GetTestAndroidCredentials(kTestAndroidRealmBeta2)));
  forms.push_back(std::make_unique<PasswordForm>(
      GetTestAndroidCredentials(kTestAndroidRealmGamma)));

  PasswordFormDigest digest = {PasswordForm::Scheme::kHtml, kTestWebRealmBeta1,
                               GURL()};
  PasswordForm web_form;
  web_form.scheme = digest.scheme;
  web_form.signon_realm = digest.signon_realm;
  web_form.url = digest.url;
  forms.push_back(std::make_unique<PasswordForm>(web_form));

  size_t expected_form_count = forms.size();

  mock_affiliation_service()
      ->ExpectCallToGetAffiliationsAndBrandingAndSucceedWithResult(
          FacetURI::FromCanonicalSpec(kTestAndroidFacetURIAlpha3),
          StrategyOnCacheMiss::FAIL, GetTestEquivalenceClassAlpha());
  mock_affiliation_service()
      ->ExpectCallToGetAffiliationsAndBrandingAndSucceedWithResult(
          FacetURI::FromCanonicalSpec(kTestAndroidFacetURIBeta2),
          StrategyOnCacheMiss::FAIL, GetTestEquivalenceClassBeta());
  mock_affiliation_service()
      ->ExpectCallToGetAffiliationsAndBrandingAndSucceedWithResult(
          FacetURI::FromCanonicalSpec(kTestAndroidFacetURIGamma),
          StrategyOnCacheMiss::FAIL, {});

  absl::variant<std::vector<std::unique_ptr<PasswordForm>>,
                PasswordStoreBackendError>
      result;
  base::MockCallback<AffiliatedMatchHelper::PasswordFormsOrErrorCallback>
      mock_reply;
  EXPECT_CALL(mock_reply, Run).WillOnce(MoveArg(&result));
  match_helper()->InjectAffiliationAndBrandingInformation(std::move(forms),
                                                          mock_reply.Get());

  auto result_forms =
      std::move(absl::get<std::vector<std::unique_ptr<PasswordForm>>>(result));

  ASSERT_EQ(expected_form_count, result_forms.size());
  EXPECT_THAT(result_forms[0]->affiliated_web_realm,
              testing::AnyOf(kTestWebRealmAlpha1, kTestWebRealmAlpha2));
  EXPECT_EQ(kTestAndroidFacetNameAlpha3, result_forms[0]->app_display_name);
  EXPECT_EQ(kTestAndroidFacetIconURLAlpha3,
            result_forms[0]->app_icon_url.possibly_invalid_spec());

  EXPECT_THAT(result_forms[1]->affiliated_web_realm,
              testing::Eq(kTestWebRealmBeta1));
  EXPECT_EQ(kTestAndroidFacetNameBeta2, result_forms[1]->app_display_name);
  EXPECT_EQ(kTestAndroidFacetIconURLBeta2,
            result_forms[1]->app_icon_url.possibly_invalid_spec());

  EXPECT_THAT(result_forms[2]->affiliated_web_realm, testing::IsEmpty());
}

INSTANTIATE_TEST_SUITE_P(FillingAcrossAffiliatedWebsites,
                         AffiliatedMatchHelperTest,
                         ::testing::Bool());

}  // namespace password_manager
