// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PUBLIC_FEED_STREAM_API_H_
#define COMPONENTS_FEED_CORE_V2_PUBLIC_FEED_STREAM_API_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/observer_list_types.h"
#include "base/strings/string_piece_forward.h"
#include "base/time/time.h"
#include "components/feed/core/v2/common_enums.h"
#include "components/feed/core/v2/public/types.h"
#include "url/gurl.h"

namespace feedui {
class StreamUpdate;
}  // namespace feedui
namespace feedstore {
class DataOperation;
}  // namespace feedstore

namespace feed {
class PersistentKeyValueStore;

// Selects the stream type.
// TODO(crbug.com/1152592): Need to use StreamType in several places:
// - Stream loading/saving
// - Metrics
// Note: currently there are two options, but this leaves room for more
// parameters.
class StreamType {
 public:
  enum class Type {
    // An unspecified stream type. Used only to represent an uninitialized
    // stream type value.
    kUnspecified,
    // The For-You feed stream.
    kInterest,
    // The Web Feed stream.
    kWebFeed,
  };
  constexpr StreamType() = default;
  constexpr explicit StreamType(Type t) : type_(t) {}
  bool operator<(const StreamType& rhs) const { return type_ < rhs.type_; }
  bool operator==(const StreamType& rhs) const { return type_ == rhs.type_; }

  bool IsInterest() const { return type_ == Type::kInterest; }
  bool IsWebFeed() const { return type_ == Type::kWebFeed; }

  // Returns a human-readable value, for debugging/DCHECK prints.
  std::string ToString() const;

 private:
  Type type_ = Type::kUnspecified;
};

inline std::ostream& operator<<(std::ostream& os,
                                const StreamType& stream_type) {
  return os << stream_type.ToString();
}

constexpr StreamType kInterestStream(StreamType::Type::kInterest);
constexpr StreamType kWebFeedStream(StreamType::Type::kWebFeed);

// This is the public access point for interacting with the Feed contents.
// FeedStreamApi serves multiple streams of data, one for each StreamType.
class FeedStreamApi {
 public:
  // Consumes stream data for a single `StreamType` and displays it to the user.
  class SurfaceInterface : public base::CheckedObserver {
   public:
    explicit SurfaceInterface(StreamType type);
    ~SurfaceInterface() override;

    // Returns a unique ID for the surface. The ID will not be reused until
    // after the Chrome process is closed.
    SurfaceId GetSurfaceId() const;

    // Returns the `StreamType` this `SurfaceInterface` requests.
    StreamType GetStreamType() const { return stream_type_; }

    // Called after registering the observer to provide the full stream state.
    // Also called whenever the stream changes.
    virtual void StreamUpdate(const feedui::StreamUpdate&) = 0;

    // Access to the xsurface data store.
    virtual void ReplaceDataStoreEntry(base::StringPiece key,
                                       base::StringPiece data) = 0;
    virtual void RemoveDataStoreEntry(base::StringPiece key) = 0;

   private:
    StreamType stream_type_;
    SurfaceId surface_id_;
  };

  FeedStreamApi();
  virtual ~FeedStreamApi();
  FeedStreamApi(const FeedStreamApi&) = delete;
  FeedStreamApi& operator=(const FeedStreamApi&) = delete;

  // Attach/detach a surface. Surfaces should be attached when content is
  // required for display, and detached when content is no longer shown.
  virtual void AttachSurface(SurfaceInterface*) = 0;
  virtual void DetachSurface(SurfaceInterface*) = 0;

  virtual bool IsArticlesListVisible() = 0;

  // Returns true if activity logging is enabled. The returned value is
  // ephemeral, this should be called for each candidate log, as it can change
  // as the feed is refreshed or the user signs in/out.
  virtual bool IsActivityLoggingEnabled() const = 0;

  // Returns the signed-in client_instance_id. This value is reset whenever the
  // feed stream is cleared (on sign-in, sign-out, and some data clear events).
  virtual std::string GetClientInstanceId() const = 0;

  // Returns the client's signed-out session id. This value is reset whenever
  // the feed stream is cleared (on sign-in, sign-out, and some data clear
  // events).
  virtual std::string GetSessionId() const = 0;

  // Invoked by RefreshTaskScheduler's scheduled task.
  virtual void ExecuteRefreshTask() = 0;

  // Request to load additional content at the end of the stream.
  // Calls |callback| when complete. If no content could be added, the parameter
  // is false, and the caller should expect |LoadMore| to fail if called
  // further.
  virtual void LoadMore(const SurfaceInterface& surface,
                        base::OnceCallback<void(bool)> callback) = 0;

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

  virtual PersistentKeyValueStore* GetPersistentKeyValueStore() = 0;

  // Apply |operations| to the stream model. Does nothing if the model is not
  // yet loaded.
  virtual void ExecuteOperations(
      const StreamType& stream_type,
      std::vector<feedstore::DataOperation> operations) = 0;

  // Create a temporary change that may be undone or committed later. Does
  // nothing if the model is not yet loaded.
  virtual EphemeralChangeId CreateEphemeralChange(
      const StreamType& stream_type,
      std::vector<feedstore::DataOperation> operations) = 0;
  // Same as |CreateEphemeralChange()|, but data is a serialized
  // |feedpacking::DismissData| message.
  virtual EphemeralChangeId CreateEphemeralChangeFromPackedData(
      const StreamType& stream_type,
      base::StringPiece data) = 0;
  // Commits a change. Returns false if the change does not exist.
  virtual bool CommitEphemeralChange(const StreamType& stream_type,
                                     EphemeralChangeId id) = 0;
  // Rejects a change. Returns false if the change does not exist.
  virtual bool RejectEphemeralChange(const StreamType& stream_type,
                                     EphemeralChangeId id) = 0;

  // Sends 'ThereAndBackAgainData' back to the server. |data| is a serialized
  // |feedwire::ThereAndBackAgainData| message.
  virtual void ProcessThereAndBackAgain(base::StringPiece data) = 0;
  // Saves a view action for eventual upload. |data| is a serialized
  //|feedwire::FeedAction| message.
  virtual void ProcessViewAction(base::StringPiece data) = 0;

  // User interaction reporting. These should have no side-effects other than
  // reporting metrics.

  // A slice was viewed (2/3rds of it is in the viewport). Should be called
  // once for each viewed slice in the stream.
  virtual void ReportSliceViewed(SurfaceId surface_id,
                                 const StreamType& stream_type,
                                 const std::string& slice_id) = 0;
  // Some feed content has been loaded and is now available to the user on the
  // feed surface. Reported only once after a surface is attached.
  virtual void ReportFeedViewed(SurfaceId surface_id) = 0;
  // A web page was loaded in response to opening a link from the Feed.
  virtual void ReportPageLoaded() = 0;
  // The user triggered the default open action, usually by tapping the card.
  virtual void ReportOpenAction(const StreamType& stream_type,
                                const std::string& slice_id) = 0;
  // The user triggered an open action, visited a web page, and then navigated
  // away or backgrouded the tab. |visit_time| is a measure of how long the
  // visited page was foregrounded.
  virtual void ReportOpenVisitComplete(base::TimeDelta visit_time) = 0;
  // The user triggered the 'open in new tab' action.
  virtual void ReportOpenInNewTabAction(const StreamType& stream_type,
                                        const std::string& slice_id) = 0;
  // The user scrolled the feed by |distance_dp| and then stopped.
  virtual void ReportStreamScrolled(int distance_dp) = 0;
  // The user started scrolling the feed. Typically followed by a call to
  // |ReportStreamScrolled()|.
  virtual void ReportStreamScrollStart() = 0;
  // Report that some user action occurred which does not have a specific
  // reporting function above..
  virtual void ReportOtherUserAction(FeedUserActionType action_type) = 0;

  // The following methods are used for the internals page.

  virtual DebugStreamData GetDebugStreamData() = 0;
  // Forces a Feed refresh from the server.
  virtual void ForceRefreshForDebugging() = 0;
  // Dumps some state information for debugging.
  virtual std::string DumpStateForDebugging() = 0;
  // Forces to render a StreamUpdate on all subsequent surface attaches.
  virtual void SetForcedStreamUpdateForDebugging(
      const feedui::StreamUpdate& stream_update) = 0;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_PUBLIC_FEED_STREAM_API_H_
