// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_dispatcher_impl.h"

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop_current.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/image_fetcher/core/mock_image_fetcher.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_event_logger.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "components/offline_pages/core/prefetch/fake_suggestions_provider.h"
#include "components/offline_pages/core/prefetch/generate_page_bundle_request.h"
#include "components/offline_pages/core/prefetch/get_operation_request.h"
#include "components/offline_pages/core/prefetch/mock_prefetch_item_generator.h"
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
#include "components/version_info/channel.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
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
const char kThumbnailUrl[] = "http://www.thumbnail.com/";
const char kThumbnailData[] = "thumbnail_data";
const char kFaviconData[] = "favicon_data";
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

PrefetchSuggestion TestSuggestion2() {
  PrefetchSuggestion suggestion;
  suggestion.article_url = kTestURL2;
  suggestion.article_title = "Second Title";
  suggestion.article_attribution = "From fun.com";
  suggestion.article_snippet = "More fun stuff";
  suggestion.thumbnail_url = GURL("http://google.com/funthumbnail");
  suggestion.favicon_url = GURL("http://fun.com/favicon");
  return suggestion;
}

PrefetchSuggestion TestSuggestion3() {
  PrefetchSuggestion suggestion;
  suggestion.article_url = GURL("http://www.google.com/3");
  suggestion.article_title = "Third Title";
  suggestion.article_attribution = "From google.com";
  suggestion.article_snippet = "I'm feeling lucky";
  suggestion.thumbnail_url = GURL("http://google.com/googlethumbnail");
  suggestion.favicon_url = GURL("http://google.com/favicon");
  return suggestion;
}

PrefetchSuggestion TestSuggestion4() {
  PrefetchSuggestion suggestion;
  suggestion.article_url = GURL("http://www.four.com");
  suggestion.article_title = "Fourth title";
  suggestion.article_attribution = "From four.com";
  suggestion.article_snippet = "I'm four";
  suggestion.thumbnail_url = GURL("http://google.com/fourthumbnail");
  suggestion.favicon_url = GURL("http://four.com/favicon");
  return suggestion;
}

PrefetchSuggestion TestSuggestion5() {
  PrefetchSuggestion suggestion;
  suggestion.article_url = GURL("http://www.five.com");
  suggestion.article_title = "Fifth title";
  suggestion.article_attribution = "From five.com";
  suggestion.article_snippet = "I'm five";
  suggestion.thumbnail_url = GURL("http://google.com/fivethumbnail");
  suggestion.favicon_url = GURL("http://five.com/favicon");
  return suggestion;
}

ClientId SuggestionClientId(const PrefetchSuggestion& suggestion) {
  return {kSuggestedArticlesNamespace, suggestion.article_url.spec()};
}

const PrefetchItem* FindByUrl(const std::set<PrefetchItem>& items,
                              const GURL& url) {
  for (const auto& item : items) {
    if (item.url == url)
      return &item;
  }
  return nullptr;
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

OfflinePageVisuals FakeVisuals(int64_t offline_id) {
  return OfflinePageVisuals(offline_id, kRenderTime, kThumbnailData,
                            kFaviconData);
}

// This class is a mix between a mock and fake.
class MockOfflinePageModel : public StubOfflinePageModel {
 public:
  explicit MockOfflinePageModel(const base::FilePath& archive_directory) {
    SetArchiveDirectory(archive_directory);
  }
  ~MockOfflinePageModel() override = default;

  // OfflinePageModel implementation.

  MOCK_METHOD2(AddPage,
               void(const OfflinePageItem& page, AddPageCallback callback));
  MOCK_METHOD2(DeletePagesWithCriteria,
               void(const PageCriteria& criteria, DeletePageCallback callback));

  void StoreThumbnail(int64_t offline_id, std::string thumbnail) override {
    insert_or_update_visuals(offline_id, thumbnail, std::string());
  }

  void StoreFavicon(int64_t offline_id, std::string favicon) override {
    insert_or_update_visuals(offline_id, std::string(), favicon);
  }

  void GetVisualsAvailability(
      int64_t offline_id,
      base::OnceCallback<void(VisualsAvailability)> callback) override {
    has_thumbnail_for_offline_id_calls_.insert(offline_id);

    VisualsAvailability availability = {false, false};
    if (visuals_.count(offline_id) > 0) {
      const OfflinePageVisuals& visuals = visuals_[offline_id];
      availability.has_thumbnail = !visuals.thumbnail.empty();
      availability.has_favicon = !visuals.favicon.empty();
    }

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), availability));
  }

  // Returns the thumbnails stored with StoreThumbnail.
  const std::unordered_map<int64_t, OfflinePageVisuals> visuals() const {
    return visuals_;
  }

  const OfflinePageVisuals* FindVisuals(int64_t offline_id) const {
    if (visuals_.count(offline_id) == 0)
      return nullptr;
    return &visuals_.at(offline_id);
  }

  void set_visuals(std::unordered_map<int64_t, OfflinePageVisuals> visuals) {
    visuals_ = std::move(visuals);
  }

  const std::set<int64_t>& has_thumbnail_for_offline_id_calls() const {
    return has_thumbnail_for_offline_id_calls_;
  }

 private:
  void insert_or_update_visuals(const int64_t offline_id,
                                const std::string& thumbnail,
                                const std::string& favicon) {
    OfflinePageVisuals& new_or_existing = visuals_[offline_id];
    new_or_existing.offline_id = offline_id;
    new_or_existing.expiration = kRenderTime;
    if (!thumbnail.empty())
      new_or_existing.thumbnail = thumbnail;
    if (!favicon.empty())
      new_or_existing.favicon = favicon;
  }

  std::unordered_map<int64_t, OfflinePageVisuals> visuals_;
  std::set<int64_t> has_thumbnail_for_offline_id_calls_;
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
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService* prefs)
      : TestPrefetchNetworkRequestFactory(url_loader_factory, prefs) {}

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
  // Zine/Feed TODO(harringtond): Collapse this method with SetUp() after Zine
  // code is removed.
  void Configure(PrefetchServiceTestTaco::SuggestionSource suggestion_source) {
    ASSERT_TRUE(archive_directory_.CreateUniqueTempDir());

    taco_ = std::make_unique<PrefetchServiceTestTaco>(suggestion_source);
    prefetch_prefs::SetEnabledByServer(taco_->pref_service(), true);
    prefetch_prefs::SetCachedPrefetchGCMToken(taco_->pref_service(),
                                              "dummy_gcm_token");
    dispatcher_ = new PrefetchDispatcherImpl(taco_->pref_service());
    network_request_factory_ = new FakePrefetchNetworkRequestFactory(
        shared_url_loader_factory(), taco_->pref_service());
    store_util_.BuildStore();
    taco_->SetPrefetchStore(store_util_.ReleaseStore());
    taco_->SetPrefetchDispatcher(base::WrapUnique(dispatcher_));
    taco_->SetPrefetchNetworkRequestFactory(
        base::WrapUnique(network_request_factory_));
    if (suggestion_source == PrefetchServiceTestTaco::kFeed) {
      auto image_fetcher = std::make_unique<image_fetcher::MockImageFetcher>();
      thumbnail_image_fetcher_ = image_fetcher.get();
      taco_->SetThumbnailImageFetcher(std::move(image_fetcher));
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
        dispatcher_, offline_model_, base::ThreadTaskRunnerHandle::Get()));

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
    prefetch_prefs::SetPrefetchingEnabledInSettings(taco_->pref_service(),
                                                    false);
  }

  bool dispatcher_suspended() const { return dispatcher_->suspended_; }
  TaskQueue* dispatcher_task_queue() { return &dispatcher_->task_queue_; }
  PrefetchDispatcher* prefetch_dispatcher() { return dispatcher_; }
  FakePrefetchNetworkRequestFactory* network_request_factory() {
    return network_request_factory_;
  }
  PrefService* prefs() { return taco_->pref_service(); }

  bool reschedule_called() const { return reschedule_called_; }
  PrefetchBackgroundTaskRescheduleType reschedule_type_result() const {
    return reschedule_type_;
  }

  void ExpectFetchThumbnail(const std::string& thumbnail_data,
                            const char* client_id) {
    ASSERT_TRUE(thumbnail_fetcher_);  // This is null in the Feed configuration.
    EXPECT_CALL(*thumbnail_fetcher_,
                FetchSuggestionImageData(
                    ClientId(kSuggestedArticlesNamespace, client_id), _))
        .WillOnce([&, thumbnail_data](
                      const ClientId& client_id,
                      ThumbnailFetcher::ImageDataFetchedCallback callback) {
          base::ThreadTaskRunnerHandle::Get()->PostTask(
              FROM_HERE, base::BindOnce(std::move(callback), thumbnail_data));
        });
  }

  void ExpectFetchThumbnailImage(const std::string& thumbnail_data,
                                 const GURL& thumbnail_url) {
    ASSERT_TRUE(thumbnail_image_fetcher_) << "Not configured in kFeed mode";
    EXPECT_CALL(*thumbnail_image_fetcher_,
                FetchImageAndData_(thumbnail_url, _, _, _))
        .WillOnce([=](const GURL& image_url,
                      image_fetcher::ImageDataFetcherCallback* data_callback,
                      image_fetcher::ImageFetcherCallback* image_callback,
                      image_fetcher::ImageFetcherParams params) {
          ASSERT_TRUE(image_callback->is_null());
          std::move(*data_callback)
              .Run(thumbnail_data, image_fetcher::RequestMetadata());
        });
  }

  void ExpectFetchFaviconImage(const std::string& favicon_data,
                               const GURL& favicon_url) {
    ASSERT_TRUE(thumbnail_image_fetcher_) << "Not configured in kFeed mode";
    EXPECT_CALL(*thumbnail_image_fetcher_,
                FetchImageAndData_(favicon_url, _, _, _))
        .WillOnce([=](const GURL& image_url,
                      image_fetcher::ImageDataFetcherCallback* data_callback,
                      image_fetcher::ImageFetcherCallback* image_callback,
                      image_fetcher::ImageFetcherParams params) {
          ASSERT_TRUE(image_callback->is_null());
          std::move(*data_callback)
              .Run(favicon_data, image_fetcher::RequestMetadata());
        });
  }

  PrefetchDispatcherImpl* dispatcher() { return dispatcher_; }
  PrefetchService* prefetch_service() { return taco_->prefetch_service(); }
  TestDownloadService* download_service() { return taco_->download_service(); }

  // Asserts that there exists a single item in the database, and returns it.
  PrefetchItem GetSingleItem() {
    std::set<PrefetchItem> items;
    EXPECT_EQ(1ul, store_util_.GetAllItems(&items));
    return *items.begin();
  }

 protected:
  // Owned by |taco_|.
  MockOfflinePageModel* offline_model_;

  std::vector<PrefetchURL> test_urls_;

  // Owned by |taco_|, may be null.
  MockThumbnailFetcher* thumbnail_fetcher_;
  image_fetcher::MockImageFetcher* thumbnail_image_fetcher_;

  PrefetchStoreTestUtil store_util_;
  MockPrefetchItemGenerator item_generator_;
  base::ScopedTempDir archive_directory_;
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
  CHECK(taco_->pref_service());
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

TEST_F(PrefetchDispatcherTest, DisabledInSettings) {
  Configure(PrefetchServiceTestTaco::kContentSuggestions);
  prefetch_prefs::SetPrefetchingEnabledInSettings(prefs(), false);
  prefetch_dispatcher()->AddCandidatePrefetchURLs(kSuggestedArticlesNamespace,
                                                  test_urls_);
  EXPECT_FALSE(dispatcher_task_queue()->HasPendingTasks());
}

TEST_F(PrefetchDispatcherTest, DisabledByServer) {
  Configure(PrefetchServiceTestTaco::kContentSuggestions);
  prefetch_prefs::SetPrefetchingEnabledInSettings(prefs(), true);
  prefetch_prefs::SetEnabledByServer(prefs(), false);
  prefetch_dispatcher()->AddCandidatePrefetchURLs(kSuggestedArticlesNamespace,
                                                  test_urls_);
  EXPECT_FALSE(dispatcher_task_queue()->HasPendingTasks());
}

TEST_F(PrefetchDispatcherTest, ForbiddenCheckDue) {
  Configure(PrefetchServiceTestTaco::kContentSuggestions);
  prefetch_prefs::SetPrefetchingEnabledInSettings(prefs(), true);
  prefetch_prefs::SetEnabledByServer(prefs(), false);
  prefetch_prefs::ResetForbiddenStateForTesting(prefs());
  prefetch_dispatcher()->AddCandidatePrefetchURLs(kSuggestedArticlesNamespace,
                                                  test_urls_);
  EXPECT_FALSE(dispatcher_task_queue()->HasPendingTasks());
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

  // We want to make sure the response is received before the dispatcher goes
  // for the next task. For that we need to make sure that only file handle
  // events (and no regular tasks) get processed by the RunLoop().RunUntilIdle()
  // call done inside of RespondWithNetError. This can be acomplished by turning
  // that RunLoop into a nested one (which would only run system tasks). By
  // posting a task that makes the RespondWithNetError call we will already be
  // running a RunLoop when the call happens thus turning the
  // RespondWithNetError RunLoop into a nested one.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // When the network request finishes, the dispatcher should still hold
        // the ScopedBackgroundTask because it needs to process the results of
        // the request
        RespondWithHttpError(net::HTTP_INTERNAL_SERVER_ERROR);
        // Stop right after the error is processed, so that we can check
        // GetBackgroundTask()
        run_loop.Quit();
      }));
  run_loop.Run();
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

  // We want to make sure the response is received before the dispatcher goes
  // for the next task. For that we need to make sure that only file handle
  // events (and no regular tasks) get processed by the RunLoop().RunUntilIdle()
  // call done inside of RespondWithNetError. This can be acomplished by turning
  // that RunLoop into a nested one (which would only run system tasks). By
  // posting a task that makes the RespondWithNetError call we will already be
  // running a RunLoop when the call happens thus turning the
  // RespondWithNetError RunLoop into a nested one.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([this]() {
        // This should trigger suspend.
        RespondWithNetError(net::ERR_BLOCKED_BY_ADMINISTRATOR);
      }));
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

TEST_F(PrefetchDispatcherTest, ThumbnailImageFetchFailure_ItemDownloaded) {
  Configure(PrefetchServiceTestTaco::kFeed);
  suggestions_provider_->SetSuggestions({TestSuggestion1()});

  PrefetchItem item = item_generator_.CreateItem(PrefetchItemState::DOWNLOADED);
  item.thumbnail_url = GURL(kThumbnailUrl);
  item.client_id.id = kClientID;
  item.offline_id = kTestOfflineID;
  store_util_.InsertPrefetchItem(item);

  ExpectFetchThumbnailImage("", GURL(kThumbnailUrl));
  prefetch_dispatcher()->ItemDownloaded(
      kTestOfflineID, ClientId(kSuggestedArticlesNamespace, kClientID));
  RunUntilIdle();

  EXPECT_TRUE(offline_model_->visuals().empty())
      << "Stored visuals: "
      << ::testing::PrintToString(offline_model_->visuals());
}

// Test attempting to fetch several suggested article thumbnails. This verifies
// that multiple fetches are attempted.
TEST_F(PrefetchDispatcherTest, ThumbnailImageFetch_SeveralThumbnailDownloads) {
  Configure(PrefetchServiceTestTaco::kFeed);
  // Suggestion 1: No thumbnail fetch because there is no thumbnail_url.
  testing::InSequence sequence;
  PrefetchSuggestion suggestion1 = TestSuggestion1();
  suggestion1.thumbnail_url = GURL();
  ExpectFetchFaviconImage(kFaviconData, suggestion1.favicon_url);

  // Suggestion 2: No favicon fetch because there is no favicon_url.
  PrefetchSuggestion suggestion2 = TestSuggestion2();
  suggestion2.favicon_url = GURL();
  ExpectFetchThumbnailImage(kThumbnailData, suggestion2.thumbnail_url);

  // Suggestion 3: Thumbnail fetch fails.
  const PrefetchSuggestion suggestion3 = TestSuggestion3();
  ExpectFetchThumbnailImage("", suggestion3.thumbnail_url);
  ExpectFetchFaviconImage(kFaviconData, suggestion3.favicon_url);
  // Suggestions 4&5: Successful thumbnail fetch.
  const PrefetchSuggestion suggestion4 = TestSuggestion4();
  ExpectFetchThumbnailImage(kThumbnailData, suggestion4.thumbnail_url);
  ExpectFetchFaviconImage(kFaviconData, suggestion4.favicon_url);
  const PrefetchSuggestion suggestion5 = TestSuggestion5();
  ExpectFetchThumbnailImage(kThumbnailData, suggestion5.thumbnail_url);
  ExpectFetchFaviconImage(kFaviconData, suggestion5.favicon_url);

  std::vector<PrefetchSuggestion> suggestions = {
      suggestion1, suggestion2, suggestion3, suggestion4, suggestion5};
  suggestions_provider_->SetSuggestions(suggestions);

  prefetch_service()->NewSuggestionsAvailable();
  RunUntilIdle();

  // Pull out the items to find the OfflineIDs, and continue processing those
  // IDs.
  std::set<PrefetchItem> items;
  store_util_.GetAllItems(&items);
  auto generate_ids = std::make_unique<PrefetchDispatcher::IdsVector>();
  for (const auto& suggestion : suggestions) {
    const PrefetchItem* item = FindByUrl(items, suggestion.article_url);
    ASSERT_TRUE(item) << " item url=" << suggestion.article_url;
    generate_ids->push_back(
        std::make_pair(item->offline_id, SuggestionClientId(suggestion)));
  }
  prefetch_dispatcher()->GeneratePageBundleRequested(std::move(generate_ids));
  RunUntilIdle();
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

  ExpectFetchThumbnail("", kClientID);
  prefetch_dispatcher()->ItemDownloaded(
      kTestOfflineID, ClientId(kSuggestedArticlesNamespace, kClientID));

  EXPECT_TRUE(offline_model_->visuals().empty())
      << "Stored visuals: "
      << ::testing::PrintToString(offline_model_->visuals());
}

TEST_F(PrefetchDispatcherTest, ThumbnailFetchSuccess_ItemDownloaded) {
  Configure(PrefetchServiceTestTaco::kContentSuggestions);

  ExpectFetchThumbnail(kThumbnailData, kClientID);
  prefetch_dispatcher()->ItemDownloaded(
      kTestOfflineID, ClientId(kSuggestedArticlesNamespace, kClientID));
  RunUntilIdle();

  const OfflinePageVisuals* stored_visuals =
      offline_model_->FindVisuals(kTestOfflineID);
  ASSERT_TRUE(stored_visuals);
  EXPECT_EQ(kThumbnailData, stored_visuals->thumbnail);
}

TEST_F(PrefetchDispatcherTest, ThumbnailAlreadyExists_ItemDownloaded) {
  Configure(PrefetchServiceTestTaco::kContentSuggestions);

  offline_model_->set_visuals({{kTestOfflineID, FakeVisuals(kTestOfflineID)}});
  EXPECT_CALL(*thumbnail_fetcher_, FetchSuggestionImageData(_, _)).Times(0);
  prefetch_dispatcher()->ItemDownloaded(
      kTestOfflineID, ClientId(kSuggestedArticlesNamespace, kClientID));
  RunUntilIdle();
}

TEST_F(PrefetchDispatcherTest,
       ThumbnailVariousCases_GeneratePageBundleRequested) {
  Configure(PrefetchServiceTestTaco::kContentSuggestions);

  // TODO -- update to include favicon cases (or break these out)
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
  ExpectFetchThumbnail(kThumbnailData, kClientID1);
  // Case #2.
  ExpectFetchThumbnail("", kClientID2);
  // Case #3: thumbnail already exists
  offline_model_->set_visuals(
      {{kTestOfflineID3, FakeVisuals(kTestOfflineID3)}});

  auto prefetch_item_ids = std::make_unique<PrefetchDispatcher::IdsVector>();
  prefetch_item_ids->emplace_back(
      kTestOfflineID1, ClientId(kSuggestedArticlesNamespace, kClientID1));
  prefetch_item_ids->emplace_back(
      kTestOfflineID2, ClientId(kSuggestedArticlesNamespace, kClientID2));
  prefetch_item_ids->emplace_back(
      kTestOfflineID3, ClientId(kSuggestedArticlesNamespace, kClientID3));
  prefetch_dispatcher()->GeneratePageBundleRequested(
      std::move(prefetch_item_ids));
  RunUntilIdle();

  EXPECT_TRUE(offline_model_->FindVisuals(kTestOfflineID1))
      << "Thumbnails: " << ::testing::PrintToString(offline_model_->visuals());
  EXPECT_FALSE(offline_model_->FindVisuals(kTestOfflineID2));
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

// Tests that |RemoveSuggestion()| removes items from the offline database, and
// triggers finalization of the prefetch item.
TEST_F(PrefetchDispatcherTest, RemoveSuggestion) {
  Configure(PrefetchServiceTestTaco::kFeed);

  EXPECT_CALL(*offline_model_, DeletePagesWithCriteria(_, _))
      .WillOnce([&](const PageCriteria& criteria, DeletePageCallback callback) {
        EXPECT_EQ(TestSuggestion1().article_url, criteria.url);
        EXPECT_EQ(std::vector<std::string>({kSuggestedArticlesNamespace}),
                  criteria.client_namespaces);
      });

  suggestions_provider_->SetSuggestions({TestSuggestion1()});
  prefetch_service()->NewSuggestionsAvailable();
  RunUntilIdle();

  const PrefetchItem item_state_1 = GetSingleItem();

  dispatcher()->RemoveSuggestion(TestSuggestion1().article_url);
  RunUntilIdle();
  const PrefetchItem item_state_2 = GetSingleItem();

  // The item is initially not finished.
  EXPECT_EQ(TestSuggestion1().article_url, item_state_1.url);
  EXPECT_NE(PrefetchItemState::FINISHED, item_state_1.state);

  // The item is finished after the suggestion is removed.
  EXPECT_EQ(PrefetchItemState::FINISHED, item_state_2.state);
}

// Verify that we can attempt to remove a URL that isn't in the prefetch
// database.
TEST_F(PrefetchDispatcherTest, RemoveSuggestionDoesNotExist) {
  Configure(PrefetchServiceTestTaco::kFeed);

  suggestions_provider_->SetSuggestions({TestSuggestion1()});
  prefetch_service()->NewSuggestionsAvailable();
  RunUntilIdle();

  dispatcher()->RemoveSuggestion(GURL("http://otherurl.com"));
  RunUntilIdle();

  // Verify the item still exists.
  GetSingleItem();
}

}  // namespace offline_pages
