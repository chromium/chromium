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
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/affiliations/core/browser/mock_affiliation_service.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

using ::affiliations::AffiliatedFacets;
using ::affiliations::Facet;
using ::affiliations::FacetBrandingInfo;
using ::affiliations::FacetURI;
using ::affiliations::GroupedFacets;
using ::affiliations::MockAffiliationService;
using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;
using StrategyOnCacheMiss =
    ::affiliations::AffiliationService::StrategyOnCacheMiss;

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
      Facet(FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha1)),
      Facet(FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha2)),
      Facet(FacetURI::FromCanonicalSpec(kTestAndroidFacetURIAlpha3),
            FacetBrandingInfo{kTestAndroidFacetNameAlpha3,
                              GURL(kTestAndroidFacetIconURLAlpha3)}),
  };
}

#if !BUILDFLAG(IS_ANDROID)
std::vector<GroupedFacets> GetTestEquivalenceGroupClassAlpha() {
  std::vector<Facet> facets = {
      Facet(FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha1)),
      Facet(FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha2)),
      Facet(FacetURI::FromCanonicalSpec(kTestAndroidFacetURIAlpha3))};

  GroupedFacets result_group;
  result_group.facets = facets;
  result_group.branding_info = FacetBrandingInfo{
      kTestAndroidFacetNameAlpha3, GURL(kTestAndroidFacetIconURLAlpha3)};
  return {result_group};
}
#endif

AffiliatedFacets GetTestEquivalenceClassBeta() {
  return {
      Facet(FacetURI::FromCanonicalSpec(kTestWebFacetURIBeta1)),
      Facet(FacetURI::FromCanonicalSpec(kTestAndroidFacetURIBeta2),
            FacetBrandingInfo{kTestAndroidFacetNameBeta2,
                              GURL(kTestAndroidFacetIconURLBeta2)}),
      Facet(FacetURI::FromCanonicalSpec(kTestAndroidFacetURIBeta3),
            FacetBrandingInfo{kTestAndroidFacetNameBeta3,
                              GURL(kTestAndroidFacetIconURLBeta3)}),
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

class AffiliatedMatchHelperTest : public testing::Test {
 public:
  AffiliatedMatchHelperTest() = default;

 protected:
  void RunUntilIdle() {
    // TODO(gab): Add support for base::RunLoop().RunUntilIdle() in scope of
    // ScopedMockTimeMessageLoopTaskRunner and use it instead of this helper
    // method.
    mock_time_task_runner_->RunUntilIdle();
  }

  MockAffiliationService* mock_affiliation_service() {
    return mock_affiliation_service_.get();
  }

  AffiliatedMatchHelper* match_helper() { return match_helper_.get(); }

 private:
  // testing::Test:
  void SetUp() override {
    mock_affiliation_service_ =
        std::make_unique<testing::StrictMock<MockAffiliationService>>();
    match_helper_ =
        std::make_unique<AffiliatedMatchHelper>(mock_affiliation_service());
  }

  void TearDown() override {
    match_helper_.reset();
    mock_affiliation_service_.reset();
    // Clean up on the background thread.
    RunUntilIdle();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::ScopedMockTimeMessageLoopTaskRunner mock_time_task_runner_;

  std::unique_ptr<MockAffiliationService> mock_affiliation_service_;
  std::unique_ptr<AffiliatedMatchHelper> match_helper_;
};

// GetAffiliatedAndroidRealm* tests verify that
// GetAffiliatedAndGroupedRealms() returns the realms of affiliated Android
// applications and web sites, but only if the observed form is a secure HTML
// login form.

TEST_F(AffiliatedMatchHelperTest, GetAffiliatedAndroidRealms) {
  GroupedFacets result_grouped_facet;
  result_grouped_facet.facets.emplace_back(
      FacetURI::FromCanonicalSpec(kTestWebFacetURIBeta1));
  EXPECT_CALL(*mock_affiliation_service(),
              GetAffiliationsAndBranding(
                  FacetURI::FromCanonicalSpec(kTestWebFacetURIBeta1),
                  StrategyOnCacheMiss::FAIL, _))
      .WillOnce(RunOnceCallback<2>(GetTestEquivalenceClassBeta(), true));
  EXPECT_CALL(*mock_affiliation_service(),
              GetGroupingInfo(testing::ElementsAre(FacetURI::FromCanonicalSpec(
                                  kTestWebFacetURIBeta1)),
                              _))
      .WillOnce(
          RunOnceCallback<1>(std::vector<GroupedFacets>{result_grouped_facet}));

  base::MockCallback<AffiliatedMatchHelper::AffiliatedRealmsCallback> callback;
  EXPECT_CALL(callback, Run(UnorderedElementsAre(kTestAndroidRealmBeta2,
                                                 kTestAndroidRealmBeta3),
                            IsEmpty()));

  match_helper()->GetAffiliatedAndGroupedRealms(
      GetTestObservedWebForm(kTestWebRealmBeta1, nullptr), callback.Get());
}

TEST_F(AffiliatedMatchHelperTest, GetAffiliatedAndroidRealmsAndWebsites) {
  GroupedFacets result_grouped_facet;
  result_grouped_facet.facets.emplace_back(
      FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha1));
  EXPECT_CALL(*mock_affiliation_service(),
              GetAffiliationsAndBranding(
                  FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha1),
                  StrategyOnCacheMiss::FAIL, _))
      .WillOnce(RunOnceCallback<2>(GetTestEquivalenceClassAlpha(), true));
  EXPECT_CALL(*mock_affiliation_service(),
              GetGroupingInfo(testing::ElementsAre(FacetURI::FromCanonicalSpec(
                                  kTestWebFacetURIAlpha1)),
                              _))
      .WillOnce(
          RunOnceCallback<1>(std::vector<GroupedFacets>{result_grouped_facet}));

  base::MockCallback<AffiliatedMatchHelper::AffiliatedRealmsCallback> callback;
  // Android doesn't support filling across affiliated websites.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(callback,
              Run(UnorderedElementsAre(kTestAndroidRealmAlpha3), IsEmpty()));
#else
  EXPECT_CALL(callback, Run(UnorderedElementsAre(kTestWebRealmAlpha2,
                                                 kTestAndroidRealmAlpha3),
                            IsEmpty()));
#endif
  match_helper()->GetAffiliatedAndGroupedRealms(
      GetTestObservedWebForm(kTestWebRealmAlpha1, nullptr), callback.Get());
}

TEST_F(AffiliatedMatchHelperTest,
       GetAffiliatedAndroidRealmsYieldsEmptyResultsForHTTPBasicAuthForms) {
  PasswordFormDigest http_auth_observed_form(
      GetTestObservedWebForm(kTestWebRealmAlpha1, nullptr));
  http_auth_observed_form.scheme = PasswordForm::Scheme::kBasic;

  EXPECT_CALL(*mock_affiliation_service(), GetAffiliationsAndBranding).Times(0);
  base::MockCallback<AffiliatedMatchHelper::AffiliatedRealmsCallback> callback;
  EXPECT_CALL(callback, Run(IsEmpty(), IsEmpty()));

  match_helper()->GetAffiliatedAndGroupedRealms(http_auth_observed_form,
                                                callback.Get());
}

TEST_F(AffiliatedMatchHelperTest,
       GetAffiliatedAndroidRealmsYieldsEmptyResultsForHTTPDigestAuthForms) {
  PasswordFormDigest http_auth_observed_form(
      GetTestObservedWebForm(kTestWebRealmAlpha1, nullptr));
  http_auth_observed_form.scheme = PasswordForm::Scheme::kDigest;

  EXPECT_CALL(*mock_affiliation_service(), GetAffiliationsAndBranding).Times(0);
  base::MockCallback<AffiliatedMatchHelper::AffiliatedRealmsCallback> callback;
  EXPECT_CALL(callback, Run(IsEmpty(), IsEmpty()));

  match_helper()->GetAffiliatedAndGroupedRealms(http_auth_observed_form,
                                                callback.Get());
}

TEST_F(AffiliatedMatchHelperTest,
       GetAffiliatedAndroidRealmsYieldsEmptyResultsForAndroidKeyedForms) {
  PasswordFormDigest android_observed_form(
      GetTestAndroidCredentials(kTestAndroidRealmBeta2));

  EXPECT_CALL(*mock_affiliation_service(), GetAffiliationsAndBranding).Times(0);
  base::MockCallback<AffiliatedMatchHelper::AffiliatedRealmsCallback> callback;
  EXPECT_CALL(callback, Run(IsEmpty(), IsEmpty()));

  match_helper()->GetAffiliatedAndGroupedRealms(android_observed_form,
                                                callback.Get());
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(AffiliatedMatchHelperTest, GetGroupedRealms) {
  EXPECT_CALL(*mock_affiliation_service(), GetAffiliationsAndBranding)
      .WillOnce(RunOnceCallback<2>(AffiliatedFacets(), true));
  EXPECT_CALL(*mock_affiliation_service(),
              GetGroupingInfo(testing::ElementsAre(FacetURI::FromCanonicalSpec(
                                  kTestWebFacetURIAlpha1)),
                              _))
      .WillOnce(RunOnceCallback<1>(GetTestEquivalenceGroupClassAlpha()));

  base::MockCallback<AffiliatedMatchHelper::AffiliatedRealmsCallback> callback;
  EXPECT_CALL(callback,
              Run(IsEmpty(), UnorderedElementsAre(kTestWebRealmAlpha2,
                                                  kTestAndroidRealmAlpha3)));
  match_helper()->GetAffiliatedAndGroupedRealms(
      GetTestObservedWebForm(kTestWebRealmAlpha1, nullptr), callback.Get());
}

TEST_F(AffiliatedMatchHelperTest, GetGroupedAndAffiliatedRealms) {
  EXPECT_CALL(*mock_affiliation_service(), GetAffiliationsAndBranding)
      .WillOnce(RunOnceCallback<2>(GetTestEquivalenceClassAlpha(), true));
  EXPECT_CALL(*mock_affiliation_service(), GetGroupingInfo)
      .WillOnce(RunOnceCallback<1>(GetTestEquivalenceGroupClassAlpha()));

  base::MockCallback<AffiliatedMatchHelper::AffiliatedRealmsCallback> callback;
  EXPECT_CALL(
      callback,
      Run(UnorderedElementsAre(kTestWebRealmAlpha2, kTestAndroidRealmAlpha3),
          UnorderedElementsAre(kTestWebRealmAlpha2, kTestAndroidRealmAlpha3)));
  match_helper()->GetAffiliatedAndGroupedRealms(
      GetTestObservedWebForm(kTestWebRealmAlpha1, nullptr), callback.Get());
}

TEST_F(AffiliatedMatchHelperTest, GetGroupedRealmsWhenNoMatch) {
  GroupedFacets result_grouped_facet;
  result_grouped_facet.facets.emplace_back(
      FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha1));

  EXPECT_CALL(*mock_affiliation_service(), GetAffiliationsAndBranding)
      .WillOnce(RunOnceCallback<2>(AffiliatedFacets(), true));
  EXPECT_CALL(*mock_affiliation_service(), GetGroupingInfo)
      .WillOnce(
          RunOnceCallback<1>(std::vector<GroupedFacets>{result_grouped_facet}));

  base::MockCallback<AffiliatedMatchHelper::AffiliatedRealmsCallback> callback;
  EXPECT_CALL(callback, Run(IsEmpty(), IsEmpty()));
  match_helper()->GetAffiliatedAndGroupedRealms(
      GetTestObservedWebForm(kTestWebRealmAlpha1, nullptr), callback.Get());
}
#endif

TEST_F(AffiliatedMatchHelperTest, InjectAffiliationAndBrandingInformation) {
  std::vector<PasswordForm> forms;
  forms.push_back(GetTestAndroidCredentials(kTestAndroidRealmAlpha3));
  forms.push_back(GetTestAndroidCredentials(kTestAndroidRealmBeta2));
  forms.push_back(GetTestAndroidCredentials(kTestAndroidRealmGamma));

  PasswordFormDigest digest = {PasswordForm::Scheme::kHtml, kTestWebRealmBeta1,
                               GURL()};
  PasswordForm web_form;
  web_form.scheme = digest.scheme;
  web_form.signon_realm = digest.signon_realm;
  web_form.url = digest.url;
  forms.push_back(web_form);

  size_t expected_form_count = forms.size();

  EXPECT_CALL(
      *mock_affiliation_service(),
      GetAffiliationsAndBranding(
          FacetURI::FromCanonicalSpec(kTestAndroidFacetURIAlpha3), _, _))
      .WillOnce(RunOnceCallback<2>(GetTestEquivalenceClassAlpha(), true));

  EXPECT_CALL(*mock_affiliation_service(),
              GetAffiliationsAndBranding(
                  FacetURI::FromCanonicalSpec(kTestAndroidFacetURIBeta2), _, _))
      .WillOnce(RunOnceCallback<2>(GetTestEquivalenceClassBeta(), true));

  EXPECT_CALL(*mock_affiliation_service(),
              GetAffiliationsAndBranding(
                  FacetURI::FromCanonicalSpec(kTestAndroidFacetURIGamma), _, _))
      .WillOnce(RunOnceCallback<2>(AffiliatedFacets(), false));

  LoginsResultOrError result;
  base::MockCallback<base::OnceCallback<void(LoginsResultOrError)>> mock_reply;
  EXPECT_CALL(mock_reply, Run).WillOnce(testing::SaveArg<0>(&result));
  match_helper()->InjectAffiliationAndBrandingInformation(std::move(forms),
                                                          mock_reply.Get());

  auto result_forms = std::move(absl::get<std::vector<PasswordForm>>(result));

  ASSERT_EQ(expected_form_count, result_forms.size());
  EXPECT_THAT(result_forms[0].affiliated_web_realm,
              testing::AnyOf(kTestWebRealmAlpha1, kTestWebRealmAlpha2));
  EXPECT_EQ(kTestAndroidFacetNameAlpha3, result_forms[0].app_display_name);
  EXPECT_EQ(kTestAndroidFacetIconURLAlpha3,
            result_forms[0].app_icon_url.possibly_invalid_spec());

  EXPECT_THAT(result_forms[1].affiliated_web_realm,
              testing::Eq(kTestWebRealmBeta1));
  EXPECT_EQ(kTestAndroidFacetNameBeta2, result_forms[1].app_display_name);
  EXPECT_EQ(kTestAndroidFacetIconURLBeta2,
            result_forms[1].app_icon_url.possibly_invalid_spec());

  EXPECT_THAT(result_forms[2].affiliated_web_realm, IsEmpty());
}

TEST_F(AffiliatedMatchHelperTest, GetPSLExtensions) {
  base::MockCallback<AffiliatedMatchHelper::PSLExtensionCallback>
      result_callback;
  base::OnceCallback<void(std::vector<std::string>)> extensions_callback;

  EXPECT_CALL(*mock_affiliation_service(), GetPSLExtensions)
      .WillOnce(MoveArg<0>(&extensions_callback));
  EXPECT_TRUE(extensions_callback.is_null());

  // Callback isn't called immediately.
  EXPECT_CALL(result_callback, Run).Times(0);
  match_helper()->GetPSLExtensions(result_callback.Get());

  EXPECT_FALSE(extensions_callback.is_null());

  std::vector<std::string> pls_extensions = {"a.com", "b.com"};
  EXPECT_CALL(result_callback,
              Run(testing::UnorderedElementsAreArray(pls_extensions)));
  std::move(extensions_callback).Run(pls_extensions);
}

TEST_F(AffiliatedMatchHelperTest, GetPSLExtensionsCachesResult) {
  base::MockCallback<AffiliatedMatchHelper::PSLExtensionCallback>
      result_callback;
  std::vector<std::string> pls_extensions = {"a.com", "b.com"};
  EXPECT_CALL(*mock_affiliation_service(), GetPSLExtensions)
      .WillOnce(RunOnceCallback<0>(pls_extensions));
  EXPECT_CALL(result_callback,
              Run(testing::UnorderedElementsAreArray(pls_extensions)));

  match_helper()->GetPSLExtensions(result_callback.Get());

  testing::Mock::VerifyAndClearExpectations(mock_affiliation_service());
  // Now affiliation service isn't called.
  EXPECT_CALL(*mock_affiliation_service(), GetPSLExtensions).Times(0);
  EXPECT_CALL(result_callback,
              Run(testing::UnorderedElementsAreArray(pls_extensions)));
  match_helper()->GetPSLExtensions(result_callback.Get());
}

}  // namespace password_manager
