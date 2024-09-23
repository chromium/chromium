// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/get_logins_with_affiliations_request_handler.h"

#include <string_view>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/affiliations/core/browser/mock_affiliation_service.h"
#include "components/password_manager/core/browser/affiliation/affiliated_match_helper.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/fake_password_store_backend.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_consumer.h"
#include "components/password_manager/core/browser/password_store/password_store_backend_error.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

using ::affiliations::Facet;
using ::affiliations::FacetURI;
using ::affiliations::GroupedFacets;
using ::affiliations::MockAffiliationService;
using ::base::test::RunOnceCallback;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAreArray;
using ::testing::VariantWith;

constexpr const char kTestWebURL[] = "https://example.com/";
constexpr const char kTestPSLURL[] = "https://one.example.com/";
constexpr const char kAffiliatedWebURL[] = "https://noneexample.com/";
constexpr const char kAffiliatedAndroidApp[] =
    "android://"
    "5Z0D_o6B8BqileZyWhXmqO_wkO8uO0etCEXvMn5tUzEqkWUgfTSjMcTM7eMMTY_"
    "FGJC9RlpRNt_8Qp5tgDocXw==@com.bambuna.podcastaddict/";
#if !BUILDFLAG(IS_ANDROID)
constexpr const char kGroupWebURL[] = "https://noneexample2.com/";
#endif

PasswordFormDigest CreateFormDigest(const std::string& url_string) {
  return {PasswordForm::Scheme::kHtml, url_string, GURL(url_string)};
}

// Creates a form.
PasswordForm CreateForm(const std::string& url_string,
                        std::u16string_view username,
                        std::u16string_view password) {
  PasswordForm form;
  form.username_value = std::u16string(username);
  form.password_value = std::u16string(password);
  form.url = GURL(url_string);
  form.signon_realm = url_string;
  form.in_store = PasswordForm::Store::kProfileStore;
  return form;
}

}  // namespace

class GetLoginsWithAffiliationsRequestHandlerTest : public testing::Test {
 public:
  PasswordStoreBackend* backend() { return &backend_; }

  MockAffiliationService& affiliation_service() {
    return mock_affiliation_service_;
  }
  AffiliatedMatchHelper& match_helper() { return match_helper_; }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  FakePasswordStoreBackend backend_;
  testing::StrictMock<MockAffiliationService> mock_affiliation_service_;
  AffiliatedMatchHelper match_helper_{&mock_affiliation_service_};
};

TEST_F(GetLoginsWithAffiliationsRequestHandlerTest, NoMatchesTest) {
  EXPECT_CALL(affiliation_service(), GetPSLExtensions)
      .WillOnce(RunOnceCallback<0>(std::vector<std::string>()));
  EXPECT_CALL(affiliation_service(), GetAffiliationsAndBranding)
      .WillOnce(RunOnceCallback<2>(std::vector<Facet>(), true));
  GroupedFacets group;
  group.facets.emplace_back(FacetURI::FromPotentiallyInvalidSpec(kTestWebURL));
  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillOnce(RunOnceCallback<1>(std::vector<GroupedFacets>{group}));

  base::MockCallback<LoginsOrErrorReply> result_callback;
  PasswordFormDigest observed_form = CreateFormDigest(kTestWebURL);
  base::HistogramTester histogram_tester;
  GetLoginsWithAffiliationsRequestHandler(
      observed_form, backend(), &match_helper(), result_callback.Get());

  EXPECT_CALL(result_callback, Run(VariantWith<LoginsResult>(IsEmpty())));
  RunUntilIdle();
}

TEST_F(GetLoginsWithAffiliationsRequestHandlerTest, ExactAndPslMatchesTest) {
  backend()->AddLoginAsync(CreateForm(kTestWebURL, u"username1", u"password"),
                           base::DoNothing());
  backend()->AddLoginAsync(CreateForm(kTestPSLURL, u"username2", u"password"),
                           base::DoNothing());
  RunUntilIdle();

  EXPECT_CALL(affiliation_service(), GetPSLExtensions)
      .WillOnce(RunOnceCallback<0>(std::vector<std::string>()));
  EXPECT_CALL(affiliation_service(), GetAffiliationsAndBranding)
      .WillOnce(RunOnceCallback<2>(std::vector<Facet>(), true));
  GroupedFacets group;
  group.facets.emplace_back(FacetURI::FromPotentiallyInvalidSpec(kTestWebURL));
  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillOnce(RunOnceCallback<1>(std::vector<GroupedFacets>{group}));

  base::MockCallback<LoginsOrErrorReply> result_callback;
  PasswordFormDigest observed_form = CreateFormDigest(kTestWebURL);
  GetLoginsWithAffiliationsRequestHandler(
      observed_form, backend(), &match_helper(), result_callback.Get());

  std::vector<PasswordForm> expected_forms;
  expected_forms.push_back(CreateForm(kTestWebURL, u"username1", u"password"));
  expected_forms.back().match_type = PasswordForm::MatchType::kExact;
  expected_forms.push_back(CreateForm(kTestPSLURL, u"username2", u"password"));
  expected_forms.back().match_type = PasswordForm::MatchType::kPSL;

  EXPECT_CALL(result_callback,
              Run(VariantWith<LoginsResult>(ElementsAreArray(expected_forms))));
  RunUntilIdle();
}

TEST_F(GetLoginsWithAffiliationsRequestHandlerTest, AffiliatedMatchesOnlyTest) {
  backend()->AddLoginAsync(
      CreateForm(kAffiliatedWebURL, u"username1", u"password"),
      base::DoNothing());
  backend()->AddLoginAsync(
      CreateForm(kAffiliatedAndroidApp, u"username2", u"password"),
      base::DoNothing());
  RunUntilIdle();

  EXPECT_CALL(affiliation_service(), GetPSLExtensions)
      .WillOnce(RunOnceCallback<0>(std::vector<std::string>()));
  std::vector<Facet> facets;
  facets.emplace_back(FacetURI::FromPotentiallyInvalidSpec(kAffiliatedWebURL));
  facets.emplace_back(
      FacetURI::FromPotentiallyInvalidSpec(kAffiliatedAndroidApp));
  EXPECT_CALL(affiliation_service(), GetAffiliationsAndBranding)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<2>(facets, true));
  GroupedFacets group;
  group.facets.emplace_back(FacetURI::FromPotentiallyInvalidSpec(kTestWebURL));
  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillOnce(RunOnceCallback<1>(std::vector<GroupedFacets>{group}));
  ;

  PasswordFormDigest observed_form = CreateFormDigest(kTestWebURL);
  base::MockCallback<LoginsOrErrorReply> result_callback;
  GetLoginsWithAffiliationsRequestHandler(
      observed_form, backend(), &match_helper(), result_callback.Get());

  std::vector<PasswordForm> expected_forms;
#if !BUILDFLAG(IS_ANDROID)
  expected_forms.push_back(
      CreateForm(kAffiliatedWebURL, u"username1", u"password"));
  expected_forms.back().match_type = PasswordForm::MatchType::kAffiliated;
#endif
  expected_forms.push_back(
      CreateForm(kAffiliatedAndroidApp, u"username2", u"password"));
  expected_forms.back().affiliated_web_realm = kAffiliatedWebURL;
  expected_forms.back().match_type = PasswordForm::MatchType::kAffiliated;

  EXPECT_CALL(result_callback, Run(VariantWith<LoginsResult>(
                                   UnorderedElementsAreArray(expected_forms))));
  RunUntilIdle();
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(GetLoginsWithAffiliationsRequestHandlerTest,
       AffiliatedAndPSLMatchesTest) {
  backend()->AddLoginAsync(CreateForm(kTestWebURL, u"username1", u"password"),
                           base::DoNothing());
  backend()->AddLoginAsync(CreateForm(kTestPSLURL, u"username2", u"password"),
                           base::DoNothing());
  GroupedFacets group;
  group.facets.emplace_back(FacetURI::FromPotentiallyInvalidSpec(kTestWebURL));
  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillOnce(RunOnceCallback<1>(std::vector<GroupedFacets>{group}));
  backend()->AddLoginAsync(
      CreateForm(kAffiliatedWebURL, u"username3", u"password"),
      base::DoNothing());
  backend()->AddLoginAsync(
      CreateForm(kAffiliatedAndroidApp, u"username4", u"password"),
      base::DoNothing());
  RunUntilIdle();

  EXPECT_CALL(affiliation_service(), GetPSLExtensions)
      .WillOnce(RunOnceCallback<0>(std::vector<std::string>()));
  std::vector<Facet> facets;
  facets.emplace_back(FacetURI::FromPotentiallyInvalidSpec(kAffiliatedWebURL));
  facets.emplace_back(
      FacetURI::FromPotentiallyInvalidSpec(kAffiliatedAndroidApp));
  EXPECT_CALL(affiliation_service(), GetAffiliationsAndBranding)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<2>(facets, true));

  PasswordFormDigest observed_form = CreateFormDigest(kTestWebURL);
  base::MockCallback<LoginsOrErrorReply> result_callback;
  GetLoginsWithAffiliationsRequestHandler(
      observed_form, backend(), &match_helper(), result_callback.Get());

  std::vector<PasswordForm> expected_forms;
  expected_forms.push_back(CreateForm(kTestWebURL, u"username1", u"password"));
  expected_forms.back().match_type = PasswordForm::MatchType::kExact;
  expected_forms.push_back(CreateForm(kTestPSLURL, u"username2", u"password"));
  expected_forms.back().match_type = PasswordForm::MatchType::kPSL;
  expected_forms.push_back(
      CreateForm(kAffiliatedWebURL, u"username3", u"password"));
  expected_forms.back().match_type = PasswordForm::MatchType::kAffiliated;
  expected_forms.push_back(
      CreateForm(kAffiliatedAndroidApp, u"username4", u"password"));
  expected_forms.back().affiliated_web_realm = kAffiliatedWebURL;
  expected_forms.back().match_type = PasswordForm::MatchType::kAffiliated;

  EXPECT_CALL(result_callback, Run(VariantWith<LoginsResult>(
                                   UnorderedElementsAreArray(expected_forms))));
  RunUntilIdle();
}

TEST_F(GetLoginsWithAffiliationsRequestHandlerTest, AffiliationsArePSLTest) {
  backend()->AddLoginAsync(CreateForm(kTestWebURL, u"username1", u"password"),
                           base::DoNothing());
  backend()->AddLoginAsync(CreateForm(kTestPSLURL, u"username2", u"password"),
                           base::DoNothing());
  GroupedFacets group;
  group.facets.emplace_back(FacetURI::FromPotentiallyInvalidSpec(kTestWebURL));
  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillOnce(RunOnceCallback<1>(std::vector<GroupedFacets>{group}));
  RunUntilIdle();

  EXPECT_CALL(affiliation_service(), GetPSLExtensions)
      .WillOnce(RunOnceCallback<0>(std::vector<std::string>()));
  std::vector<Facet> facets;
  facets.emplace_back(FacetURI::FromPotentiallyInvalidSpec(kTestWebURL));
  facets.emplace_back(FacetURI::FromPotentiallyInvalidSpec(kTestPSLURL));
  EXPECT_CALL(affiliation_service(), GetAffiliationsAndBranding)
      .WillOnce(RunOnceCallback<2>(facets, true));

  PasswordFormDigest observed_form = CreateFormDigest(kTestWebURL);
  base::MockCallback<LoginsOrErrorReply> result_callback;
  GetLoginsWithAffiliationsRequestHandler(
      observed_form, backend(), &match_helper(), result_callback.Get());

  std::vector<PasswordForm> expected_forms;
  expected_forms.push_back(CreateForm(kTestWebURL, u"username1", u"password"));
  expected_forms.back().match_type = PasswordForm::MatchType::kExact;
  expected_forms.push_back(CreateForm(kTestPSLURL, u"username2", u"password"));
  expected_forms.back().match_type =
      PasswordForm::MatchType::kAffiliated | PasswordForm::MatchType::kPSL;

  EXPECT_CALL(result_callback,
              Run(VariantWith<LoginsResult>(ElementsAreArray(expected_forms))));
  RunUntilIdle();
}

TEST_F(GetLoginsWithAffiliationsRequestHandlerTest, GroupedMatchesOnlyTest) {
  backend()->AddLoginAsync(CreateForm(kGroupWebURL, u"username", u"password"),
                           base::DoNothing());
  RunUntilIdle();

  EXPECT_CALL(affiliation_service(), GetPSLExtensions)
      .WillOnce(RunOnceCallback<0>(std::vector<std::string>()));
  EXPECT_CALL(affiliation_service(), GetAffiliationsAndBranding)
      .WillOnce(RunOnceCallback<2>(std::vector<Facet>(), true));

  GroupedFacets group;
  group.facets.emplace_back(FacetURI::FromPotentiallyInvalidSpec(kTestWebURL));
  group.facets.emplace_back(FacetURI::FromPotentiallyInvalidSpec(kGroupWebURL));
  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillOnce(RunOnceCallback<1>(std::vector<GroupedFacets>{group}));

  PasswordFormDigest observed_form = CreateFormDigest(kTestWebURL);
  base::MockCallback<LoginsOrErrorReply> result_callback;
  GetLoginsWithAffiliationsRequestHandler(
      observed_form, backend(), &match_helper(), result_callback.Get());

  PasswordForm expected_form =
      CreateForm(kGroupWebURL, u"username", u"password");
  expected_form.match_type = PasswordForm::MatchType::kGrouped;

  EXPECT_CALL(result_callback,
              Run(VariantWith<LoginsResult>(ElementsAre(expected_form))));
  RunUntilIdle();
}

TEST_F(GetLoginsWithAffiliationsRequestHandlerTest,
       GroupedAndAffiliatedMatchesIntersectTest) {
  backend()->AddLoginAsync(
      CreateForm(kAffiliatedAndroidApp, u"username1", u"password"),
      base::DoNothing());
  backend()->AddLoginAsync(CreateForm(kGroupWebURL, u"username2", u"password"),
                           base::DoNothing());
  RunUntilIdle();
  EXPECT_CALL(affiliation_service(), GetPSLExtensions)
      .WillOnce(RunOnceCallback<0>(std::vector<std::string>()));

  std::vector<Facet> facets;
  facets.emplace_back(FacetURI::FromPotentiallyInvalidSpec(kTestWebURL));
  facets.emplace_back(
      FacetURI::FromPotentiallyInvalidSpec(kAffiliatedAndroidApp));
  EXPECT_CALL(affiliation_service(), GetAffiliationsAndBranding)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<2>(facets, true));

  GroupedFacets group;
  group.facets.emplace_back(FacetURI::FromPotentiallyInvalidSpec(kTestWebURL));
  group.facets.emplace_back(FacetURI::FromPotentiallyInvalidSpec(kGroupWebURL));
  group.facets.emplace_back(
      FacetURI::FromPotentiallyInvalidSpec(kAffiliatedAndroidApp));
  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillOnce(RunOnceCallback<1>(std::vector<GroupedFacets>{group}));

  PasswordFormDigest observed_form = CreateFormDigest(kTestWebURL);
  base::MockCallback<LoginsOrErrorReply> result_callback;
  base::HistogramTester histogram_tester;
  GetLoginsWithAffiliationsRequestHandler(
      observed_form, backend(), &match_helper(), result_callback.Get());

  std::vector<PasswordForm> expected_forms;
  expected_forms.push_back(
      CreateForm(kAffiliatedAndroidApp, u"username1", u"password"));
  expected_forms.back().affiliated_web_realm = kTestWebURL;
  expected_forms.back().match_type =
      PasswordForm::MatchType::kAffiliated | PasswordForm::MatchType::kGrouped;
  expected_forms.push_back(CreateForm(kGroupWebURL, u"username2", u"password"));
  expected_forms.back().match_type = PasswordForm::MatchType::kGrouped;

  EXPECT_CALL(result_callback,
              Run(VariantWith<LoginsResult>(ElementsAreArray(expected_forms))));
  RunUntilIdle();
}

TEST_F(GetLoginsWithAffiliationsRequestHandlerTest,
       PslMatchInExtensionListButAffiliatedTest) {
  backend()->AddLoginAsync(CreateForm("https://a.slack.com/", u"test", u"test"),
                           base::DoNothing());
  backend()->AddLoginAsync(
      CreateForm("https://b.slack.com/", u"test2", u"test"), base::DoNothing());
  backend()->AddLoginAsync(
      CreateForm("https://c.slack.com/", u"test3", u"test"), base::DoNothing());
  RunUntilIdle();

  EXPECT_CALL(affiliation_service(), GetPSLExtensions)
      .WillOnce(RunOnceCallback<0>(std::vector<std::string>{"slack.com"}));
  std::vector<Facet> facets;
  facets.emplace_back(
      FacetURI::FromPotentiallyInvalidSpec("https://a.slack.com/"));
  facets.emplace_back(
      FacetURI::FromPotentiallyInvalidSpec("https://b.slack.com/"));
  EXPECT_CALL(affiliation_service(), GetAffiliationsAndBranding)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<2>(facets, true));

  GroupedFacets group;
  group.facets.emplace_back(FacetURI::FromPotentiallyInvalidSpec(kTestWebURL));
  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillOnce(RunOnceCallback<1>(std::vector<GroupedFacets>{group}));

  base::MockCallback<LoginsOrErrorReply> result_callback;
  PasswordFormDigest observed_form = CreateFormDigest("https://a.slack.com/");
  GetLoginsWithAffiliationsRequestHandler(
      observed_form, backend(), &match_helper(), result_callback.Get());

  std::vector<PasswordForm> expected_forms;
  expected_forms.push_back(
      CreateForm("https://a.slack.com/", u"test", u"test"));
  expected_forms.back().match_type = PasswordForm::MatchType::kExact;
  expected_forms.push_back(
      CreateForm("https://b.slack.com/", u"test2", u"test"));
  // The second form is only affiliated not PSL matched.
  expected_forms.back().match_type = PasswordForm::MatchType::kAffiliated;

  EXPECT_CALL(result_callback,
              Run(VariantWith<LoginsResult>(ElementsAreArray(expected_forms))));
  RunUntilIdle();
}

TEST_F(GetLoginsWithAffiliationsRequestHandlerTest,
       PslMatchesFilteredBecauseOfExtensionListTest) {
  backend()->AddLoginAsync(CreateForm("https://a.slack.com/", u"test", u"test"),
                           base::DoNothing());
  backend()->AddLoginAsync(CreateForm("https://b.slack.com/", u"test", u"test"),
                           base::DoNothing());
  RunUntilIdle();

  EXPECT_CALL(affiliation_service(), GetPSLExtensions)
      .WillOnce(RunOnceCallback<0>(std::vector<std::string>{"slack.com"}));
  EXPECT_CALL(affiliation_service(), GetAffiliationsAndBranding)
      .WillOnce(RunOnceCallback<2>(std::vector<Facet>(), true));
  GroupedFacets group;
  group.facets.emplace_back(FacetURI::FromPotentiallyInvalidSpec(kTestWebURL));
  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillOnce(RunOnceCallback<1>(std::vector<GroupedFacets>{group}));

  base::MockCallback<LoginsOrErrorReply> result_callback;
  PasswordFormDigest observed_form = CreateFormDigest("https://a.slack.com/");
  GetLoginsWithAffiliationsRequestHandler(
      observed_form, backend(), &match_helper(), result_callback.Get());

  PasswordForm expected_form =
      CreateForm("https://a.slack.com/", u"test", u"test");
  expected_form.match_type = PasswordForm::MatchType::kExact;

  EXPECT_CALL(result_callback,
              Run(VariantWith<LoginsResult>(ElementsAre(expected_form))));
  RunUntilIdle();
}

#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(GetLoginsWithAffiliationsRequestHandlerTest, AffiliatedMatchHelperNull) {
  backend()->AddLoginAsync(CreateForm(kTestWebURL, u"username1", u"password"),
                           base::DoNothing());
  backend()->AddLoginAsync(CreateForm(kTestPSLURL, u"username2", u"password"),
                           base::DoNothing());
  RunUntilIdle();

  EXPECT_CALL(affiliation_service(), GetAffiliationsAndBranding).Times(0);
  EXPECT_CALL(affiliation_service(), GetPSLExtensions).Times(0);
  base::MockCallback<LoginsOrErrorReply> result_callback;
  PasswordFormDigest observed_form = CreateFormDigest(kTestWebURL);
  GetLoginsWithAffiliationsRequestHandler(observed_form, backend(),
                                          /*affiliated_match_helper=*/nullptr,
                                          result_callback.Get());

  std::vector<PasswordForm> expected_forms;
  expected_forms.push_back(CreateForm(kTestWebURL, u"username1", u"password"));
  expected_forms.back().match_type = PasswordForm::MatchType::kExact;
  expected_forms.push_back(CreateForm(kTestPSLURL, u"username2", u"password"));
  expected_forms.back().match_type = PasswordForm::MatchType::kPSL;

  EXPECT_CALL(result_callback,
              Run(VariantWith<LoginsResult>(ElementsAreArray(expected_forms))));
  RunUntilIdle();
}

TEST_F(GetLoginsWithAffiliationsRequestHandlerTest,
       TrimUsernameOnlyCredentials) {
  PasswordForm username_only;
  username_only.scheme = PasswordForm::Scheme::kUsernameOnly;
  username_only.signon_realm = kAffiliatedAndroidApp;
  username_only.username_value = u"test";
  username_only.in_store = PasswordForm::Store::kProfileStore;

  PasswordForm federated_credential;
  federated_credential.in_store = PasswordForm::Store::kProfileStore;
  federated_credential.scheme = PasswordForm::Scheme::kUsernameOnly;
  federated_credential.signon_realm = kAffiliatedAndroidApp;
  federated_credential.username_value = u"test";
  federated_credential.federation_origin =
      url::SchemeHostPort(GURL("https://google.com/"));
  federated_credential.skip_zero_click = false;

  backend()->AddLoginAsync(username_only, base::DoNothing());
  backend()->AddLoginAsync(federated_credential, base::DoNothing());
  RunUntilIdle();

  EXPECT_CALL(affiliation_service(), GetPSLExtensions)
      .WillOnce(RunOnceCallback<0>(std::vector<std::string>()));
  std::vector<Facet> facets;
  facets.emplace_back(FacetURI::FromPotentiallyInvalidSpec(kAffiliatedWebURL));
  facets.emplace_back(
      FacetURI::FromPotentiallyInvalidSpec(kAffiliatedAndroidApp));
  EXPECT_CALL(affiliation_service(), GetAffiliationsAndBranding)
      .WillOnce(RunOnceCallback<2>(facets, true));
  GroupedFacets group;
  group.facets.emplace_back(
      FacetURI::FromPotentiallyInvalidSpec(kAffiliatedWebURL));
  EXPECT_CALL(affiliation_service(), GetGroupingInfo)
      .WillOnce(RunOnceCallback<1>(std::vector<GroupedFacets>{group}));

  base::MockCallback<LoginsOrErrorReply> result_callback;
  PasswordFormDigest observed_form = CreateFormDigest(kAffiliatedWebURL);
  GetLoginsWithAffiliationsRequestHandler(
      observed_form, backend(), &match_helper(), result_callback.Get());

  PasswordForm expected_form = federated_credential;
  expected_form.match_type = PasswordForm::MatchType::kAffiliated;
  expected_form.skip_zero_click = true;

  EXPECT_CALL(result_callback,
              Run(VariantWith<LoginsResult>(ElementsAre(expected_form))));
  RunUntilIdle();
}

}  // namespace password_manager
