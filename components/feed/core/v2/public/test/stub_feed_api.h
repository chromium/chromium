// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PUBLIC_TEST_STUB_FEED_API_H_
#define COMPONENTS_FEED_CORE_V2_PUBLIC_TEST_STUB_FEED_API_H_

#include <string_view>

#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/core/v2/public/persistent_key_value_store.h"
#include "components/feed/core/v2/public/test/stub_web_feed_subscriptions.h"

namespace feed {

class StubPersistentKeyValueStore : public PersistentKeyValueStore {
 public:
  StubPersistentKeyValueStore() = default;
  ~StubPersistentKeyValueStore() override = default;

  void ClearAll(ResultCallback callback) override {}
  void Put(const std::string& key,
           const std::string& value,
           ResultCallback callback) override {}
  void Get(const std::string& key, ResultCallback callback) override {}
  void Delete(const std::string& key, ResultCallback callback) override {}

 private:
};

class StubFeedApi : public FeedApi {
 public:
  WebFeedSubscriptions& subscriptions() override;

  SurfaceId CreateSurface(const StreamType& type,
                          SingleWebFeedEntryPoint entry_point) override;
  void DestroySurface(SurfaceId surface) override {}
  void AttachSurface(SurfaceId surface_id, SurfaceRenderer* renderer) override {
  }
  void DetachSurface(SurfaceId surface_id) override {}
  void UpdateUserProfileOnLinkClick(
      const GURL& url,
      const std::vector<int64_t>& entity_mids) override {}
  void AddUnreadContentObserver(const StreamType& stream_type,
                                UnreadContentObserver* observer) override {}
  void RemoveUnreadContentObserver(const StreamType& stream_type,
                                   UnreadContentObserver* observer) override {}
  bool IsArticlesListVisible() override;
  std::string GetSessionId() const override;
  void ExecuteRefreshTask(RefreshTaskId task_id) override {}
  void LoadMore(SurfaceId surface_id,
                base::OnceCallback<void(bool)> callback) override {}
  void ManualRefresh(SurfaceId surface_id,
                     base::OnceCallback<void(bool)> callback) override {}
  void FetchResource(
      const GURL& url,
      const std::string& method,
      const std::vector<std::string>& header_names_and_values,
      const std::string& post_data,
      base::OnceCallback<void(NetworkResponse)> callback) override {}
  ImageFetchId FetchImage(
      const GURL& url,
      base::OnceCallback<void(NetworkResponse)> callback) override;
  void CancelImageFetch(ImageFetchId id) override {}
  PersistentKeyValueStore& GetPersistentKeyValueStore() override;
  void ExecuteOperations(
      SurfaceId surface_id,
      std::vector<feedstore::DataOperation> operations) override {}
  EphemeralChangeId CreateEphemeralChange(
      SurfaceId surface_id,
      std::vector<feedstore::DataOperation> operations) override;
  EphemeralChangeId CreateEphemeralChangeFromPackedData(
      SurfaceId surface_id,
      std::string_view data) override;
  bool CommitEphemeralChange(SurfaceId surface_id,
                             EphemeralChangeId id) override;
  bool RejectEphemeralChange(SurfaceId surface_id,
                             EphemeralChangeId id) override;
  void ProcessThereAndBackAgain(
      std::string_view data,
      const LoggingParameters& logging_parameters) override {}
  void ProcessViewAction(std::string_view data,
                         const LoggingParameters& logging_parameters) override {
  }
  bool WasUrlRecentlyNavigatedFromFeed(const GURL& url) override;
  void InvalidateContentCacheFor(StreamKind stream_kind) override {}
  void RecordContentViewed(SurfaceId surface_id, uint64_t docid) override {}
  void ReportSliceViewed(SurfaceId surface_id,
                         const std::string& slice_id) override {}
  void ReportFeedViewed(SurfaceId surface_id) override {}
  void ReportPageLoaded(SurfaceId surface_id) override {}
  void ReportOpenAction(const GURL& url,
                        SurfaceId surface_id,
                        const std::string& slice_id,
                        OpenActionType action_type) override {}
  void ReportOpenVisitComplete(SurfaceId surface_id,
                               base::TimeDelta visit_time) override {}
  void ReportStreamScrolled(SurfaceId surface_id, int distance_dp) override {}
  void ReportStreamScrollStart(SurfaceId surface_id) override {}
  void ReportOtherUserAction(SurfaceId surface_id,
                             FeedUserActionType action_type) override {}
  void ReportOtherUserAction(const StreamType& stream_type,
                             FeedUserActionType action_type) override {}
  void ReportInfoCardTrackViewStarted(SurfaceId surface_id,
                                      int info_card_type) override {}
  void ReportInfoCardViewed(SurfaceId surface_id,
                            int info_card_type,
                            int minimum_view_interval_seconds) override {}
  void ReportInfoCardClicked(SurfaceId surface_id,
                             int info_card_type) override {}
  void ReportInfoCardDismissedExplicitly(SurfaceId surface_id,
                                         int info_card_type) override {}
  void ResetInfoCardStates(SurfaceId surface_id, int info_card_type) override {}
  void ReportContentSliceVisibleTimeForGoodVisits(
      SurfaceId surface_id,
      base::TimeDelta elapsed) override {}
  DebugStreamData GetDebugStreamData() override;
  void ForceRefreshForDebugging(const StreamType& stream_type) override {}
  std::string DumpStateForDebugging() override;
  void SetForcedStreamUpdateForDebugging(
      const feedui::StreamUpdate& stream_update) override {}
  base::Time GetLastFetchTime(SurfaceId surface_id) override;
  void SetContentOrder(const StreamType& stream_type,
                       ContentOrder content_order) override {}
  ContentOrder GetContentOrder(const StreamType& stream_type) const override;
  ContentOrder GetContentOrderFromPrefs(const StreamType& stream_type) override;
  void IncrementFollowedFromWebPageMenuCount() override {}

 private:
  StubWebFeedSubscriptions web_feed_subscriptions_;
  StubPersistentKeyValueStore persistent_key_value_store_;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_PUBLIC_TEST_STUB_FEED_API_H_
