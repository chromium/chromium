// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PUBLIC_FEED_API_H_
#define COMPONENTS_FEED_CORE_V2_PUBLIC_FEED_API_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "components/feed/core/v2/public/common_enums.h"
#include "components/feed/core/v2/public/refresh_task_scheduler.h"
#include "components/feed/core/v2/public/stream_type.h"
#include "components/feed/core/v2/public/types.h"
#include "components/feed/core/v2/public/unread_content_observer.h"
#include "components/feed/core/v2/public/web_feed_subscriptions.h"
#include "url/gurl.h"

namespace feedui {
class StreamUpdate;
}  // namespace feedui
namespace feedstore {
class DataOperation;
}  // namespace feedstore

namespace feed {
class PersistentKeyValueStore;
class WebFeedSubscriptions;
struct LoggingParameters;
class SurfaceRenderer;

// This is the public access point for interacting with the Feed contents.
// FeedApi serves multiple streams of data, one for each StreamType.
class FeedApi {
 public:
  FeedApi();
  virtual ~FeedApi();
  FeedApi(const FeedApi&) = delete;
  FeedApi& operator=(const FeedApi&) = delete;

  virtual WebFeedSubscriptions& subscriptions() = 0;

  // Surfaces present feed content to users. When a surface is visible to users
  // and should be displaying feed content, it is attached to request to feed
  // content and render it. A surface may be attached and detached several times
  // through its lifetime. When a surface is no longer needed, it should be
  // destroyed with DestroySurface.
  virtual SurfaceId CreateSurface(const StreamType& type,
                                  SingleWebFeedEntryPoint entry_point) = 0;
  virtual void DestroySurface(SurfaceId surface) = 0;

  // Attach/detach a surface for rendering. Surfaces should be attached when
  // content is required for display, and detached when content is no longer
  // shown.
  virtual void AttachSurface(SurfaceId surface_id,
                             SurfaceRenderer* renderer) = 0;
  virtual void DetachSurface(SurfaceId surface_id) = 0;

  // Notifies |this| that the user clicked on a feed card with its |url| and
  // |entity_mids| entities.
  virtual void UpdateUserProfileOnLinkClick(
      const GURL& url,
      const std::vector<int64_t>& entity_mids) = 0;

  // Begin/stop observing a stream type. An observer instance should not be
  // added twice without first being removed.
  virtual void AddUnreadContentObserver(const StreamType& stream_type,
                                        UnreadContentObserver* observer) = 0;
  virtual void RemoveUnreadContentObserver(const StreamType& stream_type,
                                           UnreadContentObserver* observer) = 0;

  virtual bool IsArticlesListVisible() = 0;

  // Returns the client's signed-out session id. This value is reset whenever
  // the feed stream is cleared (on sign-in, sign-out, and some data clear
  // events).
  virtual std::string GetSessionId() const = 0;

  // Sets the requested content order of the feed, and triggers a refresh if
  // necessary. Note that currently, only Web Feed can change the content order.
  virtual void SetContentOrder(const StreamType& stream_type,
                               ContentOrder content_order) = 0;

  // Returns the current `ContentOrder` for `stream_type`.
  virtual ContentOrder GetContentOrder(const StreamType& stream_type) const = 0;

  // Gets the "raw" content order value stored in prefs. Returns `kUnspecified`
  // if the user has not selected one yet.
  virtual ContentOrder GetContentOrderFromPrefs(
      const StreamType& stream_type) = 0;

  // Invoked by RefreshTaskScheduler's scheduled task.
  virtual void ExecuteRefreshTask(RefreshTaskId task_id) = 0;

  // Request to load additional content at the end of the stream.
  // Calls |callback| when complete. If no content could be added, the parameter
  // is false, and the caller should expect |LoadMore| to fail if called
  // further.
  virtual void LoadMore(SurfaceId surface_id,
                        base::OnceCallback<void(bool)> callback) = 0;

  // Refresh the feed content by fetching the fresh content from the server.
  // Calls |callback| when complete. If the fetch fails, the parameter is false.
  virtual void ManualRefresh(SurfaceId surface_id,
                             base::OnceCallback<void(bool)> callback) = 0;

  // Request to fetch a URL resource. Calls |callback| with the network response
  // when complete.
  virtual void FetchResource(
      const GURL& url,
      const std::string& method,
      const std::vector<std::string>& header_names_and_values,
      const std::string& post_data,
      base::OnceCallback<void(NetworkResponse)> callback) = 0;

  // Request to fetch and image for use in the feed. Calls |callback|
  // with the network response when complete. The returned ImageFetchId can be
  // passed to CancelImageFetch() to cancel the request.
  virtual ImageFetchId FetchImage(
      const GURL& url,
      base::OnceCallback<void(NetworkResponse)> callback) = 0;
  // If |id| matches an active fetch, cancels the fetch and runs the callback
  // with an empty response body and status_code=net::Error::ERR_ABORTED. If
  // |id| doesn't match an active fetch, nothing happens.
  virtual void CancelImageFetch(ImageFetchId id) = 0;

  virtual PersistentKeyValueStore& GetPersistentKeyValueStore() = 0;

  // Apply |operations| to the stream model. Does nothing if the model is not
  // yet loaded.
  virtual void ExecuteOperations(
      SurfaceId surface_id,
      std::vector<feedstore::DataOperation> operations) = 0;

  // Create a temporary change that may be undone or committed later. Does
  // nothing if the model is not yet loaded.
  virtual EphemeralChangeId CreateEphemeralChange(
      SurfaceId surface_id,
      std::vector<feedstore::DataOperation> operations) = 0;
  // Same as |CreateEphemeralChange()|, but data is a serialized
  // |feedpacking::DismissData| message.
  virtual EphemeralChangeId CreateEphemeralChangeFromPackedData(
      SurfaceId surface_id,
      std::string_view data) = 0;
  // Commits a change. Returns false if the change does not exist.
  virtual bool CommitEphemeralChange(SurfaceId surface_id,
                                     EphemeralChangeId id) = 0;
  // Rejects a change. Returns false if the change does not exist.
  virtual bool RejectEphemeralChange(SurfaceId surface_id,
                                     EphemeralChangeId id) = 0;

  // Sends 'ThereAndBackAgainData' back to the server. |data| is a serialized
  // |feedwire::ThereAndBackAgainData| message.
  virtual void ProcessThereAndBackAgain(
      std::string_view data,
      const LoggingParameters& logging_parameters) = 0;
  // Saves a view action for eventual upload. |data| is a serialized
  //|feedwire::FeedAction| message. `logging_parameters` are the logging
  // parameters associated with this item, see `feedui::StreamUpdate`.
  virtual void ProcessViewAction(
      std::string_view data,
      const LoggingParameters& logging_parameters) = 0;

  // Returns whether `url` is a suggested Feed URLs, recently
  // navigated to by the user.
  virtual bool WasUrlRecentlyNavigatedFromFeed(const GURL& url) = 0;

  // Requests that the cache of the feed identified by |stream_kind| be
  // invalidated so that its contents are re-fetched the next time that feed is
  // shown/loaded.
  virtual void InvalidateContentCacheFor(StreamKind stream_kind) = 0;

  // Called when content is viewed, providing a unique identifier of the content
  // which was viewed. Used for signed-out view demotion. This may be called
  // regardless of sign-in status, but may be ignored.
  virtual void RecordContentViewed(SurfaceId surface_id, uint64_t docid) = 0;

  // User interaction reporting. Unless otherwise documented, these have no
  // side-effects other than reporting metrics.

  // A slice was viewed (2/3rds of it is in the viewport). Should be called
  // once for each viewed slice in the stream.
  virtual void ReportSliceViewed(SurfaceId surface_id,
                                 const std::string& slice_id) = 0;
  // Some feed content has been loaded and is now available to the user on the
  // feed surface. Reported only once after a surface is attached.
  virtual void ReportFeedViewed(SurfaceId surface_id) = 0;
  // A web page was loaded in response to opening a link from the Feed.
  virtual void ReportPageLoaded(SurfaceId surface_id) = 0;
  // The user triggered the open action which can be caused by tapping the card,
  // or selecting 'Open in new tab' menu item. The open action to trigger is
  // providen in `action_type`.
  // Remembers the URL for later calls to `WasUrlRecentlyNavigatedFromFeed()`.
  virtual void ReportOpenAction(const GURL& url,
                                SurfaceId surface_id,
                                const std::string& slice_id,
                                OpenActionType action_type) = 0;
  // The user triggered an open action, visited a web page, and then navigated
  // away or backgrouded the tab. |visit_time| is a measure of how long the
  // visited page was foregrounded.
  virtual void ReportOpenVisitComplete(SurfaceId surface_id,
                                       base::TimeDelta visit_time) = 0;
  // The user scrolled the feed by |distance_dp| and then stopped.
  virtual void ReportStreamScrolled(SurfaceId surface_id, int distance_dp) = 0;
  // The user started scrolling the feed. Typically followed by a call to
  // |ReportStreamScrolled()|.
  virtual void ReportStreamScrollStart(SurfaceId surface_id) = 0;
  // Report that some user action occurred which does not have a specific
  // reporting function above..
  virtual void ReportOtherUserAction(SurfaceId surface_id,
                                     FeedUserActionType action_type) = 0;
  // TODO(harringtond): Remove this one.
  virtual void ReportOtherUserAction(const StreamType& stream_type,
                                     FeedUserActionType action_type) = 0;
  // Reports that the info card is being tracked for its full visibility.
  virtual void ReportInfoCardTrackViewStarted(SurfaceId surface_id,
                                              int info_card_type) = 0;
  // Reports that the info card is visible in the viewport within the threshold.
  virtual void ReportInfoCardViewed(SurfaceId surface_id,
                                    int info_card_type,
                                    int minimum_view_interval_seconds) = 0;
  // Reports that the user taps the info card.
  virtual void ReportInfoCardClicked(SurfaceId surface_id,
                                     int info_card_type) = 0;
  // Reports that the user dismisses the info card explicitly by tapping the
  // close button.
  virtual void ReportInfoCardDismissedExplicitly(SurfaceId surface_id,
                                                 int info_card_type) = 0;
  // Resets all the states of the info card.
  virtual void ResetInfoCardStates(SurfaceId surface_id,
                                   int info_card_type) = 0;
  // Report a period of time for which at least one content slice is visible
  // enough or at least one content slice covers enough of the viewport. See the
  // slice_exposure_threshold and slice_coverage_threshold feature params for
  // what counts as visible enough and covering enough.
  virtual void ReportContentSliceVisibleTimeForGoodVisits(
      SurfaceId surface_id,
      base::TimeDelta elapsed) = 0;

  // The following methods are used for the internals page.

  virtual DebugStreamData GetDebugStreamData() = 0;
  // Forces a Feed refresh from the server.
  virtual void ForceRefreshForDebugging(const StreamType& stream_type) = 0;
  // Dumps some state information for debugging.
  virtual std::string DumpStateForDebugging() = 0;
  // Forces to render a StreamUpdate on all subsequent surface attaches.
  virtual void SetForcedStreamUpdateForDebugging(
      const feedui::StreamUpdate& stream_update) = 0;
  // Returns the time of the last successful content fetch.
  virtual base::Time GetLastFetchTime(SurfaceId surface_id) = 0;
  // Increase the count of the number of times the user has followed from the
  // web page menu.
  virtual void IncrementFollowedFromWebPageMenuCount() = 0;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_PUBLIC_FEED_API_H_
