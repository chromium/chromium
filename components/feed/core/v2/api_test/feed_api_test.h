// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_API_TEST_FEED_API_TEST_H_
#define COMPONENTS_FEED_CORE_V2_API_TEST_FEED_API_TEST_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/proto/v2/keyvalue_store.pb.h"
#include "components/feed/core/proto/v2/wire/reliability_logging_enums.pb.h"
#include "components/feed/core/proto/v2/wire/there_and_back_again_data.pb.h"
#include "components/feed/core/proto/v2/wire/web_feeds.pb.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/feed_store.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/feed_stream_surface.h"
#include "components/feed/core/v2/image_fetcher.h"
#include "components/feed/core/v2/metrics_reporter.h"
#include "components/feed/core/v2/prefs.h"
#include "components/feed/core/v2/public/reliability_logging_bridge.h"
#include "components/feed/core/v2/public/types.h"
#include "components/feed/core/v2/stream_model.h"
#include "components/feed/core/v2/test/proto_printer.h"
#include "components/feed/core/v2/test/stream_builder.h"
#include "components/feed/core/v2/test/test_util.h"
#include "components/feed/core/v2/types.h"
#include "components/feed/core/v2/wire_response_translator.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "net/http/http_status_code.h"
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

std::vector<feedstore::StoredAction> ReadStoredActions(FeedStore& store);

std::string SerializedOfflineBadgeContent();

feedwire::ThereAndBackAgainData MakeThereAndBackAgainData(int64_t id);

std::string DatastoreEntryToString(std::string_view key,
                                   std::string_view value);

class TestReliabilityLoggingBridge : public ReliabilityLoggingBridge {
 public:
  TestReliabilityLoggingBridge();
  ~TestReliabilityLoggingBridge() override;

  std::string GetEventsString() const;
  void ClearEventsString();

  // ReliabilityLoggingBridge implementation.
  void LogFeedLaunchOtherStart(base::TimeTicks timestamp) override;
  void LogCacheReadStart(base::TimeTicks timestamp) override;
  void LogCacheReadEnd(base::TimeTicks timestamp,
                       feedwire::DiscoverCardReadCacheResult result) override;
  void LogFeedRequestStart(NetworkRequestId id,
                           base::TimeTicks timestamp) override;
  void LogActionsUploadRequestStart(NetworkRequestId id,
                                    base::TimeTicks timestamp) override;
  void LogWebFeedRequestStart(NetworkRequestId id,
                              base::TimeTicks timestamp) override;
  void LogSingleWebFeedRequestStart(NetworkRequestId id,
                                    base::TimeTicks timestamp) override;
  void LogRequestSent(NetworkRequestId id, base::TimeTicks timestamp) override;
  void LogResponseReceived(NetworkRequestId id,
                           int64_t server_receive_timestamp_ns,
                           int64_t server_send_timestamp_ns,
                           base::TimeTicks client_receive_timestamp) override;
  void LogRequestFinished(NetworkRequestId id,
                          base::TimeTicks timestamp,
                          int combined_network_status_code) override;
  void LogLoadingIndicatorShown(base::TimeTicks timestamp) override;
  void LogAboveTheFoldRender(
      base::TimeTicks timestamp,
      feedwire::DiscoverAboveTheFoldRenderResult result) override;

  void LogLaunchFinishedAfterStreamUpdate(
      feedwire::DiscoverLaunchResult result) override;
  void LogLoadMoreStarted() override;
  void LogLoadMoreActionUploadRequestStarted() override;
  void LogLoadMoreRequestSent() override;
  void LogLoadMoreResponseReceived(int64_t server_receive_timestamp_ns,
                                   int64_t server_send_timestamp_ns) override;
  void LogLoadMoreRequestFinished(int canonical_status) override;
  void LogLoadMoreEnded(bool success) override;
  void ReportExperiments(const std::vector<int32_t>& experiment_ids) override;

 private:
  std::vector<std::string> events_;
};

class TestSurfaceBase : public feed::SurfaceRenderer {
 public:
  // Provide some helper functionality to attach/detach the surface.
  // This way we can auto-detach in the destructor.
  explicit TestSurfaceBase(
      const StreamType& stream_type,
      FeedStream* stream = nullptr,
      SingleWebFeedEntryPoint entry_point = SingleWebFeedEntryPoint::kOther);
  ~TestSurfaceBase() override;

  SurfaceId GetSurfaceId() const;
  const StreamType GetStreamType() const { return stream_type_; }
  SingleWebFeedEntryPoint GetSingleWebFeedEntryPoint() const {
    return entry_point_;
  }

  // Create the surface with FeedApi::CreateSurface, but don't attach it.
  void CreateWithoutAttach(FeedStream* stream);

  // Calls FeedApi::CreateSurface if it hasn't been created yet, and attaches
  // the surface for rendering.
  void Attach(FeedStream* stream);

  void Detach();

  // FeedStream::FeedStreamSurface.
  void StreamUpdate(const feedui::StreamUpdate& stream_update) override;
  void ReplaceDataStoreEntry(std::string_view key,
                             std::string_view data) override;
  void RemoveDataStoreEntry(std::string_view key) override;
  ReliabilityLoggingBridge& GetReliabilityLoggingBridge() override;

  // Test functions.
  void Clear();

  // Returns a description of the updates this surface received. Each update
  // is separated by ' -> '. Returns only the updates since the last call.
  std::string DescribeUpdates();
  // Returns a description of the current state, ignoring prior updates.
  std::string DescribeState();

  std::map<std::string, std::string> GetDataStoreEntries() const;
  std::string DescribeDataStore() const;
  std::vector<std::string> DescribeDataStoreUpdates();

  // Returns the logging parameters last sent to the surface.
  LoggingParameters GetLoggingParameters() const;

  // The initial state of the stream, if it was received. This is nullopt if
  // only the loading spinner was seen.
  std::optional<feedui::StreamUpdate> initial_state;
  // The last stream update received.
  std::optional<feedui::StreamUpdate> update;
  // All stream updates.
  std::vector<feedui::StreamUpdate> all_updates;

  TestReliabilityLoggingBridge reliability_logging_bridge;

 private:
  std::string CurrentState();

  bool IsInitialLoadSpinnerUpdate(const feedui::StreamUpdate& stream_update);

  const StreamType stream_type_;
  SingleWebFeedEntryPoint entry_point_;
  SurfaceId surface_id_ = {};

  // The stream if this surface was attached at least once.
  base::WeakPtr<FeedStream> stream_;
  // The stream if this surface is attached.
  base::WeakPtr<FeedStream> bound_stream_;
  std::vector<std::string> described_updates_;
  std::map<std::string, std::string> data_store_entries_;
  std::vector<std::string> described_datastore_updates_;
  std::string last_logging_parameters_description_;
};

class TestForYouSurface : public TestSurfaceBase {
 public:
  explicit TestForYouSurface(FeedStream* stream = nullptr);
};
class TestWebFeedSurface : public TestSurfaceBase {
 public:
  explicit TestWebFeedSurface(FeedStream* stream = nullptr);
};
class TestSingleWebFeedSurface : public TestSurfaceBase {
 public:
  explicit TestSingleWebFeedSurface(
      FeedStream* stream = nullptr,
      std::string = "",
      SingleWebFeedEntryPoint entry_point = SingleWebFeedEntryPoint::kOther);
};
class TestSupervisedFeedSurface : public TestSurfaceBase {
 public:
  explicit TestSupervisedFeedSurface(FeedStream* stream = nullptr);
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

class TestUnreadContentObserver : public UnreadContentObserver {
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
      const AccountInfo& account_info,
      base::OnceCallback<void(QueryRequestResult)> callback) override;

  void SendDiscoverApiRequest(
      NetworkRequestType request_type,
      std::string_view api_path,
      std::string_view method,
      std::string request_bytes,
      const AccountInfo& account_info,
      std::optional<RequestMetadata> request_metadata,
      base::OnceCallback<void(RawResponse)> callback) override;

  void SendAsyncDataRequest(
      const GURL& url,
      std::string_view request_method,
      net::HttpRequestHeaders request_headers,
      std::string request_body,
      const AccountInfo& account_info,
      base::OnceCallback<void(RawResponse)> callback) override;

  void CancelRequests() override;

  void InjectRealFeedQueryResponse();
  void InjectRealFeedQueryResponseWithNoContent();

  template <typename API>
  void InjectApiRawResponse(RawResponse result) {
    NetworkRequestType request_type = API::kRequestType;
    injected_api_responses_[request_type].push_back(result);
  }
  template <typename API>
  void InjectApiResponse(const typename API::Response& response_message) {
    RawResponse response;
    response.response_info.status_code = 200;
    response.response_bytes = response_message.SerializeAsString();
    response.response_info.response_body_bytes = response.response_bytes.size();
    response.response_info.account_info = last_account_info;
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
  void InjectResponse(const feedwire::webfeed::QueryWebFeedResponse& response) {
    InjectApiResponse<QueryWebFeedDiscoverApi>(response);
  }
  void InjectQueryResponse(const FeedNetwork::RawResponse& response) {
    InjectApiRawResponse<QueryWebFeedDiscoverApi>(response);
  }

  void InjectListWebFeedsResponse(
      std::vector<feedwire::webfeed::WebFeed> web_feeds) {
    feedwire::webfeed::ListWebFeedsResponse response;
    for (const auto& feed : web_feeds) {
      *response.add_web_feeds() = feed;
    }
    InjectResponse(response);
  }
  void InjectListWebFeedsResponse(const FeedNetwork::RawResponse& response) {
    InjectApiRawResponse<ListWebFeedsDiscoverApi>(response);
  }
  void InjectRawResponse(const FeedNetwork::RawResponse& response) {
    injected_raw_response_ = response;
  }

  void InjectEmptyActionRequestResult();

  template <typename API>
  std::optional<typename API::Request> GetApiRequestSent() {
    std::optional<typename API::Request> result;
    NetworkRequestType request_type = API::kRequestType;
    auto iter = api_requests_sent_.find(request_type);
    if (iter != api_requests_sent_.end()) {
      typename API::Request message;
      if (!iter->second.empty()) {
        if (!message.ParseFromString(iter->second)) {
          LOG(ERROR) << "Failed to parse API request.";
          return std::nullopt;
        }
      }
      result = message;
    }
    return result;
  }

  std::optional<feedwire::UploadActionsRequest> GetActionRequestSent();

  template <typename API>
  int GetApiRequestCount() const {
    NetworkRequestType request_type = API::kRequestType;
    auto iter = api_request_count_.find(request_type);
    return iter == api_request_count_.end() ? 0 : iter->second;
  }
  std::map<NetworkRequestType, int> GetApiRequestCounts() const {
    return api_request_count_;
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
  int GetWebFeedListContentsCount() const {
    return GetApiRequestCount<WebFeedListContentsDiscoverApi>();
  }

  std::vector<NetworkRequestType> sent_request_types() const {
    return sent_request_types_;
  }

  void ClearTestData();

  // Enable (or disable) manual triggering of sending responses. When enabled,
  // injected responses are not sent upon request, but instead one at a time
  // when `SendResponse()` is called.
  void SendResponsesOnCommand(bool on);
  void SendResponse();

  std::optional<feedwire::Request> query_request_sent;
  // Number of FeedQuery requests sent (including Web Feed ListContents).
  int send_query_call_count = 0;
  AccountInfo last_account_info;
  // The consistency token to use when constructing default network responses.
  std::string consistency_token;
  net::HttpStatusCode http_status_code = net::HttpStatusCode::HTTP_OK;
  net::Error error = net::Error::OK;

 private:
  void Reply(base::OnceClosure reply_closure);

  bool send_responses_on_command_ = false;
  std::vector<base::OnceClosure> reply_closures_;
  base::RepeatingClosure on_reply_added_;
  std::map<NetworkRequestType, std::vector<RawResponse>>
      injected_api_responses_;
  std::map<NetworkRequestType, std::string> api_requests_sent_;
  std::map<NetworkRequestType, int> api_request_count_;
  std::vector<NetworkRequestType> sent_request_types_;
  std::optional<feedwire::Response> injected_response_;
  std::optional<RawResponse> injected_raw_response_;
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
      const AccountInfo& account_info,
      base::Time current_time) const override;
  void InjectResponse(std::unique_ptr<StreamModelUpdateRequest> response,
                      std::optional<std::string> session_id = std::nullopt);
  void InjectResponse(RefreshResponseData response_data);
  bool InjectedResponseConsumed() const;

 private:
  std::optional<RefreshResponseData> TranslateStreamSource(
      StreamModelUpdateRequest::Source source,
      const AccountInfo& account_info,
      base::Time current_time) const;

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
                          int index_in_stream,
                          int stream_slice_count) override;
  void OnLoadStream(const StreamType& stream_type,
                    const LoadStreamResultSummary& result_summary,
                    const ContentStats& content_stats,
                    std::unique_ptr<LoadLatencyTimes> load_latencies) override;
  void OnLoadMoreBegin(const StreamType& stream_type,
                       SurfaceId surface_id) override;
  void OnLoadMore(const StreamType& stream_type,
                  LoadStreamStatus final_status,
                  const ContentStats& content_stats) override;
  void OnBackgroundRefresh(const StreamType& stream_type,
                           LoadStreamStatus final_status) override;
  void OnUploadActions(UploadActionsStatus status) override;

  struct StreamMetrics {
    StreamMetrics();
    ~StreamMetrics();
    StreamMetrics(const StreamMetrics&) = delete;
    StreamMetrics& operator=(const StreamMetrics&) = delete;
    std::optional<LoadStreamStatus> background_refresh_status;
  };

  StreamMetrics& Stream(const StreamType& stream_type);

  // Test access.
  std::optional<int> slice_viewed_index;
  std::optional<LoadStreamStatus> load_stream_status;
  std::optional<LoadStreamStatus> load_stream_from_store_status;
  std::optional<SurfaceId> load_more_surface_id;
  std::optional<LoadStreamStatus> load_more_status;
  std::optional<LoadStreamStatus> background_refresh_status;
  std::optional<UploadActionsStatus> upload_action_status;

  StreamMetrics web_feed;
  StreamMetrics for_you;
};

// Base text fixture for feed API tests.
// Note: The web-feeds feature is enabled by default for these tests because
// GetCountry() is overridden to return one of the launch counties.
class FeedApiTest : public testing::Test, public FeedStream::Delegate {
 public:
  FeedApiTest();
  ~FeedApiTest() override;
  void SetUp() override;
  void TearDown() override;

  // FeedStream::Delegate.
  bool IsEulaAccepted() override;
  bool IsOffline() override;
  DisplayMetrics GetDisplayMetrics() override;
  std::string GetLanguageTag() override;
  TabGroupEnabledState GetTabGroupEnabledState() override;
  void ClearAll() override;
  AccountInfo GetAccountInfo() override;
  bool IsSupervisedAccount() override;
  bool IsSigninAllowed() override;
  void PrefetchImage(const GURL& url) override;
  void RegisterExperiments(const Experiments& experiments) override {}
  void RegisterFollowingFeedFollowCountFieldTrial(size_t follow_count) override;
  void RegisterFeedUserSettingsFieldTrial(std::string_view group) override;
  std::string GetCountry() override;

  // For tests.

  void SetCountry(const std::string& country);

  // Replace stream_.
  void CreateStream(bool wait_for_initialization = true,
                    bool is_new_tab_search_engine_url_android_enabled = false);
  std::unique_ptr<StreamModel> CreateStreamModel();
  bool IsTaskQueueIdle() const;
  void WaitForIdleTaskQueue();
  // Fast forwards the task environment enough for the in-memory model to
  // auto-unload, which will only take place if there are no attached surfaces.
  void WaitForModelToAutoUnload();
  void UnloadModel(const StreamType& stream_type);
  void FollowWebFeed(const WebFeedPageInformation page_info);

  // Dumps the state of |FeedStore| to a string for debugging.
  std::string DumpStoreState(bool print_keys = false);

  void UploadActions(std::vector<feedwire::FeedAction> actions);
  // Returns some logging parameters for the current signed in user. Prefer to
  // use the logging parameters passed to TestSurface*.
  LoggingParameters CreateLoggingParameters();

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  sync_preferences::TestingPrefServiceSyncable profile_prefs_;
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

  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  FakeRefreshTaskScheduler refresh_scheduler_;
  StreamModel::Context stream_model_context_;
  std::unique_ptr<FeedStream> stream_;
  bool is_eula_accepted_ = true;
  bool is_offline_ = false;
  AccountInfo account_info_ = TestAccountInfo();
  bool is_signin_allowed_ = true;
  bool is_supervised_account_ = false;
  int prefetch_image_call_count_ = 0;
  std::vector<GURL> prefetched_images_;
  base::RepeatingClosure on_clear_all_;
  std::vector<size_t> register_following_feed_follow_count_field_trial_calls_;
  std::vector<std::string> register_feed_user_settings_field_trial_calls_;
  std::string country_ = "US";
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
  void SetUp() override;
  RefreshTaskId GetRefreshTaskId() const;
};

class FeedNetworkEndpointTest
    : public FeedApiTest,
      public ::testing::WithParamInterface<::testing::tuple<bool, bool>> {
 public:
  static bool GetDiscoFeedEnabled() { return ::testing::get<0>(GetParam()); }
  // Whether Feed-Query is used instead, as request in snippets-internals.
  static bool GetUseFeedQueryRequests() {
    return ::testing::get<1>(GetParam());
  }
};

}  // namespace test
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_API_TEST_FEED_API_TEST_H_
