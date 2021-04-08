// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_API_TEST_FEED_API_TEST_H_
#define COMPONENTS_FEED_CORE_V2_API_TEST_FEED_API_TEST_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/optional.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/proto/v2/keyvalue_store.pb.h"
#include "components/feed/core/proto/v2/wire/there_and_back_again_data.pb.h"
#include "components/feed/core/proto/v2/wire/web_feeds.pb.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/feed_store.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/image_fetcher.h"
#include "components/feed/core/v2/metrics_reporter.h"
#include "components/feed/core/v2/prefs.h"
#include "components/feed/core/v2/stream_model.h"
#include "components/feed/core/v2/test/proto_printer.h"
#include "components/feed/core/v2/test/stream_builder.h"
#include "components/feed/core/v2/test/test_util.h"
#include "components/feed/core/v2/wire_response_translator.h"
#include "components/offline_pages/core/prefetch/stub_prefetch_service.h"
#include "components/offline_pages/core/stub_offline_page_model.h"
#include "components/prefs/testing_pref_service.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace test {

std::unique_ptr<StreamModel> LoadModelFromStore(const StreamType& stream_type,
                                                FeedStore* store);
std::unique_ptr<StreamModelUpdateRequest> StoredModelData(
    const StreamType& stream_type,
    FeedStore* store);

// Returns the model state string (|StreamModel::DumpStateForTesting()|),
// given a model initialized with |update_request| and having |operations|
// applied.
std::string ModelStateFor(
    std::unique_ptr<StreamModelUpdateRequest> update_request,
    std::vector<feedstore::DataOperation> operations = {},
    std::vector<feedstore::DataOperation> more_operations = {});

// Returns the model state string (|StreamModel::DumpStateForTesting()|),
// given a model initialized with |store|.
std::string ModelStateFor(const StreamType& stream_type, FeedStore* store);

feedwire::FeedAction MakeFeedAction(int64_t id, size_t pad_size = 0);

std::vector<feedstore::StoredAction> ReadStoredActions(FeedStore* store);

std::string SerializedOfflineBadgeContent();

feedwire::ThereAndBackAgainData MakeThereAndBackAgainData(int64_t id);

class TestSurfaceBase : public FeedStreamSurface {
 public:
  // Provide some helper functionality to attach/detach the surface.
  // This way we can auto-detach in the destructor.
  explicit TestSurfaceBase(const StreamType& stream_type,
                           FeedStream* stream = nullptr);

  ~TestSurfaceBase() override;

  void Attach(FeedStream* stream);

  void Detach();

  // FeedStream::FeedStreamSurface.
  void StreamUpdate(const feedui::StreamUpdate& stream_update) override;
  void ReplaceDataStoreEntry(base::StringPiece key,
                             base::StringPiece data) override;
  void RemoveDataStoreEntry(base::StringPiece key) override;

  // Test functions.

  void Clear();

  // Returns a description of the updates this surface received. Each update
  // is separated by ' -> '. Returns only the updates since the last call.
  std::string DescribeUpdates();

  std::map<std::string, std::string> GetDataStoreEntries() const;

  // The initial state of the stream, if it was received. This is nullopt if
  // only the loading spinner was seen.
  base::Optional<feedui::StreamUpdate> initial_state;
  // The last stream update received.
  base::Optional<feedui::StreamUpdate> update;

 private:
  std::string CurrentState();

  bool IsInitialLoadSpinnerUpdate(const feedui::StreamUpdate& update);

  // The stream if it was attached using the constructor.
  base::WeakPtr<FeedStream> stream_;
  std::vector<std::string> described_updates_;
  std::map<std::string, std::string> data_store_entries_;
};

class TestForYouSurface : public TestSurfaceBase {
 public:
  explicit TestForYouSurface(FeedStream* stream = nullptr);
};
class TestWebFeedSurface : public TestSurfaceBase {
 public:
  explicit TestWebFeedSurface(FeedStream* stream = nullptr);
};

class TestImageFetcher : public ImageFetcher {
 public:
  explicit TestImageFetcher(
      scoped_refptr<::network::SharedURLLoaderFactory> url_loader_factory);
  ImageFetchId Fetch(
      const GURL& url,
      base::OnceCallback<void(NetworkResponse)> callback) override;
  void Cancel(ImageFetchId id) override {}

 private:
  ImageFetchId::Generator id_generator_;
};

class TestUnreadContentObserver : public FeedApi::UnreadContentObserver {
 public:
  TestUnreadContentObserver();
  ~TestUnreadContentObserver() override;
  void HasUnreadContentChanged(bool has_unread_content) override;

  std::vector<bool> calls;
};

class TestFeedNetwork : public FeedNetwork {
 public:
  TestFeedNetwork();
  ~TestFeedNetwork() override;
  // FeedNetwork implementation.
  void SendQueryRequest(
      NetworkRequestType request_type,
      const feedwire::Request& request,
      bool force_signed_out_request,
      const std::string& gaia,
      base::OnceCallback<void(QueryRequestResult)> callback) override;

  void SendDiscoverApiRequest(
      base::StringPiece api_path,
      base::StringPiece method,
      std::string request_bytes,
      const std::string& gaia,
      base::OnceCallback<void(RawResponse)> callback) override;

  void CancelRequests() override;

  void InjectRealFeedQueryResponse();

  template <typename API>
  void InjectApiRawResponse(RawResponse result) {
    injected_api_responses_[API::RequestPath().as_string()].push_back(result);
  }

  template <typename API>
  void InjectApiResponse(const typename API::Response& response_message) {
    RawResponse response;
    response.response_info.status_code = 200;
    response.response_bytes = response_message.SerializeAsString();
    response.response_info.response_body_bytes = response.response_bytes.size();
    InjectApiRawResponse<API>(std::move(response));
  }

  void InjectResponse(
      const feedwire::webfeed::FollowWebFeedResponse& response) {
    InjectApiResponse<FollowWebFeedDiscoverApi>(response);
  }
  void InjectFollowResponse(const FeedNetwork::RawResponse& response) {
    InjectApiRawResponse<FollowWebFeedDiscoverApi>(response);
  }
  void InjectResponse(
      const feedwire::webfeed::UnfollowWebFeedResponse& response) {
    InjectApiResponse<UnfollowWebFeedDiscoverApi>(response);
  }
  void InjectUnfollowResponse(const FeedNetwork::RawResponse& response) {
    InjectApiRawResponse<UnfollowWebFeedDiscoverApi>(response);
  }
  void InjectResponse(
      feedwire::webfeed::ListRecommendedWebFeedsResponse response) {
    InjectApiResponse<ListRecommendedWebFeedDiscoverApi>(std::move(response));
  }
  void InjectResponse(feedwire::webfeed::ListWebFeedsResponse response) {
    InjectApiResponse<ListWebFeedsDiscoverApi>(std::move(response));
  }
  void InjectEmptyActionRequestResult();

  template <typename API>
  base::Optional<typename API::Request> GetApiRequestSent() {
    base::Optional<typename API::Request> result;
    auto iter = api_requests_sent_.find(API::RequestPath().as_string());
    if (iter != api_requests_sent_.end()) {
      typename API::Request message;
      if (!iter->second.empty()) {
        if (!message.ParseFromString(iter->second)) {
          LOG(ERROR) << "Failed to parse API request.";
          return base::nullopt;
        }
      }
      result = message;
    }
    return result;
  }

  base::Optional<feedwire::UploadActionsRequest> GetActionRequestSent();

  template <typename API>
  int GetApiRequestCount() const {
    auto iter = api_request_count_.find(API::RequestPath().as_string());
    return iter == api_request_count_.end() ? 0 : iter->second;
  }
  int GetActionRequestCount() const;
  int GetFollowRequestCount() const {
    return GetApiRequestCount<FollowWebFeedDiscoverApi>();
  }
  int GetUnfollowRequestCount() const {
    return GetApiRequestCount<UnfollowWebFeedDiscoverApi>();
  }
  int GetListRecommendedWebFeedsRequestCount() const {
    return GetApiRequestCount<ListRecommendedWebFeedDiscoverApi>();
  }
  int GetListFollowedWebFeedsRequestCount() const {
    return GetApiRequestCount<ListWebFeedsDiscoverApi>();
  }

  void ClearTestData();

  // Enable (or disable) manual triggering of sending responses. When enabled,
  // injected responses are not sent upon request, but instead one at a time
  // when `SendResponse()` is called.
  void SendResponsesOnCommand(bool on);
  void SendResponse();

  base::Optional<feedwire::Request> query_request_sent;
  int send_query_call_count = 0;
  std::string consistency_token;
  bool forced_signed_out_request = false;

 private:
  void Reply(base::OnceClosure reply_closure);

  bool send_responses_on_command_ = false;
  std::vector<base::OnceClosure> reply_closures_;
  base::RepeatingClosure on_reply_added_;
  std::map<std::string, std::vector<RawResponse>> injected_api_responses_;
  std::map<std::string, std::string> api_requests_sent_;
  std::map<std::string, int> api_request_count_;
  base::Optional<feedwire::Response> injected_response_;
};

// Forwards to |FeedStream::WireResponseTranslator| unless a response is
// injected.
class TestWireResponseTranslator : public WireResponseTranslator {
 public:
  TestWireResponseTranslator();
  ~TestWireResponseTranslator();
  RefreshResponseData TranslateWireResponse(
      feedwire::Response response,
      StreamModelUpdateRequest::Source source,
      bool was_signed_in_request,
      base::Time current_time) const override;
  void InjectResponse(std::unique_ptr<StreamModelUpdateRequest> response,
                      base::Optional<std::string> session_id = base::nullopt);
  void InjectResponse(RefreshResponseData response_data);
  bool InjectedResponseConsumed() const;

 private:
  mutable std::vector<RefreshResponseData> injected_responses_;
};

class FakeRefreshTaskScheduler : public RefreshTaskScheduler {
 public:
  FakeRefreshTaskScheduler();
  ~FakeRefreshTaskScheduler() override;
  // RefreshTaskScheduler implementation.
  void EnsureScheduled(RefreshTaskId id, base::TimeDelta run_time) override;
  void Cancel(RefreshTaskId id) override;
  void RefreshTaskComplete(RefreshTaskId id) override;

  void Clear();

  std::map<RefreshTaskId, base::TimeDelta> scheduled_run_times;
  std::set<RefreshTaskId> canceled_tasks;
  std::set<RefreshTaskId> completed_tasks;

 private:
  std::stringstream activity_log_;
};

class TestMetricsReporter : public MetricsReporter {
 public:
  explicit TestMetricsReporter(PrefService* prefs);
  ~TestMetricsReporter() override;

  // MetricsReporter.
  void ContentSliceViewed(const StreamType& stream_type,
                          int index_in_stream) override;
  void OnLoadStream(LoadStreamStatus load_from_store_status,
                    LoadStreamStatus final_status,
                    bool loaded_new_content_from_network,
                    base::TimeDelta stored_content_age,
                    std::unique_ptr<LoadLatencyTimes> latencies) override;
  void OnLoadMoreBegin(SurfaceId surface_id) override;
  void OnLoadMore(LoadStreamStatus final_status) override;
  void OnBackgroundRefresh(LoadStreamStatus final_status) override;
  void OnClearAll(base::TimeDelta time_since_last_clear) override;
  void OnUploadActions(UploadActionsStatus status) override;

  // Test access.

  base::Optional<int> slice_viewed_index;
  base::Optional<LoadStreamStatus> load_stream_status;
  base::Optional<LoadStreamStatus> load_stream_from_store_status;
  base::Optional<SurfaceId> load_more_surface_id;
  base::Optional<LoadStreamStatus> load_more_status;
  base::Optional<LoadStreamStatus> background_refresh_status;
  base::Optional<base::TimeDelta> time_since_last_clear;
  base::Optional<UploadActionsStatus> upload_action_status;
};

class TestPrefetchService : public offline_pages::StubPrefetchService {
 public:
  TestPrefetchService();
  ~TestPrefetchService() override;
  // offline_pages::StubPrefetchService.
  void SetSuggestionProvider(
      offline_pages::SuggestionsProvider* suggestions_provider) override;
  void NewSuggestionsAvailable() override;

  // Test functionality.
  offline_pages::SuggestionsProvider* suggestions_provider();
  int NewSuggestionsAvailableCallCount() const;

 private:
  offline_pages::SuggestionsProvider* suggestions_provider_ = nullptr;
  int new_suggestions_available_call_count_ = 0;
};

class TestOfflinePageModel : public offline_pages::StubOfflinePageModel {
 public:
  TestOfflinePageModel();
  ~TestOfflinePageModel() override;
  // offline_pages::OfflinePageModel
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void GetPagesWithCriteria(
      const offline_pages::PageCriteria& criteria,
      offline_pages::MultipleOfflinePageItemCallback callback) override;

  // Test functions.

  void AddTestPage(const GURL& url);
  std::vector<offline_pages::OfflinePageItem>& items() { return items_; }
  void CallObserverOfflinePageAdded(const offline_pages::OfflinePageItem& item);
  void CallObserverOfflinePageDeleted(
      const offline_pages::OfflinePageItem& item);

 private:
  std::vector<offline_pages::OfflinePageItem> items_;
  std::set<Observer*> observers_;
};

class FeedApiTest : public testing::Test, public FeedStream::Delegate {
 public:
  FeedApiTest();
  ~FeedApiTest() override;
  void SetUp() override;

  virtual void SetupFeatures() {}

  void TearDown() override;

  // FeedStream::Delegate.
  bool IsEulaAccepted() override;
  bool IsOffline() override;
  DisplayMetrics GetDisplayMetrics() override;
  std::string GetLanguageTag() override;
  void ClearAll() override {}
  std::string GetSyncSignedInGaia() override;
  void PrefetchImage(const GURL& url) override;
  void RegisterExperiments(const Experiments& experiments) override {}

  // For tests.

  // Replace stream_.
  void CreateStream();
  bool IsTaskQueueIdle() const;
  void WaitForIdleTaskQueue();
  void UnloadModel(const StreamType& stream_type);

  // Dumps the state of |FeedStore| to a string for debugging.
  std::string DumpStoreState(bool print_keys = false);

  void UploadActions(std::vector<feedwire::FeedAction> actions);

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple profile_prefs_;
  std::unique_ptr<TestMetricsReporter> metrics_reporter_;
  TestFeedNetwork network_;
  TestWireResponseTranslator response_translator_;
  std::unique_ptr<TestImageFetcher> image_fetcher_;
  network::TestURLLoaderFactory test_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  std::unique_ptr<FeedStore> store_ = std::make_unique<FeedStore>(
      leveldb_proto::ProtoDatabaseProvider::GetUniqueDB<feedstore::Record>(
          leveldb_proto::ProtoDbType::FEED_STREAM_DATABASE,
          /*db_dir=*/{},
          task_environment_.GetMainThreadTaskRunner()));

  std::unique_ptr<PersistentKeyValueStoreImpl> persistent_key_value_store_ =
      std::make_unique<PersistentKeyValueStoreImpl>(
          leveldb_proto::ProtoDatabaseProvider::GetUniqueDB<feedkvstore::Entry>(
              leveldb_proto::ProtoDbType::FEED_KEY_VALUE_DATABASE,
              /*db_dir=*/{},
              task_environment_.GetMainThreadTaskRunner()));

  FakeRefreshTaskScheduler refresh_scheduler_;
  TestPrefetchService prefetch_service_;
  TestOfflinePageModel offline_page_model_;
  std::unique_ptr<FeedStream> stream_;
  bool is_eula_accepted_ = true;
  bool is_offline_ = false;
  std::string signed_in_gaia_ = "examplegaia";
  base::test::ScopedFeatureList scoped_feature_list_;
  int prefetch_image_call_count_ = 0;
  std::vector<GURL> prefetched_images_;
};

class FeedStreamTestForAllStreamTypes
    : public FeedApiTest,
      public ::testing::WithParamInterface<StreamType> {
 public:
  static StreamType GetStreamType() { return GetParam(); }
  class TestSurface : public TestSurfaceBase {
   public:
    explicit TestSurface(FeedStream* stream = nullptr)
        : TestSurfaceBase(FeedStreamTestForAllStreamTypes::GetStreamType(),
                          stream) {}
  };
  RefreshTaskId GetRefreshTaskId() const;
};

}  // namespace test
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_API_TEST_FEED_API_TEST_H_
