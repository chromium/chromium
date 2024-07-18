// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search/start_suggest_service.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/search/search.h"
#include "components/search/search_provider_observer.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/search_engines/template_url_service.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::string GoodServerResponse() {
  return R"()]}'
  [
     "",
     [
        "query1",
        "query2"
     ],
     [],
     [],
     {}
  ])";
}

std::string GoodServerResponse2() {
  return R"()]}'
[
   "",
   [
      "query3",
      "query4"
   ],
   [],
   [],
   {}
])";
}

std::string BadServerResponse() {
  return R"()]}'
[
   "",
])";
}

}  // namespace

class MockSearchProviderObserver : public SearchProviderObserver {
 public:
  MockSearchProviderObserver()
      : SearchProviderObserver(/*service=*/nullptr, base::DoNothing()) {}
  ~MockSearchProviderObserver() override = default;

  MOCK_METHOD0(is_google, bool());
};

class TestStartSuggestService : public StartSuggestService {
 public:
  TestStartSuggestService(
      TemplateURLService* template_url_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<AutocompleteSchemeClassifier> scheme_classifier,
      const GURL& request_initiator_url)
      : StartSuggestService(template_url_service,
                            url_loader_factory,
                            std::move(scheme_classifier),
                            "us",
                            "en",
                            request_initiator_url) {}
  ~TestStartSuggestService() override = default;

  MockSearchProviderObserver* search_provider_observer() override {
    return &search_provider_observer_;
  }

  GURL GetRequestURL(TemplateURLRef::SearchTermsArgs search_terms_args) {
    return StartSuggestService::GetRequestURL(search_terms_args);
  }

  GURL GetQueryDestinationURL(const std::u16string& query,
                              const TemplateURL* search_provider) {
    return StartSuggestService::GetQueryDestinationURL(query, search_provider);
  }

  void SearchProviderChanged() { StartSuggestService::SearchProviderChanged(); }

  testing::NiceMock<MockSearchProviderObserver> search_provider_observer_;
};

class StartSuggestServiceTest : public ::testing::Test {
 public:
  StartSuggestServiceTest() : weak_factory_(this) {}
  ~StartSuggestServiceTest() override = default;

  void SetUp() override {
    service_ = std::make_unique<TestStartSuggestService>(
        search_engines_test_environment_.template_url_service(),
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        std::make_unique<TestSchemeClassifier>(), GURL());
  }

  void TearDown() override { service_->Shutdown(); }

  void ResetSuggestions() { suggestions_ = std::nullopt; }

  void WaitForSuggestions() {
    if (suggestions_.has_value()) {
      return;
    }

    suggestions_run_loop_ = std::make_unique<base::RunLoop>();
    suggestions_run_loop_->Run();
    suggestions_run_loop_.reset();
  }

  void OnSuggestionsReceived(std::vector<QuerySuggestion> suggestions) {
    if (!suggestions_.has_value() && suggestions_run_loop_) {
      suggestions_run_loop_->Quit();
    }
    suggestions_ = std::move(suggestions);
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

  TemplateURLService* template_url_service() {
    return search_engines_test_environment_.template_url_service();
  }

  TestStartSuggestService* service() { return service_.get(); }

  const TemplateURL* default_search_provider() {
    return template_url_service()->GetDefaultSearchProvider();
  }

  base::WeakPtr<StartSuggestServiceTest> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  const std::vector<QuerySuggestion>& suggestions() const {
    return suggestions_.value();
  }

  GURL GetQueryDestinationURL(const std::string& query) {
    return service_->GetQueryDestinationURL(base::ASCIIToUTF16(query),
                                            default_search_provider());
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<TestStartSuggestService> service_;

  std::optional<std::vector<QuerySuggestion>> suggestions_;
  std::unique_ptr<base::RunLoop> suggestions_run_loop_;

  base::WeakPtrFactory<StartSuggestServiceTest> weak_factory_;
};

// Test that, upon receiving a valid response, the service returns the
// suggestions as expected. In addition, when the default search engine changes
// away from Google, tests that the service does not return anything.
TEST_F(StartSuggestServiceTest,
       HandleBasicValidResponseAndClearWhenChangeDefaultSearchEngine) {
  TemplateURLRef::SearchTermsArgs args;
  test_url_loader_factory()->AddResponse(service()->GetRequestURL(args).spec(),
                                         GoodServerResponse());
  EXPECT_CALL(*service()->search_provider_observer(), is_google())
      .WillOnce(testing::Return(true));
  ResetSuggestions();
  service()->FetchSuggestions(
      args, base::BindOnce(&StartSuggestServiceTest::OnSuggestionsReceived,
                           GetWeakPtr()));
  WaitForSuggestions();

  std::vector<QuerySuggestion> expected_server_queries{
      {u"query1", GetQueryDestinationURL("query1")},
      {u"query2", GetQueryDestinationURL("query2")}};
  ASSERT_EQ(2.0, suggestions().size());
  EXPECT_EQ(expected_server_queries.front(), suggestions().front());
  EXPECT_EQ(expected_server_queries.at(1), suggestions().at(1));

  // Test that if the default search engine is not Google the service returns no
  // suggestions even if it has some stored.
  EXPECT_CALL(*service()->search_provider_observer(), is_google())
      .WillOnce(testing::Return(false));
  ResetSuggestions();
  service()->FetchSuggestions(
      args, base::BindOnce(&StartSuggestServiceTest::OnSuggestionsReceived,
                           GetWeakPtr()));
  WaitForSuggestions();
  ASSERT_EQ(0.0, suggestions().size());
}

// Test that the service synchronously returns cached fetched suggestions
// upon a subsequent FetchSuggestions() call.
TEST_F(StartSuggestServiceTest, TestReturnSavedSuggestions) {
  TemplateURLRef::SearchTermsArgs args;
  test_url_loader_factory()->AddResponse(service()->GetRequestURL(args).spec(),
                                         GoodServerResponse());
  EXPECT_CALL(*service()->search_provider_observer(), is_google())
      .WillRepeatedly(testing::Return(true));
  ResetSuggestions();
  service()->FetchSuggestions(
      args, base::BindOnce(&StartSuggestServiceTest::OnSuggestionsReceived,
                           GetWeakPtr()));
  WaitForSuggestions();

  std::vector<QuerySuggestion> expected_server_queries{
      {u"query1", GetQueryDestinationURL("query1")},
      {u"query2", GetQueryDestinationURL("query2")}};
  ASSERT_EQ(2.0, suggestions().size());
  EXPECT_EQ(expected_server_queries.front(), suggestions().front());
  EXPECT_EQ(expected_server_queries.at(1), suggestions().at(1));

  test_url_loader_factory()->ClearResponses();

  // This fetch should not need WaitForSuggestions() since the saved suggestions
  // should be used.
  service()->FetchSuggestions(
      args, base::BindOnce(&StartSuggestServiceTest::OnSuggestionsReceived,
                           GetWeakPtr()));
  ASSERT_EQ(2.0, suggestions().size());
  // They may be shuffled.
  bool first_expected_query_found = false;
  bool second_expected_query_found = false;
  for (const auto& suggestion : suggestions()) {
    if (expected_server_queries.front() == suggestion) {
      first_expected_query_found = true;
    }
    if (expected_server_queries.at(1) == suggestion) {
      second_expected_query_found = true;
    }
  }
  EXPECT_TRUE(first_expected_query_found);
  EXPECT_TRUE(second_expected_query_found);
}

// Tests that explicitly setting fetch_from_server to true bypasses any cache
// and sends a request.
TEST_F(StartSuggestServiceTest, TestFetchFromServerSet) {
  TemplateURLRef::SearchTermsArgs args;
  test_url_loader_factory()->AddResponse(service()->GetRequestURL(args).spec(),
                                         GoodServerResponse());
  EXPECT_CALL(*service()->search_provider_observer(), is_google())
      .WillRepeatedly(testing::Return(true));
  ResetSuggestions();
  service()->FetchSuggestions(
      args, base::BindOnce(&StartSuggestServiceTest::OnSuggestionsReceived,
                           GetWeakPtr()));
  WaitForSuggestions();

  std::vector<QuerySuggestion> expected_server_queries{
      {u"query1", GetQueryDestinationURL("query1")},
      {u"query2", GetQueryDestinationURL("query2")}};
  ASSERT_EQ(2.0, suggestions().size());
  EXPECT_EQ(expected_server_queries.front(), suggestions().front());
  EXPECT_EQ(expected_server_queries.at(1), suggestions().at(1));

  test_url_loader_factory()->ClearResponses();
  test_url_loader_factory()->AddResponse(service()->GetRequestURL(args).spec(),
                                         GoodServerResponse2());
  ResetSuggestions();
  service()->FetchSuggestions(
      args,
      base::BindOnce(&StartSuggestServiceTest::OnSuggestionsReceived,
                     GetWeakPtr()),
      true);
  WaitForSuggestions();

  std::vector<QuerySuggestion> second_expected_server_queries{
      {u"query3", GetQueryDestinationURL("query3")},
      {u"query4", GetQueryDestinationURL("query4")}};
  ASSERT_EQ(2.0, suggestions().size());
  EXPECT_EQ(second_expected_server_queries.front(), suggestions().front());
  EXPECT_EQ(second_expected_server_queries.at(1), suggestions().at(1));
}

// Test that the service returns an empty list in response to bad JSON returned.
TEST_F(StartSuggestServiceTest, TestBadResponseReturnsNothing) {
  TemplateURLRef::SearchTermsArgs args;
  test_url_loader_factory()->AddResponse(service()->GetRequestURL(args).spec(),
                                         BadServerResponse());
  EXPECT_CALL(*service()->search_provider_observer(), is_google())
      .WillRepeatedly(testing::Return(true));
  ResetSuggestions();
  service()->FetchSuggestions(
      args, base::BindOnce(&StartSuggestServiceTest::OnSuggestionsReceived,
                           GetWeakPtr()));
  WaitForSuggestions();

  ASSERT_EQ(0.0, suggestions().size());
}
