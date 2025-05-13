// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/enterprise_search_aggregator_suggestions_service.h"

#include "base/functional/bind.h"
#include "base/json/json_parser.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/variations/net/variations_http_headers.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_ids_provider.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

variations::VariationID kVariationID = 123;

const std::string& mock_response = base::StringPrintf({
    R"({"querySuggestions" : [ {
   "suggestion" : "sundar pichai",
   "dataStore" : [ "projects1", "dataStore1", "project2", "dataStore2" ]
   } ],
   "peopleSuggestions" : [ {
       "document" : {
       "name" : "",
       "derivedStructData" : {
           "displayPhoto" : {
           "url" : "example.com"
           },
           "name" : {
           "family_name_lower" : "doe",
           "familyName" : "Doe",
           "userName" : "janedoe@example.com",
           "givenName" : "Doe",
           "display_name_lower" : "jane doe",
           "displayName" : "Jane Doe",
           "given_name_lower" : "jane"
           },
           "emails" : [ {"type" : "primary", "value" : "janedoe@example.com"} ]
       }
       },
       "dataStore" : ""
   } ] })"});

class EnterpriseSearchAggregatorSuggestionsServiceTest : public testing::Test {
 public:
  EnterpriseSearchAggregatorSuggestionsServiceTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI),
        shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        identity_test_env_(&test_url_loader_factory_, &prefs_),
        enterprise_search_aggregator_suggestions_service_(
            new EnterpriseSearchAggregatorSuggestionsService(
                identity_test_env_.identity_manager(),
                shared_url_loader_factory_)) {
    // Set up a variation.
    variations::AssociateGoogleVariationID(
        variations::GOOGLE_WEB_PROPERTIES_ANY_CONTEXT, "trial name",
        "group name", kVariationID);
    base::FieldTrialList::CreateFieldTrial("trial name", "group name")
        ->Activate();
  }
  EnterpriseSearchAggregatorSuggestionsServiceTest(
      const EnterpriseSearchAggregatorSuggestionsServiceTest&) = delete;
  EnterpriseSearchAggregatorSuggestionsServiceTest& operator=(
      const EnterpriseSearchAggregatorSuggestionsServiceTest&) = delete;

 protected:
  AccountInfo SetUpPrimaryAccount() {
    auto account_info = identity_test_env_.MakePrimaryAccountAvailable(
        "foo@gmail.com", signin::ConsentLevel::kSignin);
    identity_test_env_.SetRefreshTokenForPrimaryAccount();
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
    return account_info;
  }

  base::test::TaskEnvironment task_environment_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<EnterpriseSearchAggregatorSuggestionsService>
      enterprise_search_aggregator_suggestions_service_;
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::SearchAggregatorProvider>
      scoped_config_;
};

TEST_F(EnterpriseSearchAggregatorSuggestionsServiceTest,
       ValidateKeywordModeRequest) {
  SetUpPrimaryAccount();
  scoped_config_.Get().multiple_requests = false;

  network::ResourceRequest resource_request;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        resource_request = request;
      }));

  base::Value::Dict root;
  root.Set("query", base::Value("test"));

  base::Value::List suggestion_types_list;
  std::vector<int> suggestion_types = {1, 2, 3, 5};
  for (const auto& item : suggestion_types) {
    suggestion_types_list.Append(item);
  }
  root.Set("suggestionTypes", std::move(suggestion_types_list));

  base::Value::List experiment_ids_list;
  experiment_ids_list.Append(kEnterpriseSearchAggregatorExperimentId);
  root.Set("experimentIds", std::move(experiment_ids_list));

  std::string test_request_body;
  base::JSONWriter::Write(root, &test_request_body);
  const std::u16string query = u"test";
  const GURL test_endpoint = GURL("https://fake_url.com");

  base::test::TestFuture<network::ResourceRequest*> request_future;
  base::test::TestFuture<int, std::unique_ptr<network::SimpleURLLoader>,
                         const std::string&>
      loader_future;
  base::test::TestFuture<const network::SimpleURLLoader*, int,
                         std::unique_ptr<std::string>>
      complete_future;

  enterprise_search_aggregator_suggestions_service_
      ->CreateEnterpriseSearchAggregatorSuggestionsRequest(
          query, test_endpoint, request_future.GetRepeatingCallback(),
          loader_future.GetRepeatingCallback(),
          complete_future.GetRepeatingCallback(),
          std::vector<std::vector<int>>{std::vector<int>{1, 2, 3, 5}});

  ASSERT_TRUE(request_future.Wait());
  ASSERT_TRUE(loader_future.Wait());

  const std::string mock_response_body = mock_response;
  test_url_loader_factory_.AddResponse(test_endpoint.spec(), mock_response_body,
                                       net::HTTP_OK);
  ASSERT_TRUE(complete_future.Wait());

  EXPECT_TRUE(resource_request.site_for_cookies.IsEquivalent(
      net::SiteForCookies::FromUrl(GURL(test_endpoint))))
      << resource_request.site_for_cookies.ToDebugString();

  EXPECT_EQ(resource_request.request_body->elements()->size(), 1u);

  std::optional<base::Value> request_body =
      base::JSONReader::Read(resource_request.request_body->elements()
                                 ->at(0)
                                 .As<network::DataElementBytes>()
                                 .AsStringPiece());
  std::optional<base::Value> test_request_body_value =
      base::JSONReader::Read(test_request_body);
  EXPECT_EQ(request_body, test_request_body_value);
}

TEST_F(EnterpriseSearchAggregatorSuggestionsServiceTest,
       ValidateNonKeywordModeRequest) {
  SetUpPrimaryAccount();
  scoped_config_.Get().multiple_requests = false;

  network::ResourceRequest resource_request;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        resource_request = request;
      }));

  base::Value::Dict root;
  root.Set("query", base::Value("test"));

  base::Value::List suggestion_types_list;
  std::vector<int> suggestion_types = {2, 3, 5};
  for (const auto& item : suggestion_types) {
    suggestion_types_list.Append(item);
  }
  root.Set("suggestionTypes", std::move(suggestion_types_list));

  base::Value::List experiment_ids_list;
  experiment_ids_list.Append(kEnterpriseSearchAggregatorExperimentId);
  root.Set("experimentIds", std::move(experiment_ids_list));

  std::string test_request_body;
  base::JSONWriter::Write(root, &test_request_body);
  const std::u16string query = u"test";
  const GURL test_endpoint = GURL("https://fake_url.com");

  base::test::TestFuture<network::ResourceRequest*> request_future;
  base::test::TestFuture<int, std::unique_ptr<network::SimpleURLLoader>,
                         const std::string&>
      loader_future;
  base::test::TestFuture<const network::SimpleURLLoader*, int,
                         std::unique_ptr<std::string>>
      complete_future;

  enterprise_search_aggregator_suggestions_service_
      ->CreateEnterpriseSearchAggregatorSuggestionsRequest(
          query, test_endpoint, request_future.GetRepeatingCallback(),
          loader_future.GetRepeatingCallback(),
          complete_future.GetRepeatingCallback(),
          std::vector<std::vector<int>>{std::vector<int>{2, 3, 5}});
  ASSERT_TRUE(request_future.Wait());
  ASSERT_TRUE(loader_future.Wait());

  const std::string mock_response_body = mock_response;
  test_url_loader_factory_.AddResponse(test_endpoint.spec(), mock_response_body,
                                       net::HTTP_OK);
  ASSERT_TRUE(complete_future.Wait());

  EXPECT_TRUE(resource_request.site_for_cookies.IsEquivalent(
      net::SiteForCookies::FromUrl(GURL(test_endpoint))))
      << resource_request.site_for_cookies.ToDebugString();

  EXPECT_EQ(resource_request.request_body->elements()->size(), 1u);

  std::optional<base::Value> request_body =
      base::JSONReader::Read(resource_request.request_body->elements()
                                 ->at(0)
                                 .As<network::DataElementBytes>()
                                 .AsStringPiece());
  std::optional<base::Value> test_request_body_value =
      base::JSONReader::Read(test_request_body);
  EXPECT_EQ(request_body, test_request_body_value);
}

}  // namespace
