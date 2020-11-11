// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search/repeatable_queries/repeatable_queries_service.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/files/scoped_temp_dir.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/url_database.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/omnibox/browser/in_memory_url_index.h"
#include "components/omnibox/browser/in_memory_url_index_test_util.h"
#include "components/search/search.h"
#include "components/search/search_provider_observer.h"
#include "components/search_engines/search_engines_test_util.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

namespace {

std::string GoodServerResponse() {
  return R"()]}'
[
   "",
   [
      "server query 1",
      "server query 2",
      "server query 3"
   ],
   [],
   [],
   {
      "google:suggestdetail":[
         {
            "du":"/delete?server+query+1"
         },
         {
            "du":"/delete?server+query+2"
         },
         {
            "du":"/delete?server+query+3"
         }
      ]
   }
])";
}

std::string BadServerResponse1() {
  return R"()]}'
[
   "",
   [
      "server query 1",
      "server query 2",
      "server query 3"
   ],
   [],
   [],
   {
   }
])";
}

std::string BadServerResponse2() {
  return R"()]}'
[
   "",
   [
      "server query 1",
      "server query 2",
      "server query 3"
   ],
   [],
   [],
   {
      "google:suggestdetail":[
         {
            "du":"/delete?server+query+1"
         },
         {
            "du":"/delete?server+query+2"
         },
      ]
   }
])";
}

// Used to populate the URLDatabase.
struct TestURLData {
  const TemplateURL* search_provider;
  std::string search_terms;
  int age_in_seconds;
  int visit_count = 1;
  std::string title = "";
  int typed_count = 1;
  bool hidden = false;
};

}  // namespace

class MockSearchProviderObserver : public SearchProviderObserver {
 public:
  MockSearchProviderObserver()
      : SearchProviderObserver(/*template_url_service=*/nullptr,
                               base::DoNothing::Repeatedly()) {}
  ~MockSearchProviderObserver() override = default;

  MOCK_METHOD0(is_google, bool());
};

class TestRepeatableQueriesService : public RepeatableQueriesService {
 public:
  TestRepeatableQueriesService(
      signin::IdentityManager* identity_manager,
      history::HistoryService* history_service,
      TemplateURLService* template_url_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& request_initiator_url)
      : RepeatableQueriesService(identity_manager,
                                 history_service,
                                 template_url_service,
                                 std::move(url_loader_factory),
                                 request_initiator_url) {}
  ~TestRepeatableQueriesService() override = default;

  MockSearchProviderObserver* search_provider_observer() override {
    return &search_provider_observer_;
  }

  GURL GetQueryDestinationURL(const base::string16& query) {
    return RepeatableQueriesService::GetQueryDestinationURL(query);
  }

  GURL GetQueryDeletionURL(const std::string& deletion_url) {
    return RepeatableQueriesService::GetQueryDeletionURL(deletion_url);
  }

  GURL GetRequestURL() { return RepeatableQueriesService::GetRequestURL(); }

  void SearchProviderChanged() {
    RepeatableQueriesService::SearchProviderChanged();
  }

  void SigninStatusChanged() {
    RepeatableQueriesService::SigninStatusChanged();
  }

  testing::NiceMock<MockSearchProviderObserver> search_provider_observer_;
};

class RepeatableQueriesServiceTest : public ::testing::Test,
                                     public RepeatableQueriesServiceObserver {
 public:
  RepeatableQueriesServiceTest() = default;
  ~RepeatableQueriesServiceTest() override = default;

  void SetUp() override {
    bookmark_model_ = bookmarks::TestBookmarkClient::CreateModel();

    CHECK(history_dir_.CreateUniqueTempDir());
    history_service_ = history::CreateHistoryService(
        history_dir_.GetPath(), /*create_history_db=*/true);

    in_memory_url_index_ = std::make_unique<InMemoryURLIndex>(
        bookmark_model_.get(), history_service_.get(), nullptr,
        history_dir_.GetPath(), SchemeSet());
    in_memory_url_index_->Init();

    template_url_service_ = std::make_unique<TemplateURLService>(nullptr, 0);

    // Add the fallback default search provider to the TemplateURLService so
    // that it gets a valid unique identifier. Make the newly added provider the
    // user selected default search provider.
    TemplateURL* default_provider = template_url_service_->Add(
        std::make_unique<TemplateURL>(default_search_provider()->data()));
    template_url_service_->SetUserSelectedDefaultSearchProvider(
        default_provider);
    // Verify that Google is the default search provider.
    EXPECT_TRUE(
        search::DefaultSearchProviderIsGoogle(template_url_service_.get()));

    identity_env_ = std::make_unique<signin::IdentityTestEnvironment>(
        &test_url_loader_factory_);
    identity_env_->MakePrimaryAccountAvailable("example@gmail.com");
    identity_env_->SetAutomaticIssueOfAccessTokens(true);

    service_ = std::make_unique<TestRepeatableQueriesService>(
        identity_env_->identity_manager(), history_service_.get(),
        template_url_service_.get(),
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        GURL());
    EXPECT_TRUE(service_->repeatable_queries().empty());
    service_->AddObserver(this);
  }

  void TearDown() override {
    // RepeatableQueriesService must be explicitly shut down so that its
    // observers can unregister.
    service_->Shutdown();
    // InMemoryURLIndex must be explicitly shut down or it will DCHECK() in
    // its destructor.
    in_memory_url_index_->Shutdown();
    // Needed to prevent leaks due to posted history index rebuild task.
    task_environment_.RunUntilIdle();
  }

  const TemplateURL* default_search_provider() {
    return template_url_service_->GetDefaultSearchProvider();
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

  TestRepeatableQueriesService* service() { return service_.get(); }

  void set_service_is_done(bool is_done) { service_is_done_ = is_done; }

  void SignIn() {
    AccountInfo account_info =
        identity_env_->MakeAccountAvailable("test@email.com");
    identity_env_->SetCookieAccounts({{account_info.email, account_info.gaia}});
  }

  void SignOut() { identity_env_->SetCookieAccounts({}); }

  GURL GetQueryDestinationURL(const std::string& query) {
    return service_->GetQueryDestinationURL(base::ASCIIToUTF16(query));
  }

  void RefreshAndMaybeWaitForService() {
    service_is_done_ = false;
    service_->Refresh();
    MaybeWaitForService();
  }

  void MaybeWaitForService() {
    if (!service_is_done_) {
      service_run_loop_ = std::make_unique<base::RunLoop>();
      // Quits in OnRepeatableQueriesUpdated when the service is done.
      service_run_loop_->Run();
    }
  }

  // Fills the URLDatabase with search URLs created using the provided data.
  void FillURLDatabase(const std::vector<TestURLData>& url_data_list) {
    const Time now = Time::Now();
    for (const auto& entry : url_data_list) {
      TemplateURLRef::SearchTermsArgs search_terms_args(
          base::UTF8ToUTF16(entry.search_terms));
      const auto& search_terms_data =
          template_url_service_->search_terms_data();
      std::string search_url =
          entry.search_provider->url_ref().ReplaceSearchTerms(
              search_terms_args, search_terms_data);
      history_service_->AddPageWithDetails(
          GURL(search_url), base::UTF8ToUTF16(entry.title), entry.visit_count,
          entry.typed_count, now - TimeDelta::FromSeconds(entry.age_in_seconds),
          entry.hidden, history::SOURCE_BROWSED);
      history_service_->SetKeywordSearchTermsForURL(
          GURL(search_url), entry.search_provider->id(),
          base::UTF8ToUTF16(entry.search_terms));
      WaitForHistoryService();
    }
  }

  // Waits for history::HistoryService's async operations.
  void WaitForHistoryService() {
    history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());

    // MemoryURLIndex schedules tasks to rebuild its index on the history
    // thread. Block here to make sure they are complete.
    BlockUntilInMemoryURLIndexIsRefreshed(in_memory_url_index_.get());
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<base::RunLoop> service_run_loop_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<history::HistoryService> history_service_;
  std::unique_ptr<InMemoryURLIndex> in_memory_url_index_;
  base::ScopedTempDir history_dir_;
  std::unique_ptr<TemplateURLService> template_url_service_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_env_;
  std::unique_ptr<TestRepeatableQueriesService> service_;
  bool service_is_done_ = false;

  // RepeatableQueriesServiceObserver
  void OnRepeatableQueriesUpdated() override;
  void OnRepeatableQueriesServiceShuttingDown() override;
};

void RepeatableQueriesServiceTest::OnRepeatableQueriesUpdated() {
  service_is_done_ = true;
  if (service_run_loop_) {
    service_run_loop_->Quit();
  }
}

void RepeatableQueriesServiceTest::OnRepeatableQueriesServiceShuttingDown() {
  service_->RemoveObserver(this);
}

TEST_F(RepeatableQueriesServiceTest, SignedIn) {
  SignIn();
  test_url_loader_factory()->AddResponse(service()->GetRequestURL().spec(),
                                         GoodServerResponse());

  EXPECT_CALL(*service()->search_provider_observer(), is_google())
      .WillOnce(testing::Return(true));

  // Request a refresh.
  RefreshAndMaybeWaitForService();
  // The first two server suggestions are kept as repeatable queries.
  std::vector<RepeatableQuery> expected_server_queries{
      {base::ASCIIToUTF16("server query 1"),
       GetQueryDestinationURL("server query 1"), "/delete?server+query+1"},
      {base::ASCIIToUTF16("server query 2"),
       GetQueryDestinationURL("server query 2"), "/delete?server+query+2"}};
  EXPECT_EQ(expected_server_queries, service()->repeatable_queries());
}

TEST_F(RepeatableQueriesServiceTest, SignedIn_BadResponse) {
  SignIn();
  test_url_loader_factory()->AddResponse(service()->GetRequestURL().spec(),
                                         GoodServerResponse());

  EXPECT_CALL(*service()->search_provider_observer(), is_google())
      .WillRepeatedly(testing::Return(true));

  // Request a refresh.
  RefreshAndMaybeWaitForService();
  std::vector<RepeatableQuery> expected_server_queries{
      {base::ASCIIToUTF16("server query 1"),
       GetQueryDestinationURL("server query 1"), "/delete?server+query+1"},
      {base::ASCIIToUTF16("server query 2"),
       GetQueryDestinationURL("server query 2"), "/delete?server+query+2"}};
  EXPECT_EQ(expected_server_queries, service()->repeatable_queries());

  test_url_loader_factory()->AddResponse(service()->GetRequestURL().spec(),
                                         BadServerResponse1());

  // Request a refresh.
  RefreshAndMaybeWaitForService();
  // Cached data is cleared.
  EXPECT_TRUE(service()->repeatable_queries().empty());

  test_url_loader_factory()->AddResponse(service()->GetRequestURL().spec(),
                                         BadServerResponse2());

  // Request a refresh.
  RefreshAndMaybeWaitForService();
  // Cached data is still empty.
  EXPECT_TRUE(service()->repeatable_queries().empty());
}

TEST_F(RepeatableQueriesServiceTest, SignedIn_ErrorResponse) {
  SignIn();
  test_url_loader_factory()->AddResponse(service()->GetRequestURL().spec(),
                                         GoodServerResponse());

  EXPECT_CALL(*service()->search_provider_observer(), is_google())
      .WillRepeatedly(testing::Return(true));

  // Request a refresh.
  RefreshAndMaybeWaitForService();
  std::vector<RepeatableQuery> expected_server_queries{
      {base::ASCIIToUTF16("server query 1"),
       GetQueryDestinationURL("server query 1"), "/delete?server+query+1"},
      {base::ASCIIToUTF16("server query 2"),
       GetQueryDestinationURL("server query 2"), "/delete?server+query+2"}};
  EXPECT_EQ(expected_server_queries, service()->repeatable_queries());

  test_url_loader_factory()->AddResponse(
      service()->GetRequestURL(), network::mojom::URLResponseHead::New(),
      std::string(), network::URLLoaderCompletionStatus(net::HTTP_NOT_FOUND));

  // Request a refresh.
  RefreshAndMaybeWaitForService();
  // Cached data is kept.
  EXPECT_EQ(expected_server_queries, service()->repeatable_queries());
}

TEST_F(RepeatableQueriesServiceTest, SignedIn_DefaultSearchProviderChanged) {
  SignIn();
  test_url_loader_factory()->AddResponse(service()->GetRequestURL().spec(),
                                         GoodServerResponse());

  EXPECT_CALL(*service()->search_provider_observer(), is_google())
      .WillOnce(testing::Return(true))
      .WillOnce(testing::Return(false));

  // Request a refresh.
  RefreshAndMaybeWaitForService();
  std::vector<RepeatableQuery> expected_server_queries{
      {base::ASCIIToUTF16("server query 1"),
       GetQueryDestinationURL("server query 1"), "/delete?server+query+1"},
      {base::ASCIIToUTF16("server query 2"),
       GetQueryDestinationURL("server query 2"), "/delete?server+query+2"}};
  EXPECT_EQ(expected_server_queries, service()->repeatable_queries());

  set_service_is_done(false);
  // Simulate DSP change. Requests a refresh.
  service()->SearchProviderChanged();
  MaybeWaitForService();
  // Cached data is cleared.
  EXPECT_TRUE(service()->repeatable_queries().empty());
}

TEST_F(RepeatableQueriesServiceTest, SignedIn_SigninStatusChanged) {
  base::HistogramTester histogram_tester;

  SignIn();
  test_url_loader_factory()->AddResponse(service()->GetRequestURL().spec(),
                                         GoodServerResponse());

  EXPECT_CALL(*service()->search_provider_observer(), is_google())
      .WillRepeatedly(testing::Return(true));

  // Request a refresh.
  RefreshAndMaybeWaitForService();
  std::vector<RepeatableQuery> expected_server_queries{
      {base::ASCIIToUTF16("server query 1"),
       GetQueryDestinationURL("server query 1"), "/delete?server+query+1"},
      {base::ASCIIToUTF16("server query 2"),
       GetQueryDestinationURL("server query 2"), "/delete?server+query+2"}};
  EXPECT_EQ(expected_server_queries, service()->repeatable_queries());

  int original_query_age =
      history::kAutocompleteDuplicateVisitIntervalThreshold.InSeconds() + 3;
  FillURLDatabase({
      // Issued far enough from the original query; won't be ignored:
      {default_search_provider(), "more recent local query",
       /*age_in_seconds=*/0},
      // Issued far enough from the original query; won't be ignored:
      {default_search_provider(), "less recent local query",
       /*age_in_seconds=*/1},
      {default_search_provider(), "less recent local query",
       /*age_in_seconds=*/original_query_age},
      {default_search_provider(), "more recent local query",
       /*age_in_seconds=*/original_query_age},
  });

  set_service_is_done(false);
  SignOut();  // Requests a refresh.
  MaybeWaitForService();
  // Cached data is updated to local results.
  std::vector<RepeatableQuery> expected_local_queries{
      {base::ASCIIToUTF16("more recent local query"),
       GetQueryDestinationURL("more recent local query"), ""},
      {base::ASCIIToUTF16("less recent local query"),
       GetQueryDestinationURL("less recent local query"), ""}};
  EXPECT_EQ(expected_local_queries, service()->repeatable_queries());

  histogram_tester.ExpectTotalCount(
      RepeatableQueriesService::kExtractionDurationHistogram, 1);
  histogram_tester.ExpectTotalCount(
      RepeatableQueriesService::kExtractedCountHistogram, 1);
  histogram_tester.ExpectUniqueSample(
      RepeatableQueriesService::kExtractedCountHistogram, 2, 1);
}

TEST_F(RepeatableQueriesServiceTest, SignedIn_Deletion) {
  SignIn();
  test_url_loader_factory()->AddResponse(service()->GetRequestURL().spec(),
                                         GoodServerResponse());

  EXPECT_CALL(*service()->search_provider_observer(), is_google())
      .WillRepeatedly(testing::Return(true));

  // Request a refresh.
  RefreshAndMaybeWaitForService();
  std::vector<RepeatableQuery> expected_server_queries{
      {base::ASCIIToUTF16("server query 1"),
       GetQueryDestinationURL("server query 1"), "/delete?server+query+1"},
      {base::ASCIIToUTF16("server query 2"),
       GetQueryDestinationURL("server query 2"), "/delete?server+query+2"}};
  EXPECT_EQ(expected_server_queries, service()->repeatable_queries());

  // Try to delete a query suggestion not provided by the service.
  set_service_is_done(false);
  service()->DeleteQueryWithDestinationURL(GetQueryDestinationURL("blah"));
  // No request to delete the suggestion was sent.
  EXPECT_TRUE(test_url_loader_factory()->pending_requests()->empty());
  MaybeWaitForService();
  // Suggestions should not change.
  EXPECT_EQ(expected_server_queries, service()->repeatable_queries());

  // Delete the query suggestion provided by the service.
  set_service_is_done(false);
  service()->DeleteQueryWithDestinationURL(
      GetQueryDestinationURL("server query 1"));
  // A request to delete the suggestion was sent.
  EXPECT_EQ(1u, test_url_loader_factory()->pending_requests()->size());
  EXPECT_EQ(test_url_loader_factory()->GetPendingRequest(0)->request.url,
            service()->GetQueryDeletionURL("/delete?server+query+1"));
  MaybeWaitForService();
  expected_server_queries = {{base::ASCIIToUTF16("server query 2"),
                              GetQueryDestinationURL("server query 2"),
                              "/delete?server+query+2"}};
  // The deleted suggestion is not offered anymore.
  EXPECT_EQ(expected_server_queries, service()->repeatable_queries());

  expected_server_queries = {
      {base::ASCIIToUTF16("server query 2"),
       GetQueryDestinationURL("server query 2"), "/delete?server+query+2"},
      {base::ASCIIToUTF16("server query 3"),
       GetQueryDestinationURL("server query 3"), "/delete?server+query+3"}};
  // Request a refresh.
  RefreshAndMaybeWaitForService();
  // The deleted suggestion will not be offered again.
  EXPECT_EQ(expected_server_queries, service()->repeatable_queries());
}

TEST_F(RepeatableQueriesServiceTest, SignedOut_DefaultSearchProviderChanged) {
  int original_query_age =
      history::kAutocompleteDuplicateVisitIntervalThreshold.InSeconds() + 3;
  FillURLDatabase({
      // Issued far enough from the original query; won't be ignored:
      {default_search_provider(), "more recent local query",
       /*age_in_seconds=*/0},
      // Issued far enough from the original query; won't be ignored:
      {default_search_provider(), "less recent local query",
       /*age_in_seconds=*/1},
      {default_search_provider(), "less recent local query",
       /*age_in_seconds=*/original_query_age},
      {default_search_provider(), "more recent local query",
       /*age_in_seconds=*/original_query_age},
  });

  EXPECT_CALL(*service()->search_provider_observer(), is_google())
      .WillOnce(testing::Return(true))
      .WillOnce(testing::Return(false));

  // Request a refresh.
  RefreshAndMaybeWaitForService();
  std::vector<RepeatableQuery> expected_local_queries{
      {base::ASCIIToUTF16("more recent local query"),
       GetQueryDestinationURL("more recent local query"), ""},
      {base::ASCIIToUTF16("less recent local query"),
       GetQueryDestinationURL("less recent local query"), ""}};
  EXPECT_EQ(expected_local_queries, service()->repeatable_queries());

  set_service_is_done(false);
  // Simulate DSP change. Requests a refresh.
  service()->SearchProviderChanged();
  MaybeWaitForService();
  // Cached data is cleared.
  EXPECT_TRUE(service()->repeatable_queries().empty());
}

TEST_F(RepeatableQueriesServiceTest, SignedOut_SigninStatusChanged) {
  int original_query_age =
      history::kAutocompleteDuplicateVisitIntervalThreshold.InSeconds() + 3;
  FillURLDatabase({
      // Issued far enough from the original query; won't be ignored:
      {default_search_provider(), "more recent local query",
       /*age_in_seconds=*/0},
      // Issued far enough from the original query; won't be ignored:
      {default_search_provider(), "less recent local query",
       /*age_in_seconds=*/1},
      {default_search_provider(), "less recent local query",
       /*age_in_seconds=*/original_query_age},
      {default_search_provider(), "more recent local query",
       /*age_in_seconds=*/original_query_age},
  });

  EXPECT_CALL(*service()->search_provider_observer(), is_google())
      .WillRepeatedly(testing::Return(true));

  // Request a refresh.
  RefreshAndMaybeWaitForService();
  std::vector<RepeatableQuery> expected_local_queries{
      {base::ASCIIToUTF16("more recent local query"),
       GetQueryDestinationURL("more recent local query"), ""},
      {base::ASCIIToUTF16("less recent local query"),
       GetQueryDestinationURL("less recent local query"), ""}};
  EXPECT_EQ(expected_local_queries, service()->repeatable_queries());

  test_url_loader_factory()->AddResponse(service()->GetRequestURL().spec(),
                                         GoodServerResponse());

  set_service_is_done(false);
  SignIn();  // Requests a refresh.
  MaybeWaitForService();
  // Cached data is updated to server results.
  std::vector<RepeatableQuery> expected_server_queries{
      {base::ASCIIToUTF16("server query 1"),
       GetQueryDestinationURL("server query 1"), "/delete?server+query+1"},
      {base::ASCIIToUTF16("server query 2"),
       GetQueryDestinationURL("server query 2"), "/delete?server+query+2"}};
  EXPECT_EQ(expected_server_queries, service()->repeatable_queries());
}

TEST_F(RepeatableQueriesServiceTest, SignedOut_Deletion) {
  FillURLDatabase({{default_search_provider(), "local query 1",
                    /*age_in_seconds=*/1},
                   {default_search_provider(), "local query 2",
                    /*age_in_seconds=*/2},
                   {default_search_provider(), "local query 3",
                    /*age_in_seconds=*/3}});

  EXPECT_CALL(*service()->search_provider_observer(), is_google())
      .WillRepeatedly(testing::Return(true));

  // Request a refresh.
  RefreshAndMaybeWaitForService();
  std::vector<RepeatableQuery> expected_local_queries{
      {base::ASCIIToUTF16("local query 1"),
       GetQueryDestinationURL("local query 1"), ""},
      {base::ASCIIToUTF16("local query 2"),
       GetQueryDestinationURL("local query 2"), ""}};
  EXPECT_EQ(expected_local_queries, service()->repeatable_queries());

  // Try to delete a query suggestion not provided by the service.
  set_service_is_done(false);
  service()->DeleteQueryWithDestinationURL(GetQueryDestinationURL("blah"));
  MaybeWaitForService();
  // Suggestions should not change.
  EXPECT_EQ(expected_local_queries, service()->repeatable_queries());

  // Delete the query suggestion provided by the service.
  set_service_is_done(false);
  service()->DeleteQueryWithDestinationURL(
      GetQueryDestinationURL("local query 1"));
  MaybeWaitForService();
  expected_local_queries = {{base::ASCIIToUTF16("local query 2"),
                             GetQueryDestinationURL("local query 2"), ""}};
  // The deleted suggestion is not offered anymore.
  EXPECT_EQ(expected_local_queries, service()->repeatable_queries());

  // Request a refresh.
  RefreshAndMaybeWaitForService();
  expected_local_queries = {{base::ASCIIToUTF16("local query 2"),
                             GetQueryDestinationURL("local query 2"), ""},
                            {base::ASCIIToUTF16("local query 3"),
                             GetQueryDestinationURL("local query 3"), ""}};
  // The deleted suggestion will not be offered again.
  EXPECT_EQ(expected_local_queries, service()->repeatable_queries());
}
