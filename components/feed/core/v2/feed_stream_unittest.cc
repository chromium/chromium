// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/feed_stream.h"

#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/clock.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/proto/v2/keyvalue_store.pb.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/ui.pb.h"
#include "components/feed/core/proto/v2/wire/chrome_client_info.pb.h"
#include "components/feed/core/proto/v2/wire/request.pb.h"
#include "components/feed/core/proto/v2/wire/there_and_back_again_data.pb.h"
#include "components/feed/core/proto/v2/xsurface.pb.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/image_fetcher.h"
#include "components/feed/core/v2/metrics_reporter.h"
#include "components/feed/core/v2/persistent_key_value_store_impl.h"
#include "components/feed/core/v2/prefs.h"
#include "components/feed/core/v2/protocol_translator.h"
#include "components/feed/core/v2/public/feed_stream_api.h"
#include "components/feed/core/v2/public/persistent_key_value_store.h"
#include "components/feed/core/v2/refresh_task_scheduler.h"
#include "components/feed/core/v2/scheduling.h"
#include "components/feed/core/v2/stream_model.h"
#include "components/feed/core/v2/tasks/load_stream_from_store_task.h"
#include "components/feed/core/v2/test/callback_receiver.h"
#include "components/feed/core/v2/test/proto_printer.h"
#include "components/feed/core/v2/test/stream_builder.h"
#include "components/feed/feed_feature_list.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/page_criteria.h"
#include "components/offline_pages/core/prefetch/stub_prefetch_service.h"
#include "components/offline_pages/core/stub_offline_page_model.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

namespace feed {
namespace {

std::unique_ptr<StreamModel> LoadModelFromStore(const StreamType& stream_type,
                                                FeedStore* store) {
  LoadStreamFromStoreTask::Result result;
  auto complete = [&](LoadStreamFromStoreTask::Result task_result) {
    result = std::move(task_result);
  };
  LoadStreamFromStoreTask load_task(
      LoadStreamFromStoreTask::LoadType::kFullLoad, stream_type, store,
      /*missed_last_refresh=*/false, base::BindLambdaForTesting(complete));
  // We want to load the data no matter how stale.
  load_task.IgnoreStalenessForTesting();

  base::RunLoop run_loop;
  load_task.Execute(run_loop.QuitClosure());
  run_loop.Run();

  if (result.status == LoadStreamStatus::kLoadedFromStore) {
    auto model = std::make_unique<StreamModel>();
    model->Update(std::move(result.update_request));
    return model;
  }
  LOG(WARNING) << "LoadModelFromStore failed with " << result.status;
  return nullptr;
}

// Returns the model state string (|StreamModel::DumpStateForTesting()|),
// given a model initialized with |update_request| and having |operations|
// applied.
std::string ModelStateFor(
    std::unique_ptr<StreamModelUpdateRequest> update_request,
    std::vector<feedstore::DataOperation> operations = {},
    std::vector<feedstore::DataOperation> more_operations = {}) {
  StreamModel model;
  model.Update(std::move(update_request));
  model.ExecuteOperations(operations);
  model.ExecuteOperations(more_operations);
  return model.DumpStateForTesting();
}

// Returns the model state string (|StreamModel::DumpStateForTesting()|),
// given a model initialized with |store|.
std::string ModelStateFor(const StreamType& stream_type, FeedStore* store) {
  auto model = LoadModelFromStore(stream_type, store);
  if (model) {
    return model->DumpStateForTesting();
  }
  return "{Failed to load model from store}";
}

feedwire::FeedAction MakeFeedAction(int64_t id, size_t pad_size = 0) {
  feedwire::FeedAction action;
  action.mutable_content_id()->set_id(id);
  action.mutable_content_id()->set_content_domain(std::string(pad_size, 'a'));
  return action;
}

std::vector<feedstore::StoredAction> ReadStoredActions(FeedStore* store) {
  base::RunLoop run_loop;
  CallbackReceiver<std::vector<feedstore::StoredAction>> cr(&run_loop);
  store->ReadActions(cr.Bind());
  run_loop.Run();
  CHECK(cr.GetResult());
  return std::move(*cr.GetResult());
}

std::string SerializedOfflineBadgeContent() {
  feedxsurface::OfflineBadgeContent testbadge;
  std::string badge_serialized;
  testbadge.set_available_offline(true);
  testbadge.SerializeToString(&badge_serialized);
  return badge_serialized;
}

feedwire::ThereAndBackAgainData MakeThereAndBackAgainData(int64_t id) {
  feedwire::ThereAndBackAgainData msg;
  *msg.mutable_action_payload() = MakeFeedAction(id).action_payload();
  return msg;
}

// This is EXPECT_EQ, but also dumps the string values for ease of reading.
#define EXPECT_STRINGS_EQUAL(WANT, GOT)                                       \
  {                                                                           \
    std::string want = (WANT), got = (GOT);                                   \
    EXPECT_EQ(want, got) << "Wanted:\n" << (want) << "\nBut got:\n" << (got); \
  }

// Although time is mocked through TaskEnvironment, it does drift by small
// amounts.
const base::TimeDelta kEpsilon = base::TimeDelta::FromMilliseconds(5);
#define EXPECT_TIME_EQ(WANT, GOT)          \
  {                                        \
    base::Time want = (WANT), got = (GOT); \
    if (got != want) {                     \
      EXPECT_LT(want - kEpsilon, got);     \
      EXPECT_GT(want + kEpsilon, got);     \
    }                                      \
  }

class TestSurfaceBase : public FeedStream::SurfaceInterface {
 public:
  // Provide some helper functionality to attach/detach the surface.
  // This way we can auto-detach in the destructor.
  explicit TestSurfaceBase(const StreamType& stream_type,
                           FeedStream* stream = nullptr)
      : FeedStream::SurfaceInterface(stream_type) {
    if (stream)
      Attach(stream);
  }

  ~TestSurfaceBase() override {
    if (stream_)
      Detach();
  }

  void Attach(FeedStream* stream) {
    EXPECT_FALSE(stream_);
    stream_ = stream;
    stream_->AttachSurface(this);
  }

  void Detach() {
    EXPECT_TRUE(stream_);
    stream_->DetachSurface(this);
    stream_ = nullptr;
  }

  // FeedStream::SurfaceInterface.
  void StreamUpdate(const feedui::StreamUpdate& stream_update) override {
    DVLOG(1) << "StreamUpdate: " << stream_update;
    // Some special-case treatment for the loading spinner. We don't count it
    // toward |initial_state|.
    bool is_initial_loading_spinner = IsInitialLoadSpinnerUpdate(stream_update);
    if (!initial_state && !is_initial_loading_spinner) {
      initial_state = stream_update;
    }
    update = stream_update;

    described_updates_.push_back(CurrentState());
  }
  void ReplaceDataStoreEntry(base::StringPiece key,
                             base::StringPiece data) override {
    data_store_entries_[key.as_string()] = data.as_string();
  }
  void RemoveDataStoreEntry(base::StringPiece key) override {
    data_store_entries_.erase(key.as_string());
  }

  // Test functions.

  void Clear() {
    initial_state = base::nullopt;
    update = base::nullopt;
    described_updates_.clear();
  }

  // Returns a description of the updates this surface received. Each update
  // is separated by ' -> '. Returns only the updates since the last call.
  std::string DescribeUpdates() {
    std::string result = base::JoinString(described_updates_, " -> ");
    described_updates_.clear();
    return result;
  }

  std::map<std::string, std::string> GetDataStoreEntries() const {
    return data_store_entries_;
  }

  // The initial state of the stream, if it was received. This is nullopt if
  // only the loading spinner was seen.
  base::Optional<feedui::StreamUpdate> initial_state;
  // The last stream update received.
  base::Optional<feedui::StreamUpdate> update;

 private:
  std::string CurrentState() {
    if (update && IsInitialLoadSpinnerUpdate(*update))
      return "loading";

    if (!initial_state)
      return "empty";

    bool has_loading_spinner = false;
    for (int i = 0; i < update->updated_slices().size(); ++i) {
      const feedui::StreamUpdate_SliceUpdate& slice_update =
          update->updated_slices(i);
      if (slice_update.has_slice() &&
          slice_update.slice().has_zero_state_slice()) {
        CHECK(update->updated_slices().size() == 1)
            << "Zero state with other slices" << *update;
        // Returns either "no-cards" or "cant-refresh".
        return update->updated_slices()[0].slice().slice_id();
      }
      if (slice_update.has_slice() &&
          slice_update.slice().has_loading_spinner_slice()) {
        CHECK_EQ(i, update->updated_slices().size() - 1)
            << "Loading spinner in an unexpected place" << *update;
        has_loading_spinner = true;
      }
    }
    std::stringstream ss;
    if (has_loading_spinner) {
      ss << update->updated_slices().size() - 1 << " slices +spinner";
    } else {
      ss << update->updated_slices().size() << " slices";
    }
    return ss.str();
  }

  bool IsInitialLoadSpinnerUpdate(const feedui::StreamUpdate& update) {
    return update.updated_slices().size() == 1 &&
           update.updated_slices()[0].has_slice() &&
           update.updated_slices()[0].slice().has_loading_spinner_slice();
  }

  // The stream if it was attached using the constructor.
  FeedStream* stream_ = nullptr;
  std::vector<std::string> described_updates_;
  std::map<std::string, std::string> data_store_entries_;
};

class TestForYouSurface : public TestSurfaceBase {
 public:
  explicit TestForYouSurface(FeedStream* stream = nullptr)
      : TestSurfaceBase(kInterestStream, stream) {}
};
class TestWebFeedSurface : public TestSurfaceBase {
 public:
  explicit TestWebFeedSurface(FeedStream* stream = nullptr)
      : TestSurfaceBase(kWebFeedStream, stream) {}
};

class TestImageFetcher : public ImageFetcher {
 public:
  explicit TestImageFetcher(
      scoped_refptr<::network::SharedURLLoaderFactory> url_loader_factory)
      : ImageFetcher(url_loader_factory) {}
  ImageFetchId Fetch(
      const GURL& url,
      base::OnceCallback<void(NetworkResponse)> callback) override {
    // Emulate a response.
    NetworkResponse response = {"dummyresponse", 200};
    std::move(callback).Run(std::move(response));
    return id_generator_.GenerateNextId();
  }
  void Cancel(ImageFetchId id) override {}

 private:
  ImageFetchId::Generator id_generator_;
};

class TestFeedNetwork : public FeedNetwork {
 public:
  // FeedNetwork implementation.
  void SendQueryRequest(
      NetworkRequestType request_type,
      const feedwire::Request& request,
      bool force_signed_out_request,
      base::OnceCallback<void(QueryRequestResult)> callback) override {
    forced_signed_out_request = force_signed_out_request;
    ++send_query_call_count;
    // Emulate a successful response.
    // The response body is currently an empty message, because most of the
    // time we want to inject a translated response for ease of test-writing.
    query_request_sent = request;
    QueryRequestResult result;
    result.response_info.status_code = 200;
    result.response_info.response_body_bytes = 100;
    result.response_info.fetch_duration = base::TimeDelta::FromMilliseconds(42);
    result.response_info.was_signed_in = true;
    if (injected_response_) {
      result.response_body = std::make_unique<feedwire::Response>(
          std::move(injected_response_.value()));
    } else {
      result.response_body = std::make_unique<feedwire::Response>();
    }
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
  }

  void SendDiscoverApiRequest(
      base::StringPiece api_path,
      base::StringPiece method,
      std::string request_bytes,
      base::OnceCallback<void(RawResponse)> callback) override {
    api_requests_sent_[api_path.as_string()] = request_bytes;
    ++api_request_count_[api_path.as_string()];
    std::vector<RawResponse>& injected_responses =
        injected_api_responses_[api_path.as_string()];

    // If there is no injected response, create a default response.
    if (injected_responses.empty()) {
      if (api_path == UploadActionsDiscoverApi::RequestPath()) {
        feedwire::UploadActionsRequest request;
        ASSERT_TRUE(request.ParseFromString(request_bytes));
        feedwire::UploadActionsResponse response_message;
        response_message.mutable_consistency_token()->set_token(
            consistency_token);
        InjectApiResponse<UploadActionsDiscoverApi>(response_message);
      }
    }

    if (!injected_responses.empty()) {
      RawResponse response = injected_responses[0];
      injected_responses.erase(injected_responses.begin());
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::move(response)));
      return;
    }
    ASSERT_TRUE(false)
        << "No API response injected, and no default is available:" << api_path;
  }

  void CancelRequests() override { NOTIMPLEMENTED(); }

  void InjectRealFeedQueryResponse() {
    base::FilePath response_file_path;
    CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &response_file_path));
    response_file_path = response_file_path.AppendASCII(
        "components/test/data/feed/response.binarypb");
    std::string response_data;
    CHECK(base::ReadFileToString(response_file_path, &response_data));

    feedwire::Response response;
    CHECK(response.ParseFromString(response_data));

    injected_response_ = response;
  }

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

  void InjectEmptyActionRequestResult() {
    InjectApiRawResponse<UploadActionsDiscoverApi>({});
  }

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

  base::Optional<feedwire::UploadActionsRequest> GetActionRequestSent() {
    return GetApiRequestSent<UploadActionsDiscoverApi>();
  }

  template <typename API>
  int GetApiRequestCount() const {
    auto iter = api_request_count_.find(API::RequestPath().as_string());
    return iter == api_request_count_.end() ? 0 : iter->second;
  }
  int GetActionRequestCount() const {
    return GetApiRequestCount<UploadActionsDiscoverApi>();
  }
  void ClearTestData() {
    injected_api_responses_.clear();
    api_requests_sent_.clear();
    api_request_count_.clear();
    injected_response_.reset();
  }

  base::Optional<feedwire::Request> query_request_sent;
  int send_query_call_count = 0;
  std::string consistency_token;
  bool forced_signed_out_request = false;

 private:
  std::map<std::string, std::vector<RawResponse>> injected_api_responses_;
  std::map<std::string, std::string> api_requests_sent_;
  std::map<std::string, int> api_request_count_;
  base::Optional<feedwire::Response> injected_response_;
};

// Forwards to |FeedStream::WireResponseTranslator| unless a response is
// injected.
class TestWireResponseTranslator : public FeedStream::WireResponseTranslator {
 public:
  RefreshResponseData TranslateWireResponse(
      feedwire::Response response,
      StreamModelUpdateRequest::Source source,
      bool was_signed_in_request,
      base::Time current_time) const override {
    if (!injected_responses_.empty()) {
      if (injected_responses_[0].model_update_request)
        injected_responses_[0].model_update_request->source = source;
      RefreshResponseData result = std::move(injected_responses_[0]);
      injected_responses_.erase(injected_responses_.begin());
      return result;
    }
    return FeedStream::WireResponseTranslator::TranslateWireResponse(
        std::move(response), source, was_signed_in_request, current_time);
  }
  void InjectResponse(std::unique_ptr<StreamModelUpdateRequest> response,
                      base::Optional<std::string> session_id = base::nullopt) {
    DCHECK(!response->stream_data.signed_in() || !session_id);
    RefreshResponseData data;
    data.model_update_request = std::move(response);
    data.session_id = std::move(session_id);
    InjectResponse(std::move(data));
  }
  void InjectResponse(RefreshResponseData response_data) {
    injected_responses_.push_back(std::move(response_data));
  }
  bool InjectedResponseConsumed() const { return injected_responses_.empty(); }

 private:
  mutable std::vector<RefreshResponseData> injected_responses_;
};

class FakeRefreshTaskScheduler : public RefreshTaskScheduler {
 public:
  // RefreshTaskScheduler implementation.
  void EnsureScheduled(base::TimeDelta run_time) override {
    scheduled_run_time = run_time;
  }
  void Cancel() override { canceled = true; }
  void RefreshTaskComplete() override { refresh_task_complete = true; }

  void Clear() {
    scheduled_run_time.reset();
    canceled = false;
    refresh_task_complete = false;
  }
  base::Optional<base::TimeDelta> scheduled_run_time;
  bool canceled = false;
  bool refresh_task_complete = false;
};

class TestMetricsReporter : public MetricsReporter {
 public:
  explicit TestMetricsReporter(PrefService* prefs) : MetricsReporter(prefs) {}

  // MetricsReporter.
  void ContentSliceViewed(SurfaceId surface_id, int index_in_stream) override {
    slice_viewed_index = index_in_stream;
    MetricsReporter::ContentSliceViewed(surface_id, index_in_stream);
  }
  void OnLoadStream(LoadStreamStatus load_from_store_status,
                    LoadStreamStatus final_status,
                    bool loaded_new_content_from_network,
                    base::TimeDelta stored_content_age,
                    std::unique_ptr<LoadLatencyTimes> latencies) override {
    load_stream_from_store_status = load_from_store_status;
    load_stream_status = final_status;
    LOG(INFO) << "OnLoadStream: " << final_status
              << " (store status: " << load_from_store_status << ")";
    MetricsReporter::OnLoadStream(load_from_store_status, final_status,
                                  loaded_new_content_from_network,
                                  stored_content_age, std::move(latencies));
  }
  void OnLoadMoreBegin(SurfaceId surface_id) override {
    load_more_surface_id = surface_id;
    MetricsReporter::OnLoadMoreBegin(surface_id);
  }
  void OnLoadMore(LoadStreamStatus final_status) override {
    load_more_status = final_status;
    MetricsReporter::OnLoadMore(final_status);
  }
  void OnBackgroundRefresh(LoadStreamStatus final_status) override {
    background_refresh_status = final_status;
    MetricsReporter::OnBackgroundRefresh(final_status);
  }
  void OnClearAll(base::TimeDelta time_since_last_clear) override {
    this->time_since_last_clear = time_since_last_clear;
    MetricsReporter::OnClearAll(time_since_last_clear);
  }
  void OnUploadActions(UploadActionsStatus status) override {
    upload_action_status = status;
    MetricsReporter::OnUploadActions(status);
  }

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
  TestPrefetchService() = default;
  // offline_pages::StubPrefetchService.
  void SetSuggestionProvider(
      offline_pages::SuggestionsProvider* suggestions_provider) override {
    suggestions_provider_ = suggestions_provider;
  }
  void NewSuggestionsAvailable() override {
    ++new_suggestions_available_call_count_;
  }

  // Test functionality.
  offline_pages::SuggestionsProvider* suggestions_provider() {
    return suggestions_provider_;
  }
  int NewSuggestionsAvailableCallCount() const {
    return new_suggestions_available_call_count_;
  }

 private:
  offline_pages::SuggestionsProvider* suggestions_provider_ = nullptr;
  int new_suggestions_available_call_count_ = 0;
};

class TestOfflinePageModel : public offline_pages::StubOfflinePageModel {
 public:
  // offline_pages::OfflinePageModel
  void AddObserver(Observer* observer) override {
    CHECK(observers_.insert(observer).second);
  }
  void RemoveObserver(Observer* observer) override {
    CHECK_EQ(1UL, observers_.erase(observer));
  }
  void GetPagesWithCriteria(
      const offline_pages::PageCriteria& criteria,
      offline_pages::MultipleOfflinePageItemCallback callback) override {
    std::vector<offline_pages::OfflinePageItem> result;
    for (const offline_pages::OfflinePageItem& item : items_) {
      if (MeetsCriteria(criteria, item)) {
        result.push_back(item);
      }
    }
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), result));
  }

  // Test functions.

  void AddTestPage(const GURL& url) {
    offline_pages::OfflinePageItem item;
    item.url = url;
    item.client_id =
        offline_pages::ClientId(offline_pages::kSuggestedArticlesNamespace, "");
    items_.push_back(item);
  }

  std::vector<offline_pages::OfflinePageItem>& items() { return items_; }

  void CallObserverOfflinePageAdded(
      const offline_pages::OfflinePageItem& item) {
    for (Observer* observer : observers_) {
      observer->OfflinePageAdded(this, item);
    }
  }

  void CallObserverOfflinePageDeleted(
      const offline_pages::OfflinePageItem& item) {
    for (Observer* observer : observers_) {
      observer->OfflinePageDeleted(item);
    }
  }

 private:
  std::vector<offline_pages::OfflinePageItem> items_;
  std::set<Observer*> observers_;
};

class FeedStreamTest : public testing::Test, public FeedStream::Delegate {
 public:
  void SetUp() override {
    SetupFeatures();
    kTestTimeEpoch = base::Time::Now();

    // Reset to default config, since tests can change it.
    SetFeedConfigForTesting(Config());

    feed::prefs::RegisterFeedSharedProfilePrefs(profile_prefs_.registry());
    feed::RegisterProfilePrefs(profile_prefs_.registry());
    metrics_reporter_ = std::make_unique<TestMetricsReporter>(&profile_prefs_);

    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_factory_);
    image_fetcher_ =
        std::make_unique<TestImageFetcher>(shared_url_loader_factory_);

    CreateStream();
  }

  virtual void SetupFeatures() {}

  void TearDown() override {
    // Ensure the task queue can return to idle. Failure to do so may be due
    // to a stuck task that never called |TaskComplete()|.
    WaitForIdleTaskQueue();
    // ProtoDatabase requires PostTask to clean up.
    store_.reset();
    persistent_key_value_store_.reset();
    task_environment_.RunUntilIdle();
  }

  // FeedStream::Delegate.
  bool IsEulaAccepted() override { return is_eula_accepted_; }
  bool IsOffline() override { return is_offline_; }
  DisplayMetrics GetDisplayMetrics() override {
    DisplayMetrics result;
    result.density = 200;
    result.height_pixels = 800;
    result.width_pixels = 350;
    return result;
  }
  std::string GetLanguageTag() override { return "en-US"; }
  void ClearAll() override {}
  bool IsSignedIn() override { return is_signed_in_; }
  void PrefetchImage(const GURL& url) override {
    prefetched_images_.push_back(url);
    prefetch_image_call_count_++;
  }
  void RegisterExperiments(const Experiments& experiments) override {}

  // For tests.

  // Replace stream_.
  void CreateStream() {
    ChromeInfo chrome_info;
    chrome_info.channel = version_info::Channel::STABLE;
    chrome_info.version = base::Version({99, 1, 9911, 2});
    stream_ = std::make_unique<FeedStream>(
        &refresh_scheduler_, metrics_reporter_.get(), this, &profile_prefs_,
        &network_, image_fetcher_.get(), store_.get(),
        persistent_key_value_store_.get(), &prefetch_service_,
        &offline_page_model_, chrome_info);

    WaitForIdleTaskQueue();  // Wait for any initialization.
    stream_->SetWireResponseTranslatorForTesting(&response_translator_);
  }

  bool IsTaskQueueIdle() const {
    return !stream_->GetTaskQueueForTesting()->HasPendingTasks() &&
           !stream_->GetTaskQueueForTesting()->HasRunningTask();
  }

  void WaitForIdleTaskQueue() {
    if (IsTaskQueueIdle())
      return;
    base::test::ScopedRunLoopTimeout run_timeout(
        FROM_HERE, base::TimeDelta::FromSeconds(1));
    base::RunLoop run_loop;
    stream_->SetIdleCallbackForTesting(run_loop.QuitClosure());
    run_loop.Run();
  }

  void UnloadModel() {
    WaitForIdleTaskQueue();
    stream_->UnloadModel(kInterestStream);
  }

  // Dumps the state of |FeedStore| to a string for debugging.
  std::string DumpStoreState(bool print_keys = false) {
    base::RunLoop run_loop;
    std::unique_ptr<std::map<std::string, feedstore::Record>> records;
    auto callback =
        [&](bool,
            std::unique_ptr<std::map<std::string, feedstore::Record>> result) {
          records = std::move(result);
          run_loop.Quit();
        };
    store_->GetDatabaseForTesting()->LoadKeysAndEntries(
        base::BindLambdaForTesting(callback));

    run_loop.Run();
    std::stringstream ss;
    for (const auto& item : *records) {
      if (print_keys)
        ss << '"' << item.first << "\": ";

      ss << item.second << '\n';
    }
    return ss.str();
  }

  void UploadActions(std::vector<feedwire::FeedAction> actions) {
    size_t actions_remaining = actions.size();
    for (feedwire::FeedAction& action : actions) {
      stream_->UploadAction(action, (--actions_remaining) == 0ul,
                            base::DoNothing());
    }
  }

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
  bool is_signed_in_ = true;
  base::test::ScopedFeatureList scoped_feature_list_;
  int prefetch_image_call_count_ = 0;
  std::vector<GURL> prefetched_images_;
};

class FeedStreamTestForAllStreamTypes
    : public FeedStreamTest,
      public ::testing::WithParamInterface<StreamType> {
 public:
  static StreamType GetStreamType() { return GetParam(); }
  class TestSurface : public TestSurfaceBase {
   public:
    explicit TestSurface(FeedStream* stream = nullptr)
        : TestSurfaceBase(FeedStreamTestForAllStreamTypes::GetStreamType(),
                          stream) {}
  };
};

class FeedStreamConditionalActionsUploadTest : public FeedStreamTest {
  void SetupFeatures() override {
    scoped_feature_list_.InitAndEnableFeature(
        feed::kInterestFeedV2ClicksAndViewsConditionalUpload);
  }
};

TEST_F(FeedStreamTest, IsArticlesListVisibleByDefault) {
  EXPECT_TRUE(stream_->IsArticlesListVisible());
}

TEST_F(FeedStreamTest, DoNotRefreshIfArticlesListIsHidden) {
  profile_prefs_.SetBoolean(prefs::kArticlesListVisible, false);
  stream_->InitializeScheduling();
  EXPECT_TRUE(refresh_scheduler_.canceled);

  stream_->ExecuteRefreshTask();
  EXPECT_TRUE(refresh_scheduler_.refresh_task_complete);
  EXPECT_EQ(LoadStreamStatus::kLoadNotAllowedArticlesListHidden,
            metrics_reporter_->background_refresh_status);
}

TEST_F(FeedStreamTest, BackgroundRefreshSuccess) {
  // Trigger a background refresh.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  stream_->ExecuteRefreshTask();
  WaitForIdleTaskQueue();

  // Verify the refresh happened and that we can load a stream without the
  // network.
  ASSERT_TRUE(refresh_scheduler_.refresh_task_complete);
  EXPECT_EQ(LoadStreamStatus::kLoadedFromNetwork,
            metrics_reporter_->background_refresh_status);
  EXPECT_TRUE(response_translator_.InjectedResponseConsumed());
  EXPECT_FALSE(stream_->GetModel(kInterestStream));
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  EXPECT_EQ("loading -> 2 slices", surface.DescribeUpdates());
  // Verify that prefetch service was informed.
  EXPECT_EQ(1, prefetch_service_.NewSuggestionsAvailableCallCount());
}

TEST_F(FeedStreamTest, BackgroundRefreshPrefetchesImages) {
  // Trigger a background refresh.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  stream_->ExecuteRefreshTask();
  EXPECT_EQ(0, prefetch_image_call_count_);
  WaitForIdleTaskQueue();

  std::vector<GURL> expected_fetches(
      {GURL("http://image0/"), GURL("http://favicon0/"), GURL("http://image1/"),
       GURL("http://favicon1/")});
  // Verify that images were prefetched.
  EXPECT_EQ(4, prefetch_image_call_count_);
  EXPECT_EQ(expected_fetches, prefetched_images_);
}

TEST_F(FeedStreamTest, BackgroundRefreshNotAttemptedWhenModelIsLoading) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  stream_->ExecuteRefreshTask();
  WaitForIdleTaskQueue();

  EXPECT_EQ(metrics_reporter_->background_refresh_status,
            LoadStreamStatus::kModelAlreadyLoaded);
}

TEST_F(FeedStreamTest, BackgroundRefreshNotAttemptedAfterModelIsLoaded) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  stream_->ExecuteRefreshTask();
  WaitForIdleTaskQueue();

  EXPECT_EQ(metrics_reporter_->background_refresh_status,
            LoadStreamStatus::kModelAlreadyLoaded);
}

TEST_F(FeedStreamTest, SurfaceReceivesInitialContent) {
  {
    auto model = std::make_unique<StreamModel>();
    model->Update(MakeTypicalInitialModelState());
    stream_->LoadModelForTesting(kInterestStream, std::move(model));
  }
  TestForYouSurface surface(stream_.get());
  ASSERT_TRUE(surface.initial_state);
  const feedui::StreamUpdate& initial_state = surface.initial_state.value();
  ASSERT_EQ(2, initial_state.updated_slices().size());
  EXPECT_NE("", initial_state.updated_slices(0).slice().slice_id());
  EXPECT_EQ("f:0", initial_state.updated_slices(0)
                       .slice()
                       .xsurface_slice()
                       .xsurface_frame());
  EXPECT_NE("", initial_state.updated_slices(1).slice().slice_id());
  EXPECT_EQ("f:1", initial_state.updated_slices(1)
                       .slice()
                       .xsurface_slice()
                       .xsurface_frame());
  ASSERT_EQ(1, initial_state.new_shared_states().size());
  EXPECT_EQ("ss:0",
            initial_state.new_shared_states()[0].xsurface_shared_state());
}

TEST_F(FeedStreamTest, SurfaceReceivesInitialContentLoadedAfterAttach) {
  TestForYouSurface surface(stream_.get());
  ASSERT_FALSE(surface.initial_state);
  {
    auto model = std::make_unique<StreamModel>();
    model->Update(MakeTypicalInitialModelState());
    stream_->LoadModelForTesting(kInterestStream, std::move(model));
  }

  ASSERT_EQ("loading -> 2 slices", surface.DescribeUpdates());
  const feedui::StreamUpdate& initial_state = surface.initial_state.value();

  EXPECT_NE("", initial_state.updated_slices(0).slice().slice_id());
  EXPECT_EQ("f:0", initial_state.updated_slices(0)
                       .slice()
                       .xsurface_slice()
                       .xsurface_frame());
  EXPECT_NE("", initial_state.updated_slices(1).slice().slice_id());
  EXPECT_EQ("f:1", initial_state.updated_slices(1)
                       .slice()
                       .xsurface_slice()
                       .xsurface_frame());
  ASSERT_EQ(1, initial_state.new_shared_states().size());
  EXPECT_EQ("ss:0",
            initial_state.new_shared_states()[0].xsurface_shared_state());
}

TEST_F(FeedStreamTest, SurfaceReceivesUpdatedContent) {
  {
    auto model = std::make_unique<StreamModel>();
    model->ExecuteOperations(MakeTypicalStreamOperations());
    stream_->LoadModelForTesting(kInterestStream, std::move(model));
  }
  TestForYouSurface surface(stream_.get());
  // Remove #1, add #2.
  stream_->ExecuteOperations(
      kInterestStream, {
                           MakeOperation(MakeRemove(MakeClusterId(1))),
                           MakeOperation(MakeCluster(2, MakeRootId())),
                           MakeOperation(MakeContentNode(2, MakeClusterId(2))),
                           MakeOperation(MakeContent(2)),
                       });
  ASSERT_TRUE(surface.update);
  const feedui::StreamUpdate& initial_state = surface.initial_state.value();
  const feedui::StreamUpdate& update = surface.update.value();

  ASSERT_EQ("2 slices -> 2 slices", surface.DescribeUpdates());
  // First slice is just an ID that matches the old 1st slice ID.
  EXPECT_EQ(initial_state.updated_slices(0).slice().slice_id(),
            update.updated_slices(0).slice_id());
  // Second slice is a new xsurface slice.
  EXPECT_NE("", update.updated_slices(1).slice().slice_id());
  EXPECT_EQ("f:2",
            update.updated_slices(1).slice().xsurface_slice().xsurface_frame());
}

TEST_F(FeedStreamTest, SurfaceReceivesSecondUpdatedContent) {
  {
    auto model = std::make_unique<StreamModel>();
    model->ExecuteOperations(MakeTypicalStreamOperations());
    stream_->LoadModelForTesting(kInterestStream, std::move(model));
  }
  TestForYouSurface surface(stream_.get());
  // Add #2.
  stream_->ExecuteOperations(
      kInterestStream, {
                           MakeOperation(MakeCluster(2, MakeRootId())),
                           MakeOperation(MakeContentNode(2, MakeClusterId(2))),
                           MakeOperation(MakeContent(2)),
                       });

  // Clear the last update and add #3.
  stream_->ExecuteOperations(
      kInterestStream, {
                           MakeOperation(MakeCluster(3, MakeRootId())),
                           MakeOperation(MakeContentNode(3, MakeClusterId(3))),
                           MakeOperation(MakeContent(3)),
                       });

  // The last update should have only one new piece of content.
  // This verifies the current content set is tracked properly.
  ASSERT_EQ("2 slices -> 3 slices -> 4 slices", surface.DescribeUpdates());

  ASSERT_EQ(4, surface.update->updated_slices().size());
  EXPECT_FALSE(surface.update->updated_slices(0).has_slice());
  EXPECT_FALSE(surface.update->updated_slices(1).has_slice());
  EXPECT_FALSE(surface.update->updated_slices(2).has_slice());
  EXPECT_EQ("f:3", surface.update->updated_slices(3)
                       .slice()
                       .xsurface_slice()
                       .xsurface_frame());
}

TEST_F(FeedStreamTest, RemoveAllContentResultsInZeroState) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Remove both pieces of content.
  stream_->ExecuteOperations(kInterestStream,
                             {
                                 MakeOperation(MakeRemove(MakeClusterId(0))),
                                 MakeOperation(MakeRemove(MakeClusterId(1))),
                             });

  ASSERT_EQ("loading -> 2 slices -> no-cards", surface.DescribeUpdates());
}

TEST_F(FeedStreamTest, DetachSurface) {
  {
    auto model = std::make_unique<StreamModel>();
    model->ExecuteOperations(MakeTypicalStreamOperations());
    stream_->LoadModelForTesting(kInterestStream, std::move(model));
  }
  TestForYouSurface surface(stream_.get());
  EXPECT_TRUE(surface.initial_state);
  surface.Detach();
  surface.Clear();

  // Arbitrary stream change. Surface should not see the update.
  stream_->ExecuteOperations(kInterestStream,
                             {
                                 MakeOperation(MakeRemove(MakeClusterId(1))),
                             });
  EXPECT_FALSE(surface.update);
}

TEST_F(FeedStreamTest, FetchImage) {
  CallbackReceiver<NetworkResponse> receiver;
  stream_->FetchImage(GURL("https://example.com"), receiver.Bind());

  EXPECT_EQ("dummyresponse", receiver.GetResult()->response_bytes);
}

TEST_P(FeedStreamTestForAllStreamTypes, LoadFromNetwork) {
  stream_->GetMetadata()->SetConsistencyToken("token");

  // Store is empty, so we should fallback to a network request.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_TRUE(network_.query_request_sent);
  EXPECT_EQ(
      "token",
      network_.query_request_sent->feed_request().consistency_token().token());
  EXPECT_TRUE(response_translator_.InjectedResponseConsumed());

  EXPECT_EQ("loading -> 2 slices", surface.DescribeUpdates());
  // Verify the model is filled correctly.
  EXPECT_STRINGS_EQUAL(
      ModelStateFor(MakeTypicalInitialModelState()),
      stream_->GetModel(GetStreamType())->DumpStateForTesting());
  // Verify the data was written to the store.
  EXPECT_STRINGS_EQUAL(ModelStateFor(MakeTypicalInitialModelState()),
                       ModelStateFor(GetStreamType(), store_.get()));
}

TEST_F(FeedStreamTest, ForceRefreshForDebugging) {
  // First do a normal load via network that will fail.
  is_offline_ = true;
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Next, force a refresh that results in a successful load.
  is_offline_ = false;
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  stream_->ForceRefreshForDebugging();

  WaitForIdleTaskQueue();
  EXPECT_EQ("loading -> cant-refresh -> loading -> 2 slices",
            surface.DescribeUpdates());
}

TEST_F(FeedStreamTest, RefreshScheduleFlow) {
  // Inject a typical network response, with a server-defined request schedule.
  {
    RequestSchedule schedule;
    schedule.anchor_time = kTestTimeEpoch;
    schedule.refresh_offsets = {base::TimeDelta::FromSeconds(12),
                                base::TimeDelta::FromSeconds(48)};
    RefreshResponseData response_data;
    response_data.model_update_request = MakeTypicalInitialModelState();
    response_data.request_schedule = schedule;

    response_translator_.InjectResponse(std::move(response_data));
  }
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Verify the first refresh was scheduled.
  EXPECT_EQ(base::TimeDelta::FromSeconds(12),
            refresh_scheduler_.scheduled_run_time);

  // Simulate executing the background task.
  refresh_scheduler_.Clear();
  task_environment_.AdvanceClock(base::TimeDelta::FromSeconds(12));
  stream_->ExecuteRefreshTask();
  WaitForIdleTaskQueue();

  // Verify |RefreshTaskComplete()| was called and next refresh was scheduled.
  EXPECT_TRUE(refresh_scheduler_.refresh_task_complete);
  ASSERT_TRUE(refresh_scheduler_.scheduled_run_time);
  EXPECT_EQ(base::TimeDelta::FromSeconds(48 - 12),
            *refresh_scheduler_.scheduled_run_time);

  // Simulate executing the background task again.
  refresh_scheduler_.Clear();
  task_environment_.AdvanceClock(base::TimeDelta::FromSeconds(48 - 12));
  stream_->ExecuteRefreshTask();
  WaitForIdleTaskQueue();

  // Verify |RefreshTaskComplete()| was called and next refresh was scheduled.
  EXPECT_TRUE(refresh_scheduler_.refresh_task_complete);
  ASSERT_TRUE(refresh_scheduler_.scheduled_run_time);
  EXPECT_EQ(GetFeedConfig().default_background_refresh_interval,
            *refresh_scheduler_.scheduled_run_time);
}

TEST_F(FeedStreamTest, ForceRefreshIfMissedScheduledRefresh) {
  // Inject a typical network response, with a server-defined request schedule.
  {
    RequestSchedule schedule;
    schedule.anchor_time = kTestTimeEpoch;
    schedule.refresh_offsets = {base::TimeDelta::FromSeconds(12),
                                base::TimeDelta::FromSeconds(48)};
    RefreshResponseData response_data;
    response_data.model_update_request = MakeTypicalInitialModelState();
    response_data.request_schedule = schedule;

    response_translator_.InjectResponse(std::move(response_data));
  }
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ(1, network_.send_query_call_count);
  surface.Detach();
  stream_->UnloadModel(surface.GetStreamType());

  // Ensure a refresh is foreced only after a scheduled refresh was missed.
  // First, load the stream after 11 seconds.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  task_environment_.AdvanceClock(base::TimeDelta::FromSeconds(11));
  surface.Attach(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ(1, network_.send_query_call_count);  // no refresh yet

  // Load the stream after 13 seconds. We missed the scheduled refresh at
  // 12 seconds.
  surface.Detach();
  stream_->UnloadModel(surface.GetStreamType());
  task_environment_.AdvanceClock(base::TimeDelta::FromSeconds(2));
  surface.Attach(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_EQ(2, network_.send_query_call_count);
  EXPECT_EQ(LoadStreamStatus::kDataInStoreStaleMissedLastRefresh,
            metrics_reporter_->load_stream_from_store_status);
}

TEST_P(FeedStreamTestForAllStreamTypes, LoadFromNetworkBecauseStoreIsStale) {
  // Fill the store with stream data that is just barely stale, and verify we
  // fetch new data over the network.
  store_->OverwriteStream(
      GetStreamType(),
      MakeTypicalInitialModelState(
          /*first_cluster_id=*/0, kTestTimeEpoch -
                                      GetFeedConfig().stale_content_threshold -
                                      base::TimeDelta::FromMinutes(1)),
      base::DoNothing());
  stream_->GetMetadata()->SetConsistencyToken("token-1");

  // Store is stale, so we should fallback to a network request.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_TRUE(network_.query_request_sent);
  // The stored continutation token should be sent.
  EXPECT_EQ(
      "token-1",
      network_.query_request_sent->feed_request().consistency_token().token());
  EXPECT_TRUE(response_translator_.InjectedResponseConsumed());
  ASSERT_TRUE(surface.initial_state);
}

// Same as LoadFromNetworkBecauseStoreIsStale, but with expired content.
TEST_F(FeedStreamTest, LoadFromNetworkBecauseStoreIsExpired) {
  base::HistogramTester histograms;
  const base::TimeDelta kContentAge =
      GetFeedConfig().content_expiration_threshold +
      base::TimeDelta::FromMinutes(1);
  store_->OverwriteStream(
      kInterestStream,
      MakeTypicalInitialModelState(
          /*first_cluster_id=*/0, kTestTimeEpoch - kContentAge),
      base::DoNothing());
  stream_->GetMetadata()->SetConsistencyToken("token-1");

  // Store is stale, so we should fallback to a network request.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_TRUE(network_.query_request_sent);
  // The stored continutation token should be sent.
  EXPECT_EQ(
      "token-1",
      network_.query_request_sent->feed_request().consistency_token().token());
  EXPECT_TRUE(response_translator_.InjectedResponseConsumed());
  ASSERT_TRUE(surface.initial_state);
  EXPECT_EQ(LoadStreamStatus::kDataInStoreIsExpired,
            metrics_reporter_->load_stream_from_store_status);
  histograms.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.ContentAgeOnLoad.BlockingRefresh", kContentAge,
      1);
}

TEST_F(FeedStreamTest, LoadStaleDataBecauseNetworkRequestFails) {
  // Fill the store with stream data that is just barely stale.
  base::HistogramTester histograms;
  const base::TimeDelta kContentAge =
      GetFeedConfig().stale_content_threshold + base::TimeDelta::FromMinutes(1);
  store_->OverwriteStream(
      kInterestStream,
      MakeTypicalInitialModelState(
          /*first_cluster_id=*/0, kTestTimeEpoch - kContentAge),
      base::DoNothing());
  stream_->GetMetadata()->SetConsistencyToken("token-1");

  // Store is stale, so we should fallback to a network request. Since we didn't
  // inject a network response, the network update will fail.
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_TRUE(network_.query_request_sent);
  EXPECT_EQ("loading -> 2 slices", surface.DescribeUpdates());
  EXPECT_EQ(LoadStreamStatus::kDataInStoreIsStale,
            metrics_reporter_->load_stream_from_store_status);
  EXPECT_EQ(LoadStreamStatus::kLoadedStaleDataFromStoreDueToNetworkFailure,
            metrics_reporter_->load_stream_status);
  histograms.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.ContentAgeOnLoad.NotRefreshed", kContentAge, 1);
}

TEST_P(FeedStreamTestForAllStreamTypes, LoadFailsStoredDataIsExpired) {
  // Fill the store with stream data that is just barely expired.
  store_->OverwriteStream(
      GetStreamType(),
      MakeTypicalInitialModelState(
          /*first_cluster_id=*/0,
          kTestTimeEpoch - GetFeedConfig().content_expiration_threshold -
              base::TimeDelta::FromMinutes(1)),
      base::DoNothing());

  // Store contains expired content, so we should fallback to a network request.
  // Since we didn't inject a network response, the network update will fail.
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_TRUE(network_.query_request_sent);
  EXPECT_EQ("loading -> cant-refresh", surface.DescribeUpdates());
  EXPECT_EQ(LoadStreamStatus::kDataInStoreIsExpired,
            metrics_reporter_->load_stream_from_store_status);
  EXPECT_EQ(LoadStreamStatus::kProtoTranslationFailed,
            metrics_reporter_->load_stream_status);
}

TEST_F(FeedStreamTest, LoadFromNetworkFailsDueToProtoTranslation) {
  // No data in the store, so we should fetch from the network.
  // The network will respond with an empty response, which should fail proto
  // translation.
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(LoadStreamStatus::kProtoTranslationFailed,
            metrics_reporter_->load_stream_status);
  EXPECT_EQ(0, prefetch_service_.NewSuggestionsAvailableCallCount());
}
TEST_F(FeedStreamTest, DoNotLoadFromNetworkWhenOffline) {
  is_offline_ = true;
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(LoadStreamStatus::kCannotLoadFromNetworkOffline,
            metrics_reporter_->load_stream_status);
  EXPECT_EQ("loading -> cant-refresh", surface.DescribeUpdates());
}

TEST_F(FeedStreamTest, DoNotLoadStreamWhenArticleListIsHidden) {
  profile_prefs_.SetBoolean(prefs::kArticlesListVisible, false);
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(LoadStreamStatus::kLoadNotAllowedArticlesListHidden,
            metrics_reporter_->load_stream_status);
  EXPECT_EQ("no-cards", surface.DescribeUpdates());
}

TEST_F(FeedStreamTest, DoNotLoadStreamWhenEulaIsNotAccepted) {
  is_eula_accepted_ = false;
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(LoadStreamStatus::kLoadNotAllowedEulaNotAccepted,
            metrics_reporter_->load_stream_status);
  EXPECT_EQ("no-cards", surface.DescribeUpdates());
}

TEST_F(FeedStreamTest, LoadStreamAfterEulaIsAccepted) {
  // Connect a surface before the EULA is accepted.
  is_eula_accepted_ = false;
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("no-cards", surface.DescribeUpdates());

  // Accept EULA, our surface should receive data.
  is_eula_accepted_ = true;
  stream_->OnEulaAccepted();
  WaitForIdleTaskQueue();

  EXPECT_EQ("loading -> 2 slices", surface.DescribeUpdates());
}

TEST_F(FeedStreamTest, ForceSignedOutRequestAfterHistoryIsDeleted) {
  stream_->OnAllHistoryDeleted();

  const std::string kSessionId = "session-id";

  // This test injects response post translation/parsing. We have to configure
  // the response data that should come out of the translator, which should
  // mark the request/response as having been made from the signed-out state.
  StreamModelUpdateRequestGenerator model_generator;
  model_generator.signed_in = false;

  // Advance the clock, but not past the end of the forced-signed-out period.
  task_environment_.FastForwardBy(kSuppressRefreshDuration -
                                  base::TimeDelta::FromSeconds(1));

  // Refresh the feed, queuing up a signed-out response.
  response_translator_.InjectResponse(model_generator.MakeFirstPage(),
                                      kSessionId);
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Validate that the network request was sent as signed out.
  ASSERT_EQ(1, network_.send_query_call_count);
  EXPECT_TRUE(network_.forced_signed_out_request);
  EXPECT_TRUE(network_.query_request_sent->feed_request()
                  .client_info()
                  .chrome_client_info()
                  .session_id()
                  .empty());

  // Validate the downstream consumption of the response.
  EXPECT_EQ("loading -> 2 slices", surface.DescribeUpdates());
  EXPECT_EQ(kSessionId, stream_->GetMetadata()->GetSessionIdToken());
  EXPECT_FALSE(stream_->GetModel(surface.GetStreamType())->signed_in());

  // Advance the clock beyond the forced signed out period.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(2));
  EXPECT_FALSE(stream_->GetModel(surface.GetStreamType())->signed_in());

  // Requests for subsequent pages continue the use existing session.
  // Subsequent responses may omit the session id.
  response_translator_.InjectResponse(model_generator.MakeNextPage());
  stream_->LoadMore(surface, base::DoNothing());
  WaitForIdleTaskQueue();

  // Validate that the network request was sent as signed out and
  // contained the session id.
  ASSERT_EQ(2, network_.send_query_call_count);
  EXPECT_TRUE(network_.forced_signed_out_request);
  EXPECT_EQ(kSessionId, stream_->GetMetadata()->GetSessionIdToken());
  EXPECT_EQ(network_.query_request_sent->feed_request()
                .client_info()
                .chrome_client_info()
                .session_id(),
            kSessionId);

  // The model should still be in the signed-out state.
  EXPECT_FALSE(stream_->GetModel(kInterestStream)->signed_in());

  // Force a refresh of the feed by clearing the cache. The request for the
  // first page should revert back to signed-in. The response data will denote
  // a signed-in response with no session_id.
  model_generator.signed_in = true;
  response_translator_.InjectResponse(model_generator.MakeFirstPage());
  stream_->OnCacheDataCleared();
  WaitForIdleTaskQueue();

  // Validate that a signed-in request was sent.
  ASSERT_EQ(3, network_.send_query_call_count);
  EXPECT_FALSE(network_.forced_signed_out_request);

  // The model should now be in the signed-in state.
  EXPECT_TRUE(stream_->GetModel(kInterestStream)->signed_in());
  EXPECT_TRUE(stream_->GetMetadata()->GetSessionIdToken().empty());
}

TEST_F(FeedStreamTest, AllowSignedInRequestAfterHistoryIsDeletedAfterDelay) {
  stream_->OnAllHistoryDeleted();
  task_environment_.FastForwardBy(kSuppressRefreshDuration +
                                  base::TimeDelta::FromSeconds(1));
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ("loading -> 2 slices", surface.DescribeUpdates());
  EXPECT_FALSE(network_.forced_signed_out_request);
  EXPECT_TRUE(stream_->GetMetadata()->GetSessionIdToken().empty());
}

TEST_F(FeedStreamTest, ShouldMakeFeedQueryRequestConsumesQuota) {
  LoadStreamStatus status = LoadStreamStatus::kNoStatus;
  for (; status == LoadStreamStatus::kNoStatus;
       status = stream_->ShouldMakeFeedQueryRequest(kInterestStream)) {
  }

  ASSERT_EQ(LoadStreamStatus::kCannotLoadFromNetworkThrottled, status);
}

TEST_F(FeedStreamTest, LoadStreamFromStore) {
  // Fill the store with stream data that is just barely fresh, and verify it
  // loads.
  store_->OverwriteStream(
      kInterestStream,
      MakeTypicalInitialModelState(
          /*first_cluster_id=*/0, kTestTimeEpoch -
                                      GetFeedConfig().stale_content_threshold +
                                      base::TimeDelta::FromMinutes(1)),
      base::DoNothing());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_EQ("loading -> 2 slices", surface.DescribeUpdates());
  EXPECT_FALSE(network_.query_request_sent);
  // Verify the model is filled correctly.
  EXPECT_STRINGS_EQUAL(
      ModelStateFor(MakeTypicalInitialModelState()),
      stream_->GetModel(kInterestStream)->DumpStateForTesting());
}

TEST_F(FeedStreamTest, LoadingSpinnerIsSentInitially) {
  store_->OverwriteStream(kInterestStream, MakeTypicalInitialModelState(),
                          base::DoNothing());
  TestForYouSurface surface(stream_.get());

  ASSERT_EQ("loading", surface.DescribeUpdates());
}

TEST_F(FeedStreamTest, DetachSurfaceWhileLoadingModel) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  surface.Detach();
  WaitForIdleTaskQueue();

  EXPECT_EQ("loading", surface.DescribeUpdates());
  EXPECT_TRUE(network_.query_request_sent);
}

TEST_F(FeedStreamTest, AttachMultipleSurfacesLoadsModelOnce) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  TestForYouSurface other_surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_EQ(1, network_.send_query_call_count);
  ASSERT_EQ("loading -> 2 slices", surface.DescribeUpdates());
  ASSERT_EQ("loading -> 2 slices", other_surface.DescribeUpdates());

  // After load, another surface doesn't trigger any tasks,
  // and immediately has content.
  TestForYouSurface later_surface(stream_.get());

  ASSERT_EQ("2 slices", later_surface.DescribeUpdates());
  EXPECT_TRUE(IsTaskQueueIdle());
}

TEST_P(FeedStreamTestForAllStreamTypes, ModelChangesAreSavedToStorage) {
  store_->OverwriteStream(GetStreamType(), MakeTypicalInitialModelState(),
                          base::DoNothing());
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_TRUE(surface.initial_state);

  // Remove #1, add #2.
  const std::vector<feedstore::DataOperation> operations = {
      MakeOperation(MakeRemove(MakeClusterId(1))),
      MakeOperation(MakeCluster(2, MakeRootId())),
      MakeOperation(MakeContentNode(2, MakeClusterId(2))),
      MakeOperation(MakeContent(2)),
  };
  stream_->ExecuteOperations(GetStreamType(), operations);

  WaitForIdleTaskQueue();

  // Verify changes are applied to storage.
  EXPECT_STRINGS_EQUAL(
      ModelStateFor(MakeTypicalInitialModelState(), operations),
      ModelStateFor(GetStreamType(), store_.get()));

  // Unload and reload the model from the store, and verify we can still apply
  // operations correctly.
  surface.Detach();
  surface.Clear();
  UnloadModel();
  surface.Attach(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_TRUE(surface.initial_state);

  // Remove #2, add #3.
  const std::vector<feedstore::DataOperation> operations2 = {
      MakeOperation(MakeRemove(MakeClusterId(2))),
      MakeOperation(MakeCluster(3, MakeRootId())),
      MakeOperation(MakeContentNode(3, MakeClusterId(3))),
      MakeOperation(MakeContent(3)),
  };
  stream_->ExecuteOperations(surface.GetStreamType(), operations2);

  WaitForIdleTaskQueue();
  EXPECT_STRINGS_EQUAL(
      ModelStateFor(MakeTypicalInitialModelState(), operations, operations2),
      ModelStateFor(GetStreamType(), store_.get()));
}

TEST_F(FeedStreamTest, ReportSliceViewedIdentifiesCorrectIndex) {
  store_->OverwriteStream(kInterestStream, MakeTypicalInitialModelState(),
                          base::DoNothing());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  stream_->ReportSliceViewed(
      surface.GetSurfaceId(), surface.GetStreamType(),
      surface.initial_state->updated_slices(1).slice().slice_id());
  EXPECT_EQ(1, metrics_reporter_->slice_viewed_index);
}

TEST_P(FeedStreamTestForAllStreamTypes, LoadMoreAppendsContent) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("loading -> 2 slices", surface.DescribeUpdates());
  EXPECT_EQ(1, prefetch_service_.NewSuggestionsAvailableCallCount());

  // Load page 2.
  response_translator_.InjectResponse(MakeTypicalNextPageState(2));
  CallbackReceiver<bool> callback;
  stream_->LoadMore(surface, callback.Bind());
  // Ensure metrics reporter was informed at the start of the operation.
  EXPECT_EQ(surface.GetSurfaceId(), metrics_reporter_->load_more_surface_id);
  WaitForIdleTaskQueue();
  ASSERT_EQ(base::Optional<bool>(true), callback.GetResult());
  EXPECT_EQ("2 slices +spinner -> 4 slices", surface.DescribeUpdates());
  EXPECT_EQ(2, prefetch_service_.NewSuggestionsAvailableCallCount());

  // Load page 3.
  response_translator_.InjectResponse(MakeTypicalNextPageState(3));
  stream_->LoadMore(surface, callback.Bind());

  WaitForIdleTaskQueue();
  ASSERT_EQ(base::Optional<bool>(true), callback.GetResult());
  EXPECT_EQ("4 slices +spinner -> 6 slices", surface.DescribeUpdates());
  EXPECT_EQ(3, prefetch_service_.NewSuggestionsAvailableCallCount());
}

TEST_P(FeedStreamTestForAllStreamTypes, LoadMorePersistsData) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("loading -> 2 slices", surface.DescribeUpdates());

  // Load page 2.
  response_translator_.InjectResponse(MakeTypicalNextPageState(2));
  CallbackReceiver<bool> callback;
  stream_->LoadMore(surface, callback.Bind());

  WaitForIdleTaskQueue();
  ASSERT_EQ(base::Optional<bool>(true), callback.GetResult());

  // Verify stored state is equivalent to in-memory model.
  EXPECT_STRINGS_EQUAL(
      stream_->GetModel(GetStreamType())->DumpStateForTesting(),
      ModelStateFor(GetStreamType(), store_.get()));
}

TEST_F(FeedStreamTest, LoadMorePersistAndLoadMore) {
  // Verify we can persist a LoadMore, and then do another LoadMore after
  // reloading state.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("loading -> 2 slices", surface.DescribeUpdates());

  // Load page 2.
  response_translator_.InjectResponse(MakeTypicalNextPageState(2));
  CallbackReceiver<bool> callback;
  stream_->LoadMore(surface, callback.Bind());
  WaitForIdleTaskQueue();
  ASSERT_EQ(base::Optional<bool>(true), callback.GetResult());

  surface.Detach();
  UnloadModel();

  // Load page 3.
  surface.Attach(stream_.get());
  response_translator_.InjectResponse(MakeTypicalNextPageState(3));
  WaitForIdleTaskQueue();
  callback.Clear();
  surface.Clear();
  stream_->LoadMore(surface, callback.Bind());
  WaitForIdleTaskQueue();

  ASSERT_EQ(base::Optional<bool>(true), callback.GetResult());
  ASSERT_EQ("4 slices +spinner -> 6 slices", surface.DescribeUpdates());
  // Verify stored state is equivalent to in-memory model.
  EXPECT_STRINGS_EQUAL(
      stream_->GetModel(surface.GetStreamType())->DumpStateForTesting(),
      ModelStateFor(kInterestStream, store_.get()));
}

TEST_F(FeedStreamTest, LoadMoreSendsTokens) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("loading -> 2 slices", surface.DescribeUpdates());

  stream_->GetMetadata()->SetConsistencyToken("token-1");
  response_translator_.InjectResponse(MakeTypicalNextPageState(2));
  CallbackReceiver<bool> callback;
  stream_->LoadMore(surface, callback.Bind());

  WaitForIdleTaskQueue();
  ASSERT_EQ("2 slices +spinner -> 4 slices", surface.DescribeUpdates());

  EXPECT_EQ(
      "token-1",
      network_.query_request_sent->feed_request().consistency_token().token());
  EXPECT_EQ("page-2", network_.query_request_sent->feed_request()
                          .feed_query()
                          .next_page_token()
                          .next_page_token()
                          .next_page_token());

  stream_->GetMetadata()->SetConsistencyToken("token-2");
  response_translator_.InjectResponse(MakeTypicalNextPageState(3));
  stream_->LoadMore(surface, callback.Bind());

  WaitForIdleTaskQueue();
  ASSERT_EQ("4 slices +spinner -> 6 slices", surface.DescribeUpdates());

  EXPECT_EQ(
      "token-2",
      network_.query_request_sent->feed_request().consistency_token().token());
  EXPECT_EQ("page-3", network_.query_request_sent->feed_request()
                          .feed_query()
                          .next_page_token()
                          .next_page_token()
                          .next_page_token());
}

TEST_F(FeedStreamTest, LoadMoreAbortsIfNoNextPageToken) {
  {
    std::unique_ptr<StreamModelUpdateRequest> initial_state =
        MakeTypicalInitialModelState();
    initial_state->stream_data.clear_next_page_token();
    response_translator_.InjectResponse(std::move(initial_state));
  }
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  CallbackReceiver<bool> callback;
  stream_->LoadMore(surface, callback.Bind());
  WaitForIdleTaskQueue();

  // LoadMore fails, and does not make an additional request.
  EXPECT_EQ(base::Optional<bool>(false), callback.GetResult());
  ASSERT_EQ(1, network_.send_query_call_count);
  EXPECT_EQ("loading -> 2 slices", surface.DescribeUpdates());
  EXPECT_EQ(base::nullopt, metrics_reporter_->load_more_surface_id)
      << "metrics reporter was informed about a load more operation which "
         "didn't begin";
}

TEST_F(FeedStreamTest, LoadMoreFail) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("loading -> 2 slices", surface.DescribeUpdates());

  // Don't inject another response, which results in a proto translation
  // failure.
  CallbackReceiver<bool> callback;
  stream_->LoadMore(surface, callback.Bind());
  WaitForIdleTaskQueue();

  EXPECT_EQ(base::Optional<bool>(false), callback.GetResult());
  EXPECT_EQ("2 slices +spinner -> 2 slices", surface.DescribeUpdates());
}

TEST_F(FeedStreamTest, LoadMoreWithClearAllInResponse) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("loading -> 2 slices", surface.DescribeUpdates());

  // Use a different initial state (which includes a CLEAR_ALL).
  response_translator_.InjectResponse(MakeTypicalInitialModelState(5));
  CallbackReceiver<bool> callback;
  stream_->LoadMore(surface, callback.Bind());

  WaitForIdleTaskQueue();
  ASSERT_EQ(base::Optional<bool>(true), callback.GetResult());

  // Verify stored state is equivalent to in-memory model.
  EXPECT_STRINGS_EQUAL(
      stream_->GetModel(surface.GetStreamType())->DumpStateForTesting(),
      ModelStateFor(kInterestStream, store_.get()));

  // Verify the new state has been pushed to |surface|.
  ASSERT_EQ("2 slices +spinner -> 2 slices", surface.DescribeUpdates());

  const feedui::StreamUpdate& initial_state = surface.update.value();
  ASSERT_EQ(2, initial_state.updated_slices().size());
  EXPECT_NE("", initial_state.updated_slices(0).slice().slice_id());
  EXPECT_EQ("f:5", initial_state.updated_slices(0)
                       .slice()
                       .xsurface_slice()
                       .xsurface_frame());
  EXPECT_NE("", initial_state.updated_slices(1).slice().slice_id());
  EXPECT_EQ("f:6", initial_state.updated_slices(1)
                       .slice()
                       .xsurface_slice()
                       .xsurface_frame());
}

TEST_F(FeedStreamTest, LoadMoreBeforeLoad) {
  CallbackReceiver<bool> callback;
  TestForYouSurface surface;
  stream_->LoadMore(surface, callback.Bind());

  EXPECT_EQ(base::Optional<bool>(false), callback.GetResult());
}

TEST_F(FeedStreamTest, ReadNetworkResponse) {
  base::HistogramTester histograms;
  network_.InjectRealFeedQueryResponse();
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_EQ("loading -> 10 slices", surface.DescribeUpdates());

  // Verify we're processing some of the data on the request.

  // The response has a privacy_notice_fulfilled=true.
  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.ActivityLoggingEnabled", 1, 1);

  // A request schedule with two entries was in the response. The first entry
  // should have already been scheduled/consumed, leaving only the second
  // entry still in the the refresh_offsets vector.
  RequestSchedule schedule = prefs::GetRequestSchedule(profile_prefs_);
  EXPECT_EQ(std::vector<base::TimeDelta>({
                base::TimeDelta::FromSeconds(86308) +
                    base::TimeDelta::FromNanoseconds(822963644),
                base::TimeDelta::FromSeconds(120000),
            }),
            schedule.refresh_offsets);

  // The stream's user attributes are set, so activity logging is enabled.
  EXPECT_TRUE(stream_->IsActivityLoggingEnabled());
  EXPECT_EQ(1, prefetch_service_.NewSuggestionsAvailableCallCount());
}

TEST_F(FeedStreamTest, ClearAllAfterLoadResultsInRefresh) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  stream_->OnCacheDataCleared();  // triggers ClearAll().

  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  WaitForIdleTaskQueue();

  EXPECT_EQ("loading -> 2 slices -> loading -> 2 slices",
            surface.DescribeUpdates());
}

TEST_F(FeedStreamTest, ClearAllWithNoSurfacesAttachedDoesNotReload) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  surface.Detach();

  stream_->OnCacheDataCleared();  // triggers ClearAll().
  WaitForIdleTaskQueue();

  EXPECT_EQ("loading -> 2 slices", surface.DescribeUpdates());
}

TEST_F(FeedStreamTest, ClearAllWhileLoadingMore) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  stream_->LoadMore(surface, base::DoNothing());
  response_translator_.InjectResponse(MakeTypicalNextPageState(2));
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  stream_->OnCacheDataCleared();  // triggers ClearAll().
  WaitForIdleTaskQueue();

  EXPECT_EQ(
      "loading -> 2 slices -> 2 slices +spinner -> 4 slices -> loading -> 2 "
      "slices",
      surface.DescribeUpdates());
}

TEST_F(FeedStreamTest, ClearAllWipesAllState) {
  // Trigger saving a consistency token, so it can be cleared later.
  network_.consistency_token = "token-11";
  stream_->UploadAction(MakeFeedAction(42ul), true, base::DoNothing());
  // Trigger saving a feed stream, so it can be cleared later.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Enqueue an action, so it can be cleared later.
  stream_->UploadAction(MakeFeedAction(43ul), false, base::DoNothing());

  // Trigger ClearAll, this should erase everything.
  stream_->OnCacheDataCleared();
  WaitForIdleTaskQueue();

  ASSERT_EQ("loading -> 2 slices -> loading -> cant-refresh",
            surface.DescribeUpdates());

  EXPECT_EQ("", DumpStoreState());
  EXPECT_EQ("", stream_->GetMetadata()->GetConsistencyToken());
  EXPECT_FALSE(stream_->IsActivityLoggingEnabled());
}

TEST_F(FeedStreamTest, StorePendingAction) {
  stream_->UploadAction(MakeFeedAction(42ul), false, base::DoNothing());
  WaitForIdleTaskQueue();

  std::vector<feedstore::StoredAction> result =
      ReadStoredActions(stream_->GetStore());
  ASSERT_EQ(1ul, result.size());
  EXPECT_EQ(42ul, result[0].action().content_id().id());
}

TEST_F(FeedStreamTest, UploadActionWhileSignedOutIsNoOp) {
  is_signed_in_ = false;
  stream_->UploadAction(MakeFeedAction(42ul), false, base::DoNothing());
  WaitForIdleTaskQueue();

  EXPECT_EQ(0ul, ReadStoredActions(stream_->GetStore()).size());
}

TEST_F(FeedStreamTest, SignOutWhileUploadActionDoesNotUpload) {
  stream_->UploadAction(MakeFeedAction(42ul), true, base::DoNothing());
  is_signed_in_ = false;

  WaitForIdleTaskQueue();

  EXPECT_EQ(UploadActionsStatus::kAbortUploadForSignedOutUser,
            metrics_reporter_->upload_action_status);
  EXPECT_EQ(0, network_.GetActionRequestCount());
}

TEST_F(FeedStreamTest, StorePendingActionAndUploadNow) {
  network_.consistency_token = "token-11";

  // Call |ProcessThereAndBackAgain()|, which triggers Upload() with
  // upload_now=true.
  {
    feedwire::ThereAndBackAgainData msg;
    *msg.mutable_action_payload() = MakeFeedAction(42ul).action_payload();
    stream_->ProcessThereAndBackAgain(msg.SerializeAsString());
  }
  WaitForIdleTaskQueue();

  // Verify the action was uploaded.
  EXPECT_EQ(1, network_.GetActionRequestCount());
  std::vector<feedstore::StoredAction> result =
      ReadStoredActions(stream_->GetStore());
  ASSERT_EQ(0ul, result.size());
}

TEST_F(FeedStreamTest, ProcessViewActionResultsInDelayedUpload) {
  network_.consistency_token = "token-11";

  stream_->ProcessViewAction(MakeFeedAction(42ul).SerializeAsString());
  WaitForIdleTaskQueue();
  // Verify it's not uploaded immediately.
  ASSERT_EQ(0, network_.GetActionRequestCount());

  // Trigger a network refresh.
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Verify the action was uploaded.
  EXPECT_EQ(1, network_.GetActionRequestCount());
}

TEST_F(FeedStreamTest, ActionsUploadWithoutConditionsWhenFeatureDisabled) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());

  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  stream_->ProcessViewAction(
      feedwire::FeedAction::default_instance().SerializeAsString());
  WaitForIdleTaskQueue();
  stream_->ProcessThereAndBackAgain(
      MakeThereAndBackAgainData(42ul).SerializeAsString());
  WaitForIdleTaskQueue();

  // Verify the actions were uploaded.
  ASSERT_EQ(1, network_.GetActionRequestCount());
  EXPECT_EQ(2, network_.GetActionRequestSent()->feed_actions_size());
}

TEST_F(FeedStreamConditionalActionsUploadTest,
       NoActionsUploadUntilReachedConditions) {
  // Tests the flow where we:
  //   (1) Perform a ThereAndBackAgain action and a View action while upload
  //   isn't enabled => (2) Attempt an upload while the upload conditions aren't
  //   reached => (3) Reach upload conditions => (4) Perform another View action
  //   that should be dropped => (5) Simulate the backgrounding of the app to
  //   enable actions upload => (6) Trigger an upload which will upload the
  //   stored ThereAndBackAgain action.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Process the view action and the ThereAndBackAgain action while the upload
  // conditions aren't reached.
  stream_->ProcessViewAction(MakeFeedAction(42ul).SerializeAsString());
  WaitForIdleTaskQueue();
  // Verify that the view action was dropped.
  ASSERT_EQ(0ul, ReadStoredActions(stream_->GetStore()).size());

  stream_->ProcessThereAndBackAgain(
      MakeThereAndBackAgainData(42ul).SerializeAsString());
  WaitForIdleTaskQueue();
  // Verify that the ThereAndBackAgain action is in the action store.
  ASSERT_EQ(1ul, ReadStoredActions(stream_->GetStore()).size());

  // Attempt an upload.
  stream_->OnEnterBackground();
  WaitForIdleTaskQueue();
  // Verify that no upload is done because the conditions aren't reached.
  EXPECT_EQ(0, network_.GetActionRequestCount());

  // Reach conditions.
  stream_->ReportSliceViewed(
      surface.GetSurfaceId(), surface.GetStreamType(),
      surface.initial_state->updated_slices(1).slice().slice_id());

  // Verify that the view action is still dropped because we haven't
  // transitioned out of the current surface.
  stream_->ProcessViewAction(MakeFeedAction(42ul).SerializeAsString());
  WaitForIdleTaskQueue();
  ASSERT_EQ(1ul, ReadStoredActions(stream_->GetStore()).size());

  // Enable the upload bit and trigger the upload of actions.
  stream_->OnEnterBackground();
  WaitForIdleTaskQueue();

  // Verify that the ThereAndBackAgain action was uploaded but not the view
  // action.
  ASSERT_EQ(1, network_.GetActionRequestCount());
  EXPECT_EQ(1, network_.GetActionRequestSent()->feed_actions_size());
}

TEST_F(FeedStreamConditionalActionsUploadTest, EnableUploadOnSurfaceAttached) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Perform a ThereAndBackAgain action.
  stream_->ProcessThereAndBackAgain(
      MakeThereAndBackAgainData(42ul).SerializeAsString());
  WaitForIdleTaskQueue();

  // Reach conditions.
  stream_->ReportSliceViewed(
      surface.GetSurfaceId(), surface.GetStreamType(),
      surface.initial_state->updated_slices(1).slice().slice_id());

  // Attach a new surface to update the bit to enable uploads.
  TestForYouSurface surface2(stream_.get());

  // Trigger an upload through load more to isolate the effect of the on-attach
  // event on enabling uploads.
  response_translator_.InjectResponse(MakeTypicalNextPageState());
  stream_->LoadMore(surface, base::DoNothing());
  WaitForIdleTaskQueue();

  // Verify that the ThereAndBackAgain action was uploaded.
  ASSERT_EQ(1, network_.GetActionRequestCount());
  EXPECT_EQ(1, network_.GetActionRequestSent()->feed_actions_size());
}

TEST_F(FeedStreamConditionalActionsUploadTest, EnableUploadOnEnterBackground) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Perform a ThereAndBackAgain action.
  stream_->ProcessThereAndBackAgain(
      MakeThereAndBackAgainData(42ul).SerializeAsString());
  WaitForIdleTaskQueue();

  // Reach conditions.
  stream_->ReportSliceViewed(
      surface.GetSurfaceId(), surface.GetStreamType(),
      surface.initial_state->updated_slices(1).slice().slice_id());

  stream_->OnEnterBackground();
  WaitForIdleTaskQueue();

  // Verify that the ThereAndBackAgain action was uploaded.
  ASSERT_EQ(1, network_.GetActionRequestCount());
  EXPECT_EQ(1, network_.GetActionRequestSent()->feed_actions_size());
}

TEST_F(FeedStreamConditionalActionsUploadTest,
       AllowActionsUploadWhenNoticeCardNotPresentRegardlessOfConditions) {
  auto model_state = MakeTypicalInitialModelState();
  model_state->stream_data.set_privacy_notice_fulfilled(false);
  response_translator_.InjectResponse(std::move(model_state));
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Process the view action and the ThereAndBackAgain action while the upload
  // conditions aren't reached.
  stream_->ProcessViewAction(MakeFeedAction(42ul).SerializeAsString());
  WaitForIdleTaskQueue();
  stream_->ProcessThereAndBackAgain(
      MakeThereAndBackAgainData(42ul).SerializeAsString());
  WaitForIdleTaskQueue();

  // Trigger an upload through a query.
  stream_->OnEnterBackground();
  WaitForIdleTaskQueue();

  // Verify the ThereAndBackAgain action and the view action were uploaded.
  ASSERT_EQ(1, network_.GetActionRequestCount());
  EXPECT_EQ(2, network_.GetActionRequestSent()->feed_actions_size());
}

TEST_F(FeedStreamConditionalActionsUploadTest,
       ReupdateUploadEnableBitsOnSignIn) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Reach conditions.
  stream_->ReportSliceViewed(
      surface.GetSurfaceId(), surface.GetStreamType(),
      surface.initial_state->updated_slices(1).slice().slice_id());

  // Assert that uploads are not yet enabled.
  ASSERT_FALSE(stream_->CanUploadActions());

  // Update the upload enable bits which will enable upload because the related
  // pref is true.
  stream_->OnSignedIn();

  EXPECT_TRUE(stream_->CanUploadActions());
}

TEST_F(FeedStreamConditionalActionsUploadTest,
       ResetTheUploadEnableBitsOnSignOut) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Reach conditions.
  stream_->ReportSliceViewed(
      surface.GetSurfaceId(), surface.GetStreamType(),
      surface.initial_state->updated_slices(1).slice().slice_id());

  // Update the upload enable bits which will enable upload.
  stream_->OnSignedOut();

  ASSERT_TRUE(stream_->CanUploadActions());
}

TEST_F(FeedStreamTest, LoadStreamUpdateNoticeCardFulfillmentHistogram) {
  base::HistogramTester histograms;

  // Trigger a stream refresh that updates the histogram.
  {
    auto model_state = MakeTypicalInitialModelState();
    model_state->stream_data.set_privacy_notice_fulfilled(false);
    response_translator_.InjectResponse(std::move(model_state));

    refresh_scheduler_.Clear();
    stream_->ExecuteRefreshTask();
    WaitForIdleTaskQueue();
  }

  UnloadModel();

  // Trigger another stream refresh that updates the histogram.
  {
    auto model_state = MakeTypicalInitialModelState();
    model_state->stream_data.set_privacy_notice_fulfilled(true);
    response_translator_.InjectResponse(std::move(model_state));

    refresh_scheduler_.Clear();
    stream_->ExecuteRefreshTask();
    WaitForIdleTaskQueue();
  }

  // Verify that the notice card fulfillment histogram was properly recorded.
  histograms.ExpectBucketCount("ContentSuggestions.Feed.NoticeCardFulfilled2",
                               0, 1);
  histograms.ExpectBucketCount("ContentSuggestions.Feed.NoticeCardFulfilled2",
                               1, 1);
}

TEST_F(FeedStreamConditionalActionsUploadTest,
       DontTriggerActionsUploadWhenWasNotSignedIn) {
  auto update_request = MakeTypicalInitialModelState();
  update_request->stream_data.set_signed_in(false);
  response_translator_.InjectResponse(std::move(update_request));
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Try to reach conditions.
  stream_->ReportSliceViewed(
      surface.GetSurfaceId(), surface.GetStreamType(),
      surface.initial_state->updated_slices(1).slice().slice_id());

  // Try to trigger an upload through a query.
  stream_->OnEnterBackground();
  WaitForIdleTaskQueue();

  // Verify that even if the conditions were reached, the pref that enables the
  // upload wasn't set to true because the latest refresh request wasn't signed
  // in.
  ASSERT_FALSE(stream_->CanUploadActions());
}

TEST_F(FeedStreamTest, LoadStreamFromNetworkUploadsActions) {
  stream_->UploadAction(MakeFeedAction(99ul), false, base::DoNothing());
  WaitForIdleTaskQueue();

  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(1, network_.GetActionRequestCount());
  EXPECT_EQ(1, network_.GetActionRequestSent()->feed_actions_size());

  // Uploaded action should have been erased from store.
  stream_->UploadAction(MakeFeedAction(100ul), true, base::DoNothing());
  WaitForIdleTaskQueue();
  EXPECT_EQ(2, network_.GetActionRequestCount());
  EXPECT_EQ(1, network_.GetActionRequestSent()->feed_actions_size());
}

TEST_F(FeedStreamTest, UploadedActionsHaveSequentialNumbers) {
  // Send 3 actions.
  stream_->UploadAction(MakeFeedAction(1ul), false, base::DoNothing());
  stream_->UploadAction(MakeFeedAction(2ul), false, base::DoNothing());
  stream_->UploadAction(MakeFeedAction(3ul), true, base::DoNothing());
  WaitForIdleTaskQueue();
  ASSERT_EQ(1, network_.GetActionRequestCount());
  feedwire::UploadActionsRequest request1 = *network_.GetActionRequestSent();

  // Send another action in a new request.
  stream_->UploadAction(MakeFeedAction(4ul), true, base::DoNothing());
  WaitForIdleTaskQueue();
  ASSERT_EQ(2, network_.GetActionRequestCount());
  feedwire::UploadActionsRequest request2 = *network_.GetActionRequestSent();

  // Verify that sent actions have sequential numbers.
  ASSERT_EQ(3, request1.feed_actions_size());
  ASSERT_EQ(1, request2.feed_actions_size());

  EXPECT_EQ(1, request1.feed_actions(0).client_data().sequence_number());
  EXPECT_EQ(2, request1.feed_actions(1).client_data().sequence_number());
  EXPECT_EQ(3, request1.feed_actions(2).client_data().sequence_number());
  EXPECT_EQ(4, request2.feed_actions(0).client_data().sequence_number());
}

TEST_F(FeedStreamTest, LoadMoreUploadsActions) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  stream_->UploadAction(MakeFeedAction(99ul), false, base::DoNothing());
  WaitForIdleTaskQueue();

  network_.consistency_token = "token-12";

  stream_->LoadMore(surface, base::DoNothing());
  WaitForIdleTaskQueue();

  EXPECT_EQ(1, network_.GetActionRequestSent()->feed_actions_size());
  EXPECT_EQ("token-12", stream_->GetMetadata()->GetConsistencyToken());

  // Uploaded action should have been erased from the store.
  network_.ClearTestData();
  stream_->UploadAction(MakeFeedAction(100ul), true, base::DoNothing());
  WaitForIdleTaskQueue();
  EXPECT_EQ(1, network_.GetActionRequestSent()->feed_actions_size());
  EXPECT_EQ(100ul,
            network_.GetActionRequestSent()->feed_actions(0).content_id().id());
}

TEST_F(FeedStreamTest, LoadMoreUpdatesIsActivityLoggingEnabled) {
  EXPECT_FALSE(stream_->IsActivityLoggingEnabled());
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  EXPECT_TRUE(stream_->IsActivityLoggingEnabled());

  int page = 2;
  for (bool signed_in : {true, false}) {
    for (bool waa_on : {true, false}) {
      for (bool privacy_notice_fulfilled : {true, false}) {
        response_translator_.InjectResponse(
            MakeTypicalNextPageState(page++, kTestTimeEpoch, signed_in, waa_on,
                                     privacy_notice_fulfilled));
        CallbackReceiver<bool> callback;
        stream_->LoadMore(surface, callback.Bind());
        WaitForIdleTaskQueue();
        EXPECT_EQ(
            stream_->IsActivityLoggingEnabled(),
            (signed_in && waa_on) ||
                (!signed_in && GetFeedConfig().send_signed_out_session_logs))
            << "signed_in=" << signed_in << " waa_on=" << waa_on
            << " privacy_notice_fulfilled=" << privacy_notice_fulfilled
            << " send_signed_out_session_logs="
            << GetFeedConfig().send_signed_out_session_logs;
      }
    }
  }
}

TEST_F(FeedStreamConditionalActionsUploadTest,
       LoadMoreDoesntUpdateNoticeCardPrefAndHistogram) {
  // The initial stream load has the notice card.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Inject a response for the LoadMore fetch that doesn't have the notice card.
  // It shouldn't overwrite the notice card pref.
  response_translator_.InjectResponse(MakeTypicalNextPageState(
      /* first_cluster_id= */ 0,
      /* last_added_time= */ kTestTimeEpoch,
      /* logging_enabled= */ true,
      /* privacy_notice_fulfilled= */ false));

  // Start tracking histograms after the initial stream load to isolate the
  // effect of load more.
  base::HistogramTester histograms;

  stream_->LoadMore(surface, base::DoNothing());
  WaitForIdleTaskQueue();

  // Process a view action that should be dropped because the upload of actions
  // is still disabled because there is still a notice card.
  stream_->ProcessViewAction(MakeFeedAction(42ul).SerializeAsString());
  WaitForIdleTaskQueue();

  // Trigger an upload.
  stream_->OnEnterBackground();
  WaitForIdleTaskQueue();

  // Verify that there were no uploads.
  EXPECT_EQ(0, network_.GetActionRequestCount());

  // Verify that the notice card fulfillment histogram isn't recorded for load
  // more.
  histograms.ExpectTotalCount("ContentSuggestions.Feed.NoticeCardFulfilled2",
                              0);
}

TEST_F(FeedStreamTest, BackgroundingAppUploadsActions) {
  stream_->UploadAction(MakeFeedAction(1ul), false, base::DoNothing());
  stream_->OnEnterBackground();
  WaitForIdleTaskQueue();
  EXPECT_EQ(1, network_.GetActionRequestSent()->feed_actions_size());
  EXPECT_EQ(1ul,
            network_.GetActionRequestSent()->feed_actions(0).content_id().id());
}

TEST_F(FeedStreamTest, BackgroundingAppDoesNotUploadActions) {
  Config config;
  config.upload_actions_on_enter_background = false;
  SetFeedConfigForTesting(config);

  stream_->UploadAction(MakeFeedAction(1ul), false, base::DoNothing());
  stream_->OnEnterBackground();
  WaitForIdleTaskQueue();
  EXPECT_EQ(0, network_.GetActionRequestCount());
}

TEST_F(FeedStreamTest, UploadedActionsAreNotSentAgain) {
  stream_->UploadAction(MakeFeedAction(1ul), false, base::DoNothing());
  stream_->OnEnterBackground();
  WaitForIdleTaskQueue();
  ASSERT_EQ(1, network_.GetActionRequestCount());

  stream_->OnEnterBackground();
  WaitForIdleTaskQueue();
  EXPECT_EQ(1, network_.GetActionRequestCount());
}

TEST_F(FeedStreamTest, UploadActionsOneBatch) {
  UploadActions(
      {MakeFeedAction(97ul), MakeFeedAction(98ul), MakeFeedAction(99ul)});
  WaitForIdleTaskQueue();

  EXPECT_EQ(1, network_.GetActionRequestCount());
  EXPECT_EQ(3, network_.GetActionRequestSent()->feed_actions_size());

  stream_->UploadAction(MakeFeedAction(99ul), true, base::DoNothing());
  WaitForIdleTaskQueue();
  EXPECT_EQ(2, network_.GetActionRequestCount());
  EXPECT_EQ(1, network_.GetActionRequestSent()->feed_actions_size());
}

TEST_F(FeedStreamTest, UploadActionsMultipleBatches) {
  UploadActions({
      // Batch 1: One really big action.
      MakeFeedAction(100ul, /*pad_size=*/20001ul),

      // Batch 2
      MakeFeedAction(101ul, 10000ul),
      MakeFeedAction(102ul, 9000ul),

      // Batch 3. Trigger upload.
      MakeFeedAction(103ul, 2000ul),
  });
  WaitForIdleTaskQueue();

  EXPECT_EQ(3, network_.GetActionRequestCount());

  stream_->UploadAction(MakeFeedAction(99ul), true, base::DoNothing());
  WaitForIdleTaskQueue();
  EXPECT_EQ(4, network_.GetActionRequestCount());
  EXPECT_EQ(1, network_.GetActionRequestSent()->feed_actions_size());
}

TEST_F(FeedStreamTest, UploadActionsSkipsStaleActionsByTimestamp) {
  stream_->UploadAction(MakeFeedAction(2ul), false, base::DoNothing());
  WaitForIdleTaskQueue();
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(25));

  // Trigger upload
  CallbackReceiver<UploadActionsTask::Result> cr;
  stream_->UploadAction(MakeFeedAction(3ul), true, cr.Bind());
  WaitForIdleTaskQueue();

  // Just one action should have been uploaded.
  EXPECT_EQ(1, network_.GetActionRequestCount());
  EXPECT_EQ(1, network_.GetActionRequestSent()->feed_actions_size());
  EXPECT_EQ(3ul,
            network_.GetActionRequestSent()->feed_actions(0).content_id().id());

  ASSERT_TRUE(cr.GetResult());
  EXPECT_EQ(1ul, cr.GetResult()->upload_attempt_count);
  EXPECT_EQ(1ul, cr.GetResult()->stale_count);
}

TEST_F(FeedStreamTest, UploadActionsErasesStaleActionsByAttempts) {
  // Three failed uploads, plus one more to cause the first action to be erased.
  network_.InjectEmptyActionRequestResult();
  stream_->UploadAction(MakeFeedAction(0ul), true, base::DoNothing());
  network_.InjectEmptyActionRequestResult();
  stream_->UploadAction(MakeFeedAction(1ul), true, base::DoNothing());
  network_.InjectEmptyActionRequestResult();
  stream_->UploadAction(MakeFeedAction(2ul), true, base::DoNothing());

  CallbackReceiver<UploadActionsTask::Result> cr;
  stream_->UploadAction(MakeFeedAction(3ul), true, cr.Bind());
  WaitForIdleTaskQueue();

  // Four requests, three pending actions in the last request.
  EXPECT_EQ(4, network_.GetActionRequestCount());
  EXPECT_EQ(3, network_.GetActionRequestSent()->feed_actions_size());

  // Action 0 should have been erased.
  ASSERT_TRUE(cr.GetResult());
  EXPECT_EQ(3ul, cr.GetResult()->upload_attempt_count);
  EXPECT_EQ(1ul, cr.GetResult()->stale_count);
}

TEST_F(FeedStreamTest, MetadataLoadedWhenDatabaseInitialized) {
  ASSERT_TRUE(stream_->GetMetadata());
  // Verify the schema has been updated to the current version.
  EXPECT_EQ((int)FeedStore::kCurrentStreamSchemaVersion,
            stream_->GetMetadata()
                ->GetMetadataProtoForTesting()
                .stream_schema_version());

  const auto kExpiry = kTestTimeEpoch + base::TimeDelta::FromDays(1234);
  stream_->GetMetadata()->SetSessionId("session-id", kExpiry);
  stream_->GetMetadata()->SetConsistencyToken("token");
  EXPECT_EQ(1, stream_->GetMetadata()->GetNextActionId().GetUnsafeValue());

  // Creating a stream should load metadata.
  CreateStream();

  ASSERT_TRUE(stream_->GetMetadata());
  EXPECT_EQ("session-id", stream_->GetMetadata()->GetSessionIdToken());
  EXPECT_TIME_EQ(kExpiry, stream_->GetMetadata()->GetSessionIdExpiryTime());
  EXPECT_EQ("token", stream_->GetMetadata()->GetConsistencyToken());
  EXPECT_EQ(2, stream_->GetMetadata()->GetNextActionId().GetUnsafeValue());
}

TEST_F(FeedStreamTest, ModelUnloadsAfterTimeout) {
  Config config;
  config.model_unload_timeout = base::TimeDelta::FromSeconds(1);
  SetFeedConfigForTesting(config);

  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  surface.Detach();

  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(999));
  WaitForIdleTaskQueue();
  EXPECT_TRUE(stream_->GetModel(surface.GetStreamType()));

  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(2));
  WaitForIdleTaskQueue();
  EXPECT_FALSE(stream_->GetModel(surface.GetStreamType()));
}

TEST_F(FeedStreamTest, ModelDoesNotUnloadIfSurfaceIsAttached) {
  Config config;
  config.model_unload_timeout = base::TimeDelta::FromSeconds(1);
  SetFeedConfigForTesting(config);

  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  surface.Detach();

  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(999));
  WaitForIdleTaskQueue();
  EXPECT_TRUE(stream_->GetModel(surface.GetStreamType()));

  surface.Attach(stream_.get());

  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(2));
  WaitForIdleTaskQueue();
  EXPECT_TRUE(stream_->GetModel(surface.GetStreamType()));
}

TEST_F(FeedStreamTest, ModelUnloadsAfterSecondTimeout) {
  Config config;
  config.model_unload_timeout = base::TimeDelta::FromSeconds(1);
  SetFeedConfigForTesting(config);

  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  surface.Detach();

  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(999));
  WaitForIdleTaskQueue();
  EXPECT_TRUE(stream_->GetModel(surface.GetStreamType()));

  // Attaching another surface will prolong the unload time for another second.
  surface.Attach(stream_.get());
  surface.Detach();

  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(999));
  WaitForIdleTaskQueue();
  EXPECT_TRUE(stream_->GetModel(surface.GetStreamType()));

  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(2));
  WaitForIdleTaskQueue();
  EXPECT_FALSE(stream_->GetModel(surface.GetStreamType()));
}

TEST_F(FeedStreamTest, ProvidesPrefetchSuggestionsWhenModelLoaded) {
  // Setup by triggering a model load.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Because we loaded from the network,
  // PrefetchService::NewSuggestionsAvailable() should have been called.
  EXPECT_EQ(1, prefetch_service_.NewSuggestionsAvailableCallCount());

  CallbackReceiver<std::vector<offline_pages::PrefetchSuggestion>> callback;
  prefetch_service_.suggestions_provider()->GetCurrentArticleSuggestions(
      callback.Bind());
  WaitForIdleTaskQueue();

  ASSERT_TRUE(callback.GetResult());
  const std::vector<offline_pages::PrefetchSuggestion>& suggestions =
      callback.GetResult().value();

  ASSERT_EQ(2UL, suggestions.size());
  EXPECT_EQ("http://content0/", suggestions[0].article_url);
  EXPECT_EQ("title0", suggestions[0].article_title);
  EXPECT_EQ("publisher0", suggestions[0].article_attribution);
  EXPECT_EQ("snippet0", suggestions[0].article_snippet);
  EXPECT_EQ("http://image0/", suggestions[0].thumbnail_url);
  EXPECT_EQ("http://favicon0/", suggestions[0].favicon_url);

  EXPECT_EQ("http://content1/", suggestions[1].article_url);
}

TEST_F(FeedStreamTest, ProvidesPrefetchSuggestionsWhenModelNotLoaded) {
  store_->OverwriteStream(kInterestStream, MakeTypicalInitialModelState(),
                          base::DoNothing());

  CallbackReceiver<std::vector<offline_pages::PrefetchSuggestion>> callback;
  prefetch_service_.suggestions_provider()->GetCurrentArticleSuggestions(
      callback.Bind());
  WaitForIdleTaskQueue();

  ASSERT_FALSE(stream_->GetModel(kInterestStream));
  ASSERT_TRUE(callback.GetResult());
  const std::vector<offline_pages::PrefetchSuggestion>& suggestions =
      callback.GetResult().value();

  ASSERT_EQ(2UL, suggestions.size());
  EXPECT_EQ("http://content0/", suggestions[0].article_url);
  EXPECT_EQ("http://content1/", suggestions[1].article_url);
  EXPECT_EQ(0, prefetch_service_.NewSuggestionsAvailableCallCount());
}

TEST_F(FeedStreamTest, ScrubsUrlsInProvidedPrefetchSuggestions) {
  {
    auto initial_state = MakeTypicalInitialModelState();
    initial_state->content[0].mutable_prefetch_metadata(0)->set_uri(
        "?notavalidurl?");
    initial_state->content[0].mutable_prefetch_metadata(0)->set_image_url(
        "?asdf?");
    initial_state->content[0].mutable_prefetch_metadata(0)->set_favicon_url(
        "?hi?");
    initial_state->content[0].mutable_prefetch_metadata(0)->clear_uri();
    store_->OverwriteStream(kInterestStream, std::move(initial_state),
                            base::DoNothing());
  }

  CallbackReceiver<std::vector<offline_pages::PrefetchSuggestion>> callback;
  prefetch_service_.suggestions_provider()->GetCurrentArticleSuggestions(
      callback.Bind());
  WaitForIdleTaskQueue();

  ASSERT_TRUE(callback.GetResult());
  const std::vector<offline_pages::PrefetchSuggestion>& suggestions =
      callback.GetResult().value();

  ASSERT_EQ(2UL, suggestions.size());
  EXPECT_EQ("", suggestions[0].article_url.possibly_invalid_spec());
  EXPECT_EQ("", suggestions[0].thumbnail_url.possibly_invalid_spec());
  EXPECT_EQ("", suggestions[0].favicon_url.possibly_invalid_spec());
}

TEST_F(FeedStreamTest, OfflineBadgesArePopulatedInitially) {
  // Add two offline pages. We exclude tab-bound pages, so only the first is
  // used.
  offline_page_model_.AddTestPage(GURL("http://content0/"));
  offline_page_model_.AddTestPage(GURL("http://content1/"));
  offline_page_model_.items()[1].client_id.name_space =
      offline_pages::kLastNNamespace;
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ((std::map<std::string, std::string>(
                {{"app/badge0", SerializedOfflineBadgeContent()}})),
            surface.GetDataStoreEntries());
}

TEST_F(FeedStreamTest, OfflineBadgesArePopulatedOnNewOfflineItemAdded) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_EQ((std::map<std::string, std::string>({})),
            surface.GetDataStoreEntries());

  // Add an offline page.
  offline_page_model_.AddTestPage(GURL("http://content1/"));
  offline_page_model_.CallObserverOfflinePageAdded(
      offline_page_model_.items()[0]);
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1));

  EXPECT_EQ((std::map<std::string, std::string>(
                {{"app/badge1", SerializedOfflineBadgeContent()}})),
            surface.GetDataStoreEntries());
}

TEST_F(FeedStreamTest, OfflineBadgesAreRemovedWhenOfflineItemRemoved) {
  offline_page_model_.AddTestPage(GURL("http://content0/"));
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_EQ((std::map<std::string, std::string>(
                {{"app/badge0", SerializedOfflineBadgeContent()}})),
            surface.GetDataStoreEntries());

  // Remove the offline page.
  offline_page_model_.CallObserverOfflinePageDeleted(
      offline_page_model_.items()[0]);
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1));

  EXPECT_EQ((std::map<std::string, std::string>()),
            surface.GetDataStoreEntries());
}

TEST_F(FeedStreamTest, OfflineBadgesAreProvidedToNewSurfaces) {
  offline_page_model_.AddTestPage(GURL("http://content0/"));
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  TestForYouSurface surface2(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ((std::map<std::string, std::string>(
                {{"app/badge0", SerializedOfflineBadgeContent()}})),
            surface2.GetDataStoreEntries());
}

TEST_F(FeedStreamTest, OfflineBadgesAreRemovedWhenModelIsUnloaded) {
  offline_page_model_.AddTestPage(GURL("http://content0/"));
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  stream_->UnloadModel(surface.GetStreamType());

  // Offline badge no longer present.
  EXPECT_EQ((std::map<std::string, std::string>()),
            surface.GetDataStoreEntries());
}

TEST_F(FeedStreamTest, MultipleOfflineBadgesWithSameUrl) {
  {
    std::unique_ptr<StreamModelUpdateRequest> state =
        MakeTypicalInitialModelState();
    const feedwire::PrefetchMetadata& prefetch_metadata1 =
        state->content[0].prefetch_metadata(0);
    feedwire::PrefetchMetadata& prefetch_metadata2 =
        *state->content[0].add_prefetch_metadata();
    prefetch_metadata2 = prefetch_metadata1;
    prefetch_metadata2.set_badge_id("app/badge0b");
    response_translator_.InjectResponse(std::move(state));
  }
  offline_page_model_.AddTestPage(GURL("http://content0/"));

  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ((std::map<std::string, std::string>(
                {{"app/badge0", SerializedOfflineBadgeContent()},
                 {"app/badge0b", SerializedOfflineBadgeContent()}})),
            surface.GetDataStoreEntries());
}

TEST_F(FeedStreamTest, SendsClientInstanceId) {
  stream_->GetMetadata()->SetConsistencyToken("token");

  // Store is empty, so we should fallback to a network request.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_EQ(1, network_.send_query_call_count);
  ASSERT_TRUE(network_.query_request_sent);

  // Instance ID is a random token. Verify it is not empty.
  std::string first_instance_id = network_.query_request_sent->feed_request()
                                      .client_info()
                                      .client_instance_id();
  EXPECT_NE("", first_instance_id);

  // No signed-out session id was in the request.
  EXPECT_TRUE(network_.query_request_sent->feed_request()
                  .client_info()
                  .chrome_client_info()
                  .session_id()
                  .empty());

  // LoadMore, and verify the same token is used.
  response_translator_.InjectResponse(MakeTypicalNextPageState(2));
  stream_->LoadMore(surface, base::DoNothing());
  WaitForIdleTaskQueue();

  ASSERT_EQ(2, network_.send_query_call_count);
  EXPECT_EQ(first_instance_id, network_.query_request_sent->feed_request()
                                   .client_info()
                                   .client_instance_id());

  // No signed-out session id was in the request.
  EXPECT_TRUE(network_.query_request_sent->feed_request()
                  .client_info()
                  .chrome_client_info()
                  .session_id()
                  .empty());

  // Trigger a ClearAll to verify the instance ID changes.
  stream_->OnSignedOut();
  WaitForIdleTaskQueue();

  EXPECT_FALSE(stream_->GetModel(surface.GetStreamType()));
  const bool is_for_next_page = false;  // No model so no first page yet.
  const std::string new_instance_id =
      stream_->GetRequestMetadata(kInterestStream, is_for_next_page)
          .client_instance_id;
  ASSERT_NE("", new_instance_id);
  ASSERT_NE(first_instance_id, new_instance_id);
}

TEST_F(FeedStreamTest, LoadStreamSendsNoticeCardAcknowledgement) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      feed::kInterestFeedNoticeCardAutoDismiss);

  store_->OverwriteStream(kInterestStream, MakeTypicalInitialModelState(),
                          base::DoNothing());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Generate 3 view actions and 1 click action to trigger the acknowledgement
  // of the notice card.
  const int notice_card_index = 0;
  std::string slice_id =
      surface.initial_state->updated_slices(notice_card_index)
          .slice()
          .slice_id();
  stream_->ReportSliceViewed(surface.GetSurfaceId(), surface.GetStreamType(),
                             slice_id);
  stream_->ReportSliceViewed(surface.GetSurfaceId(), surface.GetStreamType(),
                             slice_id);
  stream_->ReportSliceViewed(surface.GetSurfaceId(), surface.GetStreamType(),
                             slice_id);
  stream_->ReportOpenAction(surface.GetStreamType(), slice_id);

  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  refresh_scheduler_.Clear();
  stream_->UnloadModel(surface.GetStreamType());
  stream_->ExecuteRefreshTask();
  WaitForIdleTaskQueue();

  EXPECT_TRUE(network_.query_request_sent->feed_request()
                  .feed_query()
                  .chrome_fulfillment_info()
                  .notice_card_acknowledged());
}

TEST_F(FeedStreamTest, GetSetAndUpdateSessionId) {
  const std::string kToken1 = "token1";
  const std::string kToken2 = "token2";
  const base::Time kExpiryTime1 =
      kTestTimeEpoch + base::TimeDelta::FromHours(2);
  const base::Time kExpiryTime2 =
      kTestTimeEpoch + GetFeedConfig().session_id_max_age;
  ASSERT_NE(kExpiryTime1, kExpiryTime2);

  // The stream metadata is initialized with an empty token and expiry time.
  EXPECT_TRUE(stream_->GetMetadata()->GetSessionIdToken().empty());
  EXPECT_TRUE(stream_->GetMetadata()->GetSessionIdExpiryTime().is_null());

  // Verify that directly calling SetSessionId works as expected.
  stream_->GetMetadata()->SetSessionId(kToken1, kExpiryTime1);
  EXPECT_EQ(kToken1, stream_->GetMetadata()->GetSessionIdToken());
  EXPECT_TIME_EQ(kExpiryTime1,
                 stream_->GetMetadata()->GetSessionIdExpiryTime());

  // Updating the token with nullopt is a NOP.
  stream_->GetMetadata()->MaybeUpdateSessionId(base::nullopt);
  EXPECT_EQ(kToken1, stream_->GetMetadata()->GetSessionIdToken());
  EXPECT_TIME_EQ(kExpiryTime1,
                 stream_->GetMetadata()->GetSessionIdExpiryTime());

  // Updating the token with the same value is a NOP.
  stream_->GetMetadata()->MaybeUpdateSessionId(kToken1);
  EXPECT_EQ(kToken1, stream_->GetMetadata()->GetSessionIdToken());
  EXPECT_TIME_EQ(kExpiryTime1,
                 stream_->GetMetadata()->GetSessionIdExpiryTime());

  // Updating the token with a different value resets the token and assigns a
  // new expiry time.
  stream_->GetMetadata()->MaybeUpdateSessionId(kToken2);
  EXPECT_EQ(kToken2, stream_->GetMetadata()->GetSessionIdToken());
  EXPECT_TIME_EQ(kExpiryTime2,
                 stream_->GetMetadata()->GetSessionIdExpiryTime());

  // Updating the token with the empty string clears its value.
  stream_->GetMetadata()->MaybeUpdateSessionId("");
  EXPECT_TRUE(stream_->GetMetadata()->GetSessionIdToken().empty());
  EXPECT_TRUE(stream_->GetMetadata()->GetSessionIdExpiryTime().is_null());
}

TEST_F(FeedStreamTest, SignedOutSessionIdConsistency) {
  const std::string kSessionToken1("session-token-1");
  const std::string kSessionToken2("session-token-2");

  is_signed_in_ = false;

  StreamModelUpdateRequestGenerator model_generator;
  model_generator.signed_in = false;

  // (1) Do an initial load of the store
  //     - this should trigger a network request
  //     - the request should not include client-instance-id
  //     - the request should not include a session-id
  //     - the stream should capture the session-id token from the response
  response_translator_.InjectResponse(model_generator.MakeFirstPage(),
                                      kSessionToken1);
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ(1, network_.send_query_call_count);
  EXPECT_TRUE(network_.query_request_sent->feed_request()
                  .client_info()
                  .client_instance_id()
                  .empty());
  EXPECT_FALSE(network_.query_request_sent->feed_request()
                   .client_info()
                   .has_chrome_client_info());
  EXPECT_EQ(kSessionToken1, stream_->GetMetadata()->GetSessionIdToken());
  const base::Time kSessionToken1ExpiryTime =
      stream_->GetMetadata()->GetSessionIdExpiryTime();

  // (2) LoadMore: the server returns the same session-id token
  //     - this should trigger a network request
  //     - the request should not include client-instance-id
  //     - the request should include the first session-id
  //     - the stream should retain the first session-id
  //     - the session-id's expiry time should be unchanged
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  response_translator_.InjectResponse(model_generator.MakeNextPage(2),
                                      kSessionToken1);
  stream_->LoadMore(surface, base::DoNothing());
  WaitForIdleTaskQueue();
  ASSERT_EQ(2, network_.send_query_call_count);
  EXPECT_TRUE(network_.query_request_sent->feed_request()
                  .client_info()
                  .client_instance_id()
                  .empty());
  EXPECT_EQ(kSessionToken1, network_.query_request_sent->feed_request()
                                .client_info()
                                .chrome_client_info()
                                .session_id());
  EXPECT_EQ(kSessionToken1, stream_->GetMetadata()->GetSessionIdToken());
  EXPECT_TIME_EQ(kSessionToken1ExpiryTime,
                 stream_->GetMetadata()->GetSessionIdExpiryTime());

  // (3) LoadMore: the server omits returning a session-id token
  //     - this should trigger a network request
  //     - the request should not include client-instance-id
  //     - the request should include the first session-id
  //     - the stream should retain the first session-id
  //     - the session-id's expiry time should be unchanged
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  response_translator_.InjectResponse(model_generator.MakeNextPage(3));
  stream_->LoadMore(surface, base::DoNothing());
  WaitForIdleTaskQueue();
  ASSERT_EQ(3, network_.send_query_call_count);
  EXPECT_TRUE(network_.query_request_sent->feed_request()
                  .client_info()
                  .client_instance_id()
                  .empty());
  EXPECT_EQ(kSessionToken1, network_.query_request_sent->feed_request()
                                .client_info()
                                .chrome_client_info()
                                .session_id());
  EXPECT_EQ(kSessionToken1, stream_->GetMetadata()->GetSessionIdToken());
  EXPECT_TIME_EQ(kSessionToken1ExpiryTime,
                 stream_->GetMetadata()->GetSessionIdExpiryTime());

  // (4) LoadMore: the server returns new session id.
  //     - this should trigger a network request
  //     - the request should not include client-instance-id
  //     - the request should include the first session-id
  //     - the stream should retain the second session-id
  //     - the new session-id's expiry time should be updated
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  response_translator_.InjectResponse(model_generator.MakeNextPage(4),
                                      kSessionToken2);
  stream_->LoadMore(surface, base::DoNothing());
  WaitForIdleTaskQueue();
  ASSERT_EQ(4, network_.send_query_call_count);
  EXPECT_TRUE(network_.query_request_sent->feed_request()
                  .client_info()
                  .client_instance_id()
                  .empty());
  EXPECT_EQ(kSessionToken1, network_.query_request_sent->feed_request()
                                .client_info()
                                .chrome_client_info()
                                .session_id());
  EXPECT_EQ(kSessionToken2, stream_->GetMetadata()->GetSessionIdToken());
  EXPECT_TIME_EQ(kSessionToken1ExpiryTime + base::TimeDelta::FromSeconds(3),
                 stream_->GetMetadata()->GetSessionIdExpiryTime());
}

TEST_F(FeedStreamTest, ClearAllResetsSessionId) {
  is_signed_in_ = false;

  // Initialize a session id.
  stream_->GetMetadata()->MaybeUpdateSessionId("session-id");
  ASSERT_FALSE(stream_->GetMetadata()->GetSessionIdToken().empty());
  ASSERT_FALSE(stream_->GetMetadata()->GetSessionIdExpiryTime().is_null());

  // Trigger a ClearAll.
  stream_->OnCacheDataCleared();
  WaitForIdleTaskQueue();

  // Session-ID should be wiped.
  EXPECT_TRUE(stream_->GetMetadata()->GetSessionIdToken().empty());
  EXPECT_TRUE(stream_->GetMetadata()->GetSessionIdExpiryTime().is_null());
}

TEST_F(FeedStreamTest, SignedOutSessionIdExpiry) {
  const std::string kSessionToken1("session-token-1");
  const std::string kSessionToken2("session-token-2");

  is_signed_in_ = false;

  StreamModelUpdateRequestGenerator model_generator;
  model_generator.signed_in = false;

  // (1) Do an initial load of the store
  //     - this should trigger a network request
  //     - the request should not include a session-id
  //     - the stream should capture the session-id token from the response
  response_translator_.InjectResponse(model_generator.MakeFirstPage(),
                                      kSessionToken1);
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ(1, network_.send_query_call_count);
  EXPECT_FALSE(network_.query_request_sent->feed_request()
                   .client_info()
                   .has_chrome_client_info());
  EXPECT_EQ(kSessionToken1, stream_->GetMetadata()->GetSessionIdToken());

  // (2) Reload the stream from the network:
  //     - Detach the surface, advance the clock beyond the stale content
  //       threshold, re-attach the surface.
  //     - this should trigger a network request
  //     - the request should include kSessionToken1
  //     - the stream should retain the original session-id
  surface.Detach();
  task_environment_.FastForwardBy(GetFeedConfig().stale_content_threshold +
                                  base::TimeDelta::FromSeconds(1));
  response_translator_.InjectResponse(model_generator.MakeFirstPage());
  surface.Attach(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ(2, network_.send_query_call_count);
  EXPECT_EQ(kSessionToken1, network_.query_request_sent->feed_request()
                                .client_info()
                                .chrome_client_info()
                                .session_id());
  EXPECT_EQ(kSessionToken1, stream_->GetMetadata()->GetSessionIdToken());

  // (3) Reload the stream from the network:
  //     - Detach the surface, advance the clock beyond the session id max age
  //       threshold, re-attach the surface.
  //     - this should trigger a network request
  //     - the request should not include a session-id
  //     - the stream should get a new session-id
  surface.Detach();
  task_environment_.FastForwardBy(GetFeedConfig().session_id_max_age -
                                  GetFeedConfig().stale_content_threshold);
  ASSERT_LT(stream_->GetMetadata()->GetSessionIdExpiryTime(),
            base::Time::Now());
  response_translator_.InjectResponse(model_generator.MakeFirstPage(),
                                      kSessionToken2);
  surface.Attach(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ(3, network_.send_query_call_count);
  EXPECT_FALSE(network_.query_request_sent->feed_request()
                   .client_info()
                   .has_chrome_client_info());
  EXPECT_EQ(kSessionToken2, stream_->GetMetadata()->GetSessionIdToken());
}

TEST_F(FeedStreamTest, SessionIdPersistsAcrossStreamLoads) {
  const std::string kSessionToken("session-token-ftw");
  const base::Time kExpiryTime =
      kTestTimeEpoch + GetFeedConfig().session_id_max_age;

  StreamModelUpdateRequestGenerator model_generator;
  model_generator.signed_in = false;
  is_signed_in_ = false;

  // (1) Do an initial load of the store
  //     - this should trigger a network request
  //     - the stream should capture the session-id token from the response
  response_translator_.InjectResponse(model_generator.MakeFirstPage(),
                                      kSessionToken);
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ(1, network_.send_query_call_count);

  // (2) Reload the metadata from disk.
  //     - the network query call count should be unchanged
  //     - the session token and expiry time should have been reloaded.
  surface.Detach();
  CreateStream();
  WaitForIdleTaskQueue();
  ASSERT_EQ(1, network_.send_query_call_count);
  EXPECT_EQ(kSessionToken, stream_->GetMetadata()->GetSessionIdToken());
  EXPECT_TIME_EQ(kExpiryTime, stream_->GetMetadata()->GetSessionIdExpiryTime());
}

TEST_F(FeedStreamTest, PersistentKeyValueStoreIsClearedOnClearAll) {
  // Store some data and verify it exists.
  PersistentKeyValueStore* store = stream_->GetPersistentKeyValueStore();
  store->Put("x", "y", base::DoNothing());
  CallbackReceiver<PersistentKeyValueStore::Result> get_result;
  store->Get("x", get_result.Bind());
  ASSERT_EQ("y", *get_result.RunAndGetResult().get_result);

  stream_->OnCacheDataCleared();  // triggers ClearAll().
  WaitForIdleTaskQueue();

  // Verify ClearAll() deleted the data.
  get_result.Clear();
  store->Get("x", get_result.Bind());
  EXPECT_FALSE(get_result.RunAndGetResult().get_result);
}

TEST_F(FeedStreamTest, LoadMultipleStreams) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface for_you_surface(stream_.get());
  TestWebFeedSurface web_feed_surface(stream_.get());

  WaitForIdleTaskQueue();

  ASSERT_EQ("loading -> 2 slices", for_you_surface.DescribeUpdates());
  ASSERT_EQ("loading -> 2 slices", web_feed_surface.DescribeUpdates());
}

TEST_F(FeedStreamTest, UnloadOnlyOneOfMultipleModels) {
  Config config;
  config.model_unload_timeout = base::TimeDelta::FromSeconds(1);
  SetFeedConfigForTesting(config);

  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface for_you_surface(stream_.get());
  TestWebFeedSurface web_feed_surface(stream_.get());

  WaitForIdleTaskQueue();

  for_you_surface.Detach();

  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(2));
  WaitForIdleTaskQueue();

  EXPECT_TRUE(stream_->GetModel(kWebFeedStream));
  EXPECT_FALSE(stream_->GetModel(kInterestStream));
}

TEST_F(FeedStreamTest, ExperimentsAreClearedOnClearAll) {
  Experiments e;
  e["Trial1"] = "Group1";
  e["Trial2"] = "Group2";
  prefs::SetExperiments(e, profile_prefs_);

  stream_->OnCacheDataCleared();  // triggers ClearAll().
  WaitForIdleTaskQueue();

  Experiments empty;
  Experiments got = prefs::GetExperiments(profile_prefs_);
  EXPECT_EQ(got, empty);
}

// Keep instantiations at the bottom.
INSTANTIATE_TEST_SUITE_P(FeedStreamTest,
                         FeedStreamTestForAllStreamTypes,
                         ::testing::Values(kInterestStream, kWebFeedStream),
                         ::testing::PrintToStringParamName());

}  // namespace
}  // namespace feed
