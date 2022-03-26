// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_TYPES_H_
#define COMPONENTS_FEED_CORE_V2_TYPES_H_

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "base/types/id_type.h"
#include "base/values.h"
#include "components/feed/core/proto/v2/wire/client_info.pb.h"
#include "components/feed/core/proto/v2/wire/reliability_logging_enums.pb.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/public/common_enums.h"
#include "components/feed/core/v2/public/types.h"

namespace feed {

// Make sure public types are included here too.
// See components/feed/core/v2/public/types.h.
using ::feed::ChromeInfo;
using ::feed::EphemeralChangeId;
using ::feed::Experiments;
using ::feed::WebFeedSubscriptionRequestStatus;
using ::feed::WebFeedSubscriptionStatus;

// Uniquely identifies a revision of a |feedstore::Content|. If Content changes,
// it is assigned a new revision number.
using ContentRevision = base::IdTypeU32<class ContentRevisionClass>;

// ID for a stored pending action.
using LocalActionId = base::IdType32<class LocalActionIdClass>;

std::string ToString(ContentRevision c);
ContentRevision ToContentRevision(const std::string& str);

// Metadata sent with Feed requests.
struct RequestMetadata {
  RequestMetadata();
  ~RequestMetadata();
  RequestMetadata(RequestMetadata&&);
  RequestMetadata& operator=(RequestMetadata&&);

  feedwire::ClientInfo ToClientInfo() const;

  ChromeInfo chrome_info;
  std::string language_tag;
  std::string client_instance_id;
  std::string session_id;
  DisplayMetrics display_metrics;
  ContentOrder content_order = ContentOrder::kUnspecified;
  bool notice_card_acknowledged = false;
  bool autoplay_enabled = false;
  int followed_from_web_page_menu_count = 0;
  std::vector<std::string> acknowledged_notice_keys;
};

// Data internal to MetricsReporter which is persisted to Prefs.
struct PersistentMetricsData {
  // The midnight time for the day in which this metric was recorded.
  base::Time current_day_start;
  // The total recorded time spent on the Feed for the current day.
  base::TimeDelta accumulated_time_spent_in_feed;
};

base::Value PersistentMetricsDataToValue(const PersistentMetricsData& data);
PersistentMetricsData PersistentMetricsDataFromValue(const base::Value& value);

class LoadLatencyTimes {
 public:
  enum StepKind {
    // Time from when the LoadStreamTask was created to when it is executed.
    kTaskExecution,
    // Time spent loading the stream state from storage.
    kLoadFromStore,
    // Time spent querying for and uploading stored actions. Recorded even if
    // no actions are uploaded.
    kUploadActions,
    // Time spent making the FeedQuery (or WebFeed List Contents) request.
    kQueryRequest,
    // A view was reported in the stream, indicating the stream was shown.
    kStreamViewed,
  };
  struct Step {
    StepKind kind;
    base::TimeDelta latency;
  };

  LoadLatencyTimes();
  ~LoadLatencyTimes();
  LoadLatencyTimes(const LoadLatencyTimes&) = delete;
  LoadLatencyTimes& operator=(const LoadLatencyTimes&) = delete;

  void StepComplete(StepKind kind);

  const std::vector<Step>& steps() const { return steps_; }

 private:
  base::TimeTicks last_time_;
  std::vector<Step> steps_;
};

// Tracks a set of `feedstore::Content` content IDs, for tracking whether unread
// content is received from the server.
class ContentIdSet {
 public:
  ContentIdSet();
  ~ContentIdSet();
  explicit ContentIdSet(base::flat_set<int64_t> ids);
  ContentIdSet(const ContentIdSet&);
  ContentIdSet(ContentIdSet&&);
  ContentIdSet& operator=(const ContentIdSet&);
  ContentIdSet& operator=(ContentIdSet&&);

  // Returns whether this set contains all items.
  bool ContainsAllOf(const ContentIdSet& items) const;
  bool IsEmpty() const;
  const base::flat_set<int64_t>& values() const { return content_ids_; }

  bool operator==(const ContentIdSet& rhs) const;

 private:
  // Note, we only store the `id` field of ContentId, with the assumption that
  // `id` is unique enough given these are only `feedstore::Content` ids.
  base::flat_set<int64_t> content_ids_;
};

std::ostream& operator<<(std::ostream& s, const ContentIdSet& id_set);

struct ContentStats {
  int card_count = 0;
  int total_content_frame_size_bytes = 0;
  int shared_state_size = 0;
};

struct LaunchResult {
  LoadStreamStatus load_stream_status;
  feedwire::DiscoverLaunchResult launch_result;

  LaunchResult(LoadStreamStatus load_stream_status,
               feedwire::DiscoverLaunchResult launch_result);
  LaunchResult(const LaunchResult& other);
  ~LaunchResult();
  LaunchResult& operator=(const LaunchResult& other);
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_TYPES_H_
