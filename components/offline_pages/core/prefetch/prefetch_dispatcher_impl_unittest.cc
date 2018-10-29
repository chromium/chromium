// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_dispatcher_impl.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_event_logger.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/prefetch/fake_suggestions_provider.h"
#include "components/offline_pages/core/prefetch/generate_page_bundle_request.h"
#include "components/offline_pages/core/prefetch/get_operation_request.h"
#include "components/offline_pages/core/prefetch/mock_thumbnail_fetcher.h"
#include "components/offline_pages/core/prefetch/prefetch_background_task.h"
#include "components/offline_pages/core/prefetch/prefetch_importer_impl.h"
#include "components/offline_pages/core/prefetch/prefetch_item.h"
#include "components/offline_pages/core/prefetch/prefetch_network_request_factory.h"
#include "components/offline_pages/core/prefetch/prefetch_prefs.h"
#include "components/offline_pages/core/prefetch/prefetch_request_test_base.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "components/offline_pages/core/prefetch/prefetch_service_test_taco.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_test_util.h"
#include "components/offline_pages/core/prefetch/suggested_articles_observer.h"
#include "components/offline_pages/core/prefetch/test_download_service.h"
#include "components/offline_pages/core/prefetch/test_prefetch_network_request_factory.h"
#include "components/offline_pages/core/stub_offline_page_model.h"
#include "components/prefs/testing_pref_service.h"
#include "components/version_info/channel.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gmock_mutant.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::Contains;
using ::testing::InSequence;

namespace offline_pages {

namespace {
using testing::_;

const char kTestID[] = "id";
const GURL kTestURL("https://www.chromium.org");
const GURL kTestURL2("https://www.chromium.org/2");
const int64_t kTestOfflineID = 1111;
const char kClientID[] = "client-id-1";
const char kOperationName[] = "operation-1";
const char kBodyName[] = "body-1";
const int64_t kBodyLength = 10;
const char kBodyContent[] = "abcde12345";
const base::Time kRenderTime = base::Time::Now();

PrefetchSuggestion TestSuggestion1() {
  PrefetchSuggestion suggestion;
  suggestion.article_url = kTestURL;
  suggestion.article_title = "Article Title";
  suggestion.article_attribution = "From news.com";
  suggestion.article_snippet = "This is an article";
  suggestion.thumbnail_url = GURL("http://google.com/newsthumbnail");
  suggestion.favicon_url = GURL("http://news.com/favicon");
  return suggestion;
}

RenderPageInfo RenderInfo(const std::string& url) {
  RenderPageInfo info;
  info.url = url;
  info.redirect_url = "";
  info.status = RenderStatus::RENDERED;
  info.body_name = kBodyName;
  info.body_length = kBodyLength;
  info.render_time = kRenderTime;
  return info;
}

class MockOfflinePageModel : public StubOfflinePageModel {
 public:
  explicit MockOfflinePageModel(const base::FilePath& archive_directory) {
    SetArchiveDirectory(archive_directory);
  }
  ~MockOfflinePageModel() override = default;
  MOCK_METHOD1(StoreThumbnail, void(const OfflinePageThumbnail& thumb));
  MOCK_METHOD2(HasThumbnailForOfflineId,
               void(int64_t offline_id,
                    base::OnceCallback<void(bool)> callback));
  MOCK_METHOD2(AddPage,
               void(const OfflinePageItem& page, AddPageCallback callback));
};

class TestPrefetchBackgroundTask : public PrefetchBackgroundTask {
 public:
  TestPrefetchBackgroundTask(
      PrefetchService* service,
      base::RepeatingCallback<void(PrefetchBackgroundTaskRescheduleType)>
          callback)
      : PrefetchBackgroundTask(service), callback_(std::move(callback)) {}
  ~TestPrefetchBackgroundTask() override = default;

  void SetReschedule(PrefetchBackgroundTaskRescheduleType type) override {
    PrefetchBackgroundTask::SetReschedule(type);
    if (!callback_.is_null())
      callback_.Run(reschedule_type());
  }

 private:
  base::RepeatingCallback<void(PrefetchBackgroundTaskRescheduleType)> callback_;
};

class FakePrefetchNetworkRequestFactory
    : public TestPrefetchNetworkRequestFactory {
 public:
  FakePrefetchNetworkRequestFactory(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : TestPrefetchNetworkRequestFactory(url_loader_factory) {}

  void MakeGeneratePageBundleRequest(
      const std::vector<std::string>& prefetch_urls,
      const std::string& gcm_registration_id,
      PrefetchRequestFinishedCallback callback) override {
    // TODO(https://crbug.com/850648): Explicitly passing in a base::DoNothing()
    // callback when |respond_to_generate_page_bundle_| is set to true, to avoid
    // the |callback| being called for more than once.
    if (!respond_to_generate_page_bundle_) {
      TestPrefetchNetworkRequestFactory::MakeGeneratePageBundleRequest(
          prefetch_urls, gcm_registration_id, std::move(callback));
      return;
    } else {
      TestPrefetchNetworkRequestFactory::MakeGeneratePageBundleRequest(
          prefetch_urls, gcm_registration_id, base::DoNothing());
    }
    std::vector<RenderPageInfo> pages;
    for (const std::string& url : prefetch_urls) {
      pages.push_back(RenderInfo(url));
    }
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), PrefetchRequestStatus::kSuccess,
                       kOperationName, pages));
  }

  void set_respond_to_generate_page_bundle(bool value) {
    respond_to_generate_page_bundle_ = value;
  }

 private:
  bool respond_to_generate_page_bundle_ = false;
};
}  // namespace

class PrefetchDispatcherTest : public PrefetchRequestTestBase {
 public:
  PrefetchDispatcherTest();

  // Test implementation.
  void TearDown() override;

  // Configures the fixture for the test. Must be called before each test.
  // feed_configuration=true configures the fixture as if Feed were the
  // suggestion source. Otherwise, it's configured as if Zine was the source.
  // Zine/Feed TODO(harringtond): Collapse this method with SetUp() after Zine
  // code is removed.
  void Configure(PrefetchServiceTestTaco::SuggestionSource suggestion_source) {
    ASSERT_TRUE(archive_directory_.CreateUniqueTempDir());

    dispatcher_ = new PrefetchDispatcherImpl(&prefs_);
    network_request_factory_ =
        new FakePrefetchNetworkRequestFactory(shared_url_loader_factory());
    prefetch_prefs::RegisterPrefs(prefs_.registry());
    taco_ = std::make_unique<PrefetchServiceTestTaco>(suggestion_source);
    store_util_.BuildStore();
    taco_->SetPrefetchStore(store_util_.ReleaseStore());
    taco_->SetPrefetchDispatcher(base::WrapUnique(dispatcher_));
    taco_->SetPrefetchNetworkRequestFactory(
        base::WrapUnique(network_request_factory_));
    if (suggestion_source == PrefetchServiceTestTaco::kFeed) {
      taco_->SetThumbnailFetcher(std::unique_ptr<MockThumbnailFetcher>());

    } else {
      auto thumbnail_fetcher = std::make_unique<MockThumbnailFetcher>();
      thumbnail_fetcher_ = thumbnail_fetcher.get();
      taco_->SetThumbnailFetcher(std::move(thumbnail_fetcher));
    }

    auto model =
        std::make_unique<MockOfflinePageModel>(archive_directory_.GetPath());

    offline_model_ = model.get();
    taco_->SetOfflinePageModel(std::move(model));
    taco_->SetPrefetchImporter(std::make_unique<PrefetchImporterImpl>(
        dispatcher_, offline_model_, task_runner()));

    taco_->CreatePrefetchService();

    if (suggestion_source == PrefetchServiceTestTaco::kFeed) {
      suggestions_provider_ = std::make_unique<FakeSuggestionsProvider>();
      taco_->prefetch_service()->SetSuggestionProvider(
          suggestions_provider_.get());
    }

    ASSERT_TRUE(test_urls_.empty());
    test_urls_.push_back(
        {"1", GURL("http://testurl.com/foo"), base::string16()});
    test_urls_.push_back(
        {"2", GURL("https://testurl.com/bar"), base::string16()});
  }

  void BeginBackgroundTask();

  PrefetchBackgroundTask* GetBackgroundTask() {
    return dispatcher_->background_task_.get();
  }

  TaskQueue& GetTaskQueueFrom(PrefetchDispatcherImpl* prefetch_dispatcher) {
    return prefetch_dispatcher->task_queue_;
  }

  void SetReschedule(PrefetchBackgroundTaskRescheduleType type) {
    reschedule_called_ = true;
    reschedule_type_ = type;
  }

  void DisablePrefetchingInSettings() {
    prefetch_prefs::SetPrefetchingEnabledInSettings(&prefs_, false);
  }

  bool dispatcher_suspended() const { return dispatcher_->suspended_; }
  TaskQueue* dispatcher_task_queue() { return &dispatcher_->task_queue_; }
  PrefetchDispatcher* prefetch_dispatcher() { return dispatcher_; }
  FakePrefetchNetworkRequestFactory* network_request_factory() {
    return network_request_factory_;
  }

  bool reschedule_called() const { return reschedule_called_; }
  PrefetchBackgroundTaskRescheduleType reschedule_type_result() const {
    return reschedule_type_;
  }

  void ExpectFetchThumbnail(const std::string& thumbnail_data,
                            const bool first_attempt,
                            const char* client_id) {
    ASSERT_TRUE(thumbnail_fetcher_);  // This is null in the Feed configuration.
    EXPECT_CALL(
        *thumbnail_fetcher_,
        FetchSuggestionImageData(
            ClientId(kSuggestedArticlesNamespace, client_id), first_attempt, _))
        .WillOnce([&, thumbnail_data](
                      const ClientId& client_id, bool first_attempt,
                      ThumbnailFetcher::ImageDataFetchedCallback callback) {
          task_runner()->PostTask(
              FROM_HERE, base::BindOnce(std::move(callback), thumbnail_data));
        });
  }

  void ExpectHasThumbnailForOfflineId(int64_t offline_id, bool to_return) {
    EXPECT_CALL(*offline_model_, HasThumbnailForOfflineId(offline_id, _))
        .WillOnce([&, to_return](int64_t offline_id,
                                 base::OnceCallback<void(bool)> callback) {
          task_runner()->PostTask(
              FROM_HERE, base::BindOnce(std::move(callback), to_return));
        });
  }

  PrefetchDispatcherImpl* dispatcher() { return dispatcher_; }
  PrefetchService* prefetch_service() { return taco_->prefetch_service(); }
  TestDownloadService* download_service() { return taco_->download_service(); }

 protected:
  // Owned by |taco_|.
  MockOfflinePageModel* offline_model_;

  std::vector<PrefetchURL> test_urls_;

  // Owned by |taco_|, may be null.
  MockThumbnailFetcher* thumbnail_fetcher_;

  PrefetchStoreTestUtil store_util_{task_runner()};
  base::ScopedTempDir archive_directory_;
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<FakeSuggestionsProvider> suggestions_provider_;

 private:
  std::unique_ptr<PrefetchServiceTestTaco> taco_;

  base::test::ScopedFeatureList feature_list_;

  // Owned by |taco_|.
  PrefetchDispatcherImpl* dispatcher_;
  // Owned by |taco_|.
  FakePrefetchNetworkRequestFactory* network_request_factory_;

  bool reschedule_called_ = false;
  PrefetchBackgroundTaskRescheduleType reschedule_type_ =
      PrefetchBackgroundTaskRescheduleType::NO_RESCHEDULE;
};

PrefetchDispatcherTest::PrefetchDispatcherTest() {
  feature_list_.InitAndEnableFeature(kPrefetchingOfflinePagesFeature);
}

void PrefetchDispatcherTest::TearDown() {
  // Ensures that the task is stopped first.
  dispatcher_->StopBackgroundTask();
  RunUntilIdle();

  // Ensures that the store is properly disposed off.
  taco_.reset();
  RunUntilIdle();
}

void PrefetchDispatcherTest::BeginBackgroundTask() {
  dispatcher_->BeginBackgroundTask(std::make_unique<TestPrefetchBackgroundTask>(
      taco_->prefetch_service(),
      base::BindRepeating(&PrefetchDispatcherTest::SetReschedule,
                          base::Unretained(this))));
}

MATCHER(ValidThumbnail, "") {
  return arg.offline_id == kTestOfflineID && !arg.thumbnail.empty();
}

TEST_F(PrefetchDispatcherTest, DispatcherDoesNotCrash) {
  Configure(PrefetchServiceTestTaco::kContentSuggestions);

  // TODO(https://crbug.com/735254): Ensure that Dispatcher unit test keep up
  // with the state of adding tasks, and that the end state is we have tests
  // that verify the proper tasks were added in the proper order at each wakeup
  // signal of the dispatcher.
  prefetch_dispatcher()->AddCandidatePrefetchURLs(kSuggestedArticlesNamespace,
                                                  test_urls_);
  prefetch_dispatcher()->RemoveAllUnprocessedPrefetchURLs(
      kSuggestedArticlesNamespace);
  prefetch_dispatcher()->RemovePrefetchURLsByClientId(
      {kSuggestedArticlesNamespace, "123"});
}

TEST_F(PrefetchDispatcherTest, AddCandidatePrefetchURLsTask) {
  Configure(PrefetchServiceTestTaco::kContentSuggestions);

  prefetch_dispatcher()->AddCandidatePrefetchURLs(kSuggestedArticlesNamespace,
                                                  test_urls_);
  EXPECT_TRUE(dispatcher_task_queue()->HasPendingTasks());
  RunUntilIdle();
  EXPECT_FALSE(dispatcher_task_queue()->HasPendingTasks());
  EXPECT_FALSE(dispatcher_task_queue()->HasRunningTask());
}

TEST_F(PrefetchDispatcherTest, RemovePrefetchURLsByClientId) {
  Configure(PrefetchServiceTestTaco::kContentSuggestions);

  prefetch_dispatcher()->AddCandidatePrefetchURLs(kSuggestedArticlesNamespace,
                                                  test_urls_);
  RunUntilIdle();
  prefetch_dispatcher()->RemovePrefetchURLsByClientId(
      ClientId(kSuggestedArticlesNamespace, test_urls_.front().id));
  EXPECT_TRUE(dispatcher_task_queue()->HasPendingTasks());
  RunUntilIdle();
  EXPECT_FALSE(dispatcher_task_queue()->HasPendingTasks());
  EXPECT_FALSE(dispatcher_task_queue()->HasRunningTask());
}

TEST_F(PrefetchDispatcherTest, ZineDispatcherDoesNothingIfFeatureNotEnabled) {
  Configure(PrefetchServiceTestTaco::kContentSuggestions);

  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(kPrefetchingOfflinePagesFeature);

  // Don't add a task for new prefetch URLs.
  prefetch_dispatcher()->AddCandidatePrefetchURLs(kSuggestedArticlesNamespace,
                                                  test_urls_);
  EXPECT_FALSE(dispatcher_task_queue()->HasRunningTask());

  // Do nothing with a new background task.
  BeginBackgroundTask();
  EXPECT_EQ(nullptr, GetBackgroundTask());

  // TODO(carlosk): add more checks here.
}

TEST_F(PrefetchDispatcherTest, FeedDispatcherDoesNothingIfFeatureNotEnabled) {
  Configure(PrefetchServiceTestTaco::kFeed);
  suggestions_provider_->SetSuggestions({TestSuggestion1()});

  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(kPrefetchingOfflinePagesFeature);

  // Don't add a task for new prefetch URLs.
  prefetch_service()->NewSuggestionsAvailable();
  EXPECT_FALSE(dispatcher_task_queue()->HasRunningTask());

  // Do nothing with a new background task.
  BeginBackgroundTask();
  EXPECT_EQ(nullptr, GetBackgroundTask());

  // TODO(carlosk): add more checks here.
}

TEST_F(PrefetchDispatcherTest,
       ZineDispatcherDoesNothingIfSettingsDoNotAllowIt) {
  Configure(PrefetchServiceTestTaco::kContentSuggestions);

  DisablePrefetchingInSettings();

  // Don't add a task for new prefetch URLs.
  prefetch_dispatcher()->AddCandidatePrefetchURLs(kSuggestedArticlesNamespace,
                                                  test_urls_);
  EXPECT_FALSE(dispatcher_task_queue()->HasRunningTask());

  // Do nothing with a new background task.
  BeginBackgroundTask();
  EXPECT_EQ(nullptr, GetBackgroundTask());

  // TODO(carlosk): add more checks here.
}

TEST_F(PrefetchDispatcherTest,
       FeedDispatcherDoesNothingIfSettingsDoNotAllowIt) {
  Configure(PrefetchServiceTestTaco::kFeed);
  suggestions_provider_->SetSuggestions({TestSuggestion1()});

  DisablePrefetchingInSettings();

  // Don't add a task for new prefetch URLs.
  prefetch_service()->NewSuggestionsAvailable();
  EXPECT_FALSE(dispatcher_task_queue()->HasRunningTask());

  // Do nothing with a new background task.
  BeginBackgroundTask();
  EXPECT_EQ(nullptr, GetBackgroundTask());

  // TODO(carlosk): add more checks here.
}

TEST_F(PrefetchDispatcherTest, DispatcherReleasesBackgroundTask) {
  Configure(PrefetchServiceTestTaco::kContentSuggestions);

  PrefetchURL prefetch_url(kTestID, kTestURL, base::string16());
  prefetch_dispatcher()->AddCandidatePrefetchURLs(
      kSuggestedArticlesNamespace, std::vector<PrefetchURL>(1, prefetch_url));
  RunUntilIdle();

  // We start the background task, causing reconcilers and action tasks to be
  // run. We should hold onto the background task until there is no more work to
  // do, after the network request ends.
  ASSERT_EQ(nullptr, GetBackgroundTask());
  BeginBackgroundTask();
  EXPECT_TRUE(dispatcher_task_queue()->HasRunningTask());
  RunUntilIdle();

  // Still holding onto the background task.
  EXPECT_NE(nullptr, GetBackgroundTask());
  EXPECT_FALSE(dispatcher_task_queue()->HasRunningTask());
  EXPECT_THAT(*network_request_factory()->GetAllUrlsRequested(),
              Contains(prefetch_url.url.spec()));

  // When the network request finishes, the dispatcher should still hold the
  // ScopedBackgroundTask because it needs to process the results of the
  // request.
  RespondWithHttpError(net::HTTP_INTERNAL_SERVER_ERROR);
  EXPECT_NE(nullptr, GetBackgroundTask());
  RunUntilIdle();

  // Because there is no work remaining, the background task should be released.
  EXPECT_EQ(nullptr, GetBackgroundTask());
}

TEST_F(PrefetchDispatcherTest, RetryWithBackoffAfterFailedNetworkRequest) {
  Configure(PrefetchServiceTestTaco::kContentSuggestions);

  PrefetchURL prefetch_url(kTestID, kTestURL, base::string16());
  prefetch_dispatcher()->AddCandidatePrefetchURLs(
      kSuggestedArticlesNamespace, std::vector<PrefetchURL>(1, prefetch_url));
  RunUntilIdle();

  BeginBackgroundTask();
  RunUntilIdle();

  // Trigger another request to make sure we have more work to do.
  PrefetchURL prefetch_url2(kTestID, kTestURL2, base::string16());
  prefetch_dispatcher()->AddCandidatePrefetchURLs(
      kSuggestedArticlesNamespace, std::vector<PrefetchURL>(1, prefetch_url2));
  RunUntilIdle();

  // This should trigger retry with backoff.
  RespondWithHttpError(net::HTTP_INTERNAL_SERVER_ERROR);
  RunUntilIdle();

  EXPECT_TRUE(reschedule_called());
  EXPECT_EQ(PrefetchBackgroundTaskRescheduleType::RESCHEDULE_WITH_BACKOFF,
            reschedule_type_result());

  EXPECT_FALSE(dispatcher_suspended());

  // There are still outstanding requests.
  EXPECT_TRUE(network_request_factory()->HasOutstandingRequests());

  // Still holding onto the background task.
  EXPECT_NE(nullptr, GetBackgroundTask());
}

TEST_F(PrefetchDispatcherTest, RetryWithoutBackoffAfterFailedNetworkRequest) {
  Configure(PrefetchServiceTestTaco::kContentSuggestions);

  PrefetchURL prefetch_url(kTestID, kTestURL, base::string16());
  prefetch_dispatcher()->AddCandidatePrefetchURLs(
      kSuggestedArticlesNamespace, std::vector<PrefetchURL>(1, prefetch_url));
  RunUntilIdle();

  BeginBackgroundTask();
  RunUntilIdle();

  // Trigger another request to make sure we have more work to do.
  PrefetchURL prefetch_url2(kTestID, kTestURL2, base::string16());
  prefetch_dispatcher()->AddCandidatePrefetchURLs(
      kSuggestedArticlesNamespace, std::vector<PrefetchURL>(1, prefetch_url2));

  // This should trigger retry without backoff.
  RespondWithNetError(net::ERR_CONNECTION_CLOSED);
  RunUntilIdle();

  EXPECT_TRUE(reschedule_called());
  EXPECT_EQ(PrefetchBackgroundTaskRescheduleType::RESCHEDULE_WITHOUT_BACKOFF,
            reschedule_type_result());

  EXPECT_FALSE(dispatcher_suspended());

  // There are still outstanding requests.
  EXPECT_TRUE(network_request_factory()->HasOutstandingRequests());

  // Still holding onto the background task.
  EXPECT_NE(nullptr, GetBackgroundTask());
}

TEST_F(PrefetchDispatcherTest, SuspendAfterFailedNetworkRequest) {
  Configure(PrefetchServiceTestTaco::kContentSuggestions);
  PrefetchURL prefetch_url(kTestID, kTestURL, base::string16());
  prefetch_dispatcher()->AddCandidatePrefetchURLs(
      kSuggestedArticlesNamespace, std::vector<PrefetchURL>(1, prefetch_url));
  RunUntilIdle();

  BeginBackgroundTask();
  RunUntilIdle();

  // Trigger another request to make sure we have more work to do.
  PrefetchURL prefetch_url2(kTestID, kTestURL2, base::string16());
  prefetch_dispatcher()->AddCandidatePrefetchURLs(
      kSuggestedArticlesNamespace, std::vector<PrefetchURL>(1, prefetch_url2));

  EXPECT_FALSE(dispatcher_suspended());

  // This should trigger suspend.
  RespondWithNetError(net::ERR_BLOCKED_BY_ADMINISTRATOR);
  RunUntilIdle();

  EXPECT_TRUE(reschedule_called());
  EXPECT_EQ(PrefetchBackgroundTaskRescheduleType::SUSPEND,
            reschedule_type_result());

  // The dispatcher should be suspended.
  EXPECT_TRUE(dispatcher_suspended());

  // The 2nd request will not be created because the prefetch dispatcher
  // pipeline is in suspended state and will not queue any new tasks.
  EXPECT_FALSE(network_request_factory()->HasOutstandingRequests());

  // The background task should finally be released due to suspension.
  EXPECT_EQ(nullptr, GetBackgroundTask());
}

TEST_F(PrefetchDispatcherTest, SuspendRemovedAfterNewBackgroundTask) {
  Configure(PrefetchServiceTestTaco::kContentSuggestions);

  PrefetchURL prefetch_url(kTestID, kTestURL, base::string16());
  prefetch_dispatcher()->AddCandidatePrefetchURLs(
      kSuggestedArticlesNamespace, std::vector<PrefetchURL>(1, prefetch_url));
  RunUntilIdle();

  BeginBackgroundTask();
  RunUntilIdle();

  // This should trigger suspend.
  RespondWithNetError(net::ERR_BLOCKED_BY_ADMINISTRATOR);
  RunUntilIdle();

  EXPECT_TRUE(reschedule_called());
  EXPECT_EQ(PrefetchBackgroundTaskRescheduleType::SUSPEND,
            reschedule_type_result());

  // The dispatcher should be suspended.
  EXPECT_TRUE(dispatcher_suspended());

  // No task is in the queue.
  EXPECT_FALSE(dispatcher_task_queue()->HasPendingTasks());

  // The background task should finally be released due to suspension.
  EXPECT_EQ(nullptr, GetBackgroundTask());

  // Trigger another request to make sure we have more work to do.
  PrefetchURL prefetch_url2(kTestID, kTestURL2, base::string16());
  prefetch_dispatcher()->AddCandidatePrefetchURLs(
      kSuggestedArticlesNamespace, std::vector<PrefetchURL>(1, prefetch_url2));

  BeginBackgroundTask();

  // The suspended state should be reset.
  EXPECT_FALSE(dispatcher_suspended());

  // Some reconcile and action tasks should be created.
  EXPECT_TRUE(dispatcher_task_queue()->HasPendingTasks());
}

TEST_F(PrefetchDispatcherTest, ZineNoNetworkRequestsAfterNewURLs) {
  Configure(PrefetchServiceTestTaco::kContentSuggestions);

  PrefetchURL prefetch_url(kTestID, kTestURL, base::string16());
  prefetch_dispatcher()->AddCandidatePrefetchURLs(
      kSuggestedArticlesNamespace, std::vector<PrefetchURL>(1, prefetch_url));
  RunUntilIdle();

  // We should not have started GPB
  EXPECT_EQ(nullptr, GetPendingRequest());
}

TEST_F(PrefetchDispatcherTest, FeedNoNetworkRequestsAfterNewURLs) {
  Configure(PrefetchServiceTestTaco::kFeed);
  suggestions_provider_->SetSuggestions({TestSuggestion1()});

  PrefetchURL prefetch_url(kTestID, kTestURL, base::string16());
  prefetch_service()->NewSuggestionsAvailable();
  RunUntilIdle();

  // We should not have started GPB
  EXPECT_EQ(nullptr, GetPendingRequest());
}

TEST_F(PrefetchDispatcherTest, ThumbnailFetchFailure_ItemDownloaded) {
  Configure(PrefetchServiceTestTaco::kContentSuggestions);

  ExpectFetchThumbnail("", false, kClientID);
  ExpectHasThumbnailForOfflineId(kTestOfflineID, false);
  EXPECT_CALL(*offline_model_, StoreThumbnail(_)).Times(0);
  prefetch_dispatcher()->ItemDownloaded(
      kTestOfflineID, ClientId(kSuggestedArticlesNamespace, kClientID));
}

TEST_F(PrefetchDispatcherTest, ThumbnailFetchSuccess_ItemDownloaded) {
  Configure(PrefetchServiceTestTaco::kContentSuggestions);

  std::string kThumbnailData = "abc";
  ExpectHasThumbnailForOfflineId(kTestOfflineID, false);
  EXPECT_CALL(*offline_model_, StoreThumbnail(ValidThumbnail()));
  ExpectFetchThumbnail(kThumbnailData, false, kClientID);
  prefetch_dispatcher()->ItemDownloaded(
      kTestOfflineID, ClientId(kSuggestedArticlesNamespace, kClientID));
}

TEST_F(PrefetchDispatcherTest, ThumbnailAlreadyExists_ItemDownloaded) {
  Configure(PrefetchServiceTestTaco::kContentSuggestions);

  ExpectHasThumbnailForOfflineId(kTestOfflineID, true);
  EXPECT_CALL(*thumbnail_fetcher_, FetchSuggestionImageData(_, _, _)).Times(0);
  EXPECT_CALL(*offline_model_, StoreThumbnail(_)).Times(0);
  prefetch_dispatcher()->ItemDownloaded(
      kTestOfflineID, ClientId(kSuggestedArticlesNamespace, kClientID));
}

TEST_F(PrefetchDispatcherTest,
       ThumbnailVariousCases_GeneratePageBundleRequested) {
  Configure(PrefetchServiceTestTaco::kContentSuggestions);

  // Covers all possible thumbnail cases with a single
  // GeneratePageBundleRequested call: fetch succeeds (#1), fetch fails (#2),
  // item already exists (#3).
  const int64_t kTestOfflineID1 = 100;
  const int64_t kTestOfflineID2 = 101;
  const int64_t kTestOfflineID3 = 102;
  const char kClientID1[] = "a";
  const char kClientID2[] = "b";
  const char kClientID3[] = "c";

  InSequence in_sequence;
  // Case #1.
  ExpectHasThumbnailForOfflineId(kTestOfflineID1, false);
  ExpectFetchThumbnail("abc", true, kClientID1);
  EXPECT_CALL(*offline_model_, StoreThumbnail(_)).Times(1);
  // Case #2.
  ExpectHasThumbnailForOfflineId(kTestOfflineID2, false);
  ExpectFetchThumbnail("", true, kClientID2);
  // Case #3.
  ExpectHasThumbnailForOfflineId(kTestOfflineID3, true);

  auto prefetch_item_ids = std::make_unique<PrefetchDispatcher::IdsVector>();
  prefetch_item_ids->emplace_back(
      kTestOfflineID1, ClientId(kSuggestedArticlesNamespace, kClientID1));
  prefetch_item_ids->emplace_back(
      kTestOfflineID2, ClientId(kSuggestedArticlesNamespace, kClientID2));
  prefetch_item_ids->emplace_back(
      kTestOfflineID3, ClientId(kSuggestedArticlesNamespace, kClientID3));
  prefetch_dispatcher()->GeneratePageBundleRequested(
      std::move(prefetch_item_ids));
}

// Runs through the entire lifecycle of a successful prefetch item,
// from GeneratePageBundle, GetOperation, download, import, and completion.
TEST_F(PrefetchDispatcherTest, ZinePrefetchItemFlow) {
  Configure(PrefetchServiceTestTaco::kContentSuggestions);

  auto get_item = [&]() {
    std::set<PrefetchItem> items;
    EXPECT_EQ(1ul, store_util_.GetAllItems(&items));
    return *items.begin();
  };
  // The page should be added to the offline model. Return success through the
  // callback, and store the page to added_page.
  OfflinePageItem added_page;
  EXPECT_CALL(*offline_model_, AddPage(_, _))
      .WillOnce([&](const OfflinePageItem& page, AddPageCallback callback) {
        added_page = page;
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback),
                                      AddPageResult::SUCCESS, page.offline_id));
      });

  network_request_factory()->set_respond_to_generate_page_bundle(true);
  download_service()->SetTestFileData(kBodyContent);
  std::vector<PrefetchURL> prefetch_urls;
  prefetch_urls.push_back(PrefetchURL("url-1", GURL("http://www.url1.com"),
                                      base::ASCIIToUTF16("URL 1")));

  // Kick off the request.
  dispatcher()->AddCandidatePrefetchURLs(kSuggestedArticlesNamespace,
                                         prefetch_urls);

  // Run the pipeline to completion.
  RunUntilIdle();
  PrefetchItem state1 = get_item();
  BeginBackgroundTask();
  RunUntilIdle();
  PrefetchItem state2 = get_item();
  BeginBackgroundTask();
  RunUntilIdle();
  PrefetchItem state3 = get_item();

  // Check progression of item state. Log the states to help explain any failed
  // expectations.
  SCOPED_TRACE(testing::Message() << "\nstate1: " << state1 << "\nstate2: "
                                  << state2 << "\nstate3: " << state3);

  // State 1.
  EXPECT_EQ(PrefetchItemState::NEW_REQUEST, state1.state);

  // State 2.
  EXPECT_EQ(PrefetchItemState::FINISHED, state2.state);
  EXPECT_FALSE(state2.file_path.empty());
  EXPECT_FALSE(added_page.file_path.empty());
  std::string imported_file_contents;
  EXPECT_TRUE(
      base::ReadFileToString(added_page.file_path, &imported_file_contents));
  EXPECT_EQ(kBodyContent, imported_file_contents);
  EXPECT_EQ(kBodyLength, state2.file_size);

  // State 3.
  EXPECT_EQ(PrefetchItemState::ZOMBIE, state3.state);
  EXPECT_EQ(PrefetchItemErrorCode::SUCCESS, state3.error_code);
}

// This is the same as PrefetchItemFlow, but with the Feed configuration.
TEST_F(PrefetchDispatcherTest, FeedPrefetchItemFlow) {
  Configure(PrefetchServiceTestTaco::kFeed);

  network_request_factory()->set_respond_to_generate_page_bundle(true);
  download_service()->SetTestFileData(kBodyContent);

  // Mock AddPage so that importing succeeds.
  EXPECT_CALL(*offline_model_, AddPage(_, _))
      .WillOnce([&](const OfflinePageItem& page, AddPageCallback callback) {
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback),
                                      AddPageResult::SUCCESS, page.offline_id));
      });

  suggestions_provider_->SetSuggestions({TestSuggestion1()});
  prefetch_service()->NewSuggestionsAvailable();
  RunUntilIdle();
  EXPECT_FALSE(dispatcher_task_queue()->HasPendingTasks());
  EXPECT_FALSE(dispatcher_task_queue()->HasRunningTask());

  RunUntilIdle();
  BeginBackgroundTask();
  RunUntilIdle();
  BeginBackgroundTask();
  RunUntilIdle();

  std::set<PrefetchItem> items;
  store_util_.GetAllItems(&items);
  EXPECT_EQ(1ul, items.size());
  const PrefetchItem item = *items.begin();

  EXPECT_EQ(base::UTF8ToUTF16(TestSuggestion1().article_title), item.title);
  EXPECT_EQ(TestSuggestion1().article_url, item.url);
  EXPECT_EQ(PrefetchItemState::ZOMBIE, item.state);
  EXPECT_EQ(PrefetchItemErrorCode::SUCCESS, item.error_code);
}

}  // namespace offline_pages
