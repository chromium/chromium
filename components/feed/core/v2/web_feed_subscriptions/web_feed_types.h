// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTIONS_WEB_FEED_TYPES_H_
#define COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTIONS_WEB_FEED_TYPES_H_

#include <iosfwd>
#include <optional>

#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/wire/web_feeds.pb.h"
#include "components/feed/core/v2/operation_token.h"
#include "components/feed/core/v2/public/types.h"

// Defines some types needed by WebFeedSubscriptionCoordinator and its models.

namespace feed {

struct WebFeedSubscriptionInfo {
  WebFeedSubscriptionStatus status = WebFeedSubscriptionStatus::kUnknown;
  feedstore::WebFeedInfo web_feed_info;
};

// Identifies how the in flight change should be processed.
enum class WebFeedInFlightChangeStrategy {
  // The follow/unfollow attempt is aborted upon network failure.
  kNotDurableRequest,
  // A new follow/unfollow attempt to be retried after failure.
  kNewDurableRequest,
  // A durable request, on subsequent tries.
  kRetry,
  // Used when the `InFlightChange` is stored for later execution.
  kPending,
};
std::ostream& operator<<(std::ostream& os,
                         const WebFeedInFlightChangeStrategy& strategy);

// Represents an in-progress attempt to change a Web Feed subscription.
struct WebFeedInFlightChange {
  // Maximum number of tries for a durable subscribe/unsubscribe operation.
  // TODO(b/205770750): Add metrics to help optimize this value.
  static constexpr int kMaxDurableOperationAttempts = 4;

  WebFeedInFlightChange();
  WebFeedInFlightChange(const WebFeedInFlightChange&);
  WebFeedInFlightChange(WebFeedInFlightChange&&);
  WebFeedInFlightChange& operator=(WebFeedInFlightChange&&);
  WebFeedInFlightChange& operator=(const WebFeedInFlightChange&);
  ~WebFeedInFlightChange();

  OperationToken token = OperationToken::MakeInvalid();
  // Either subscribing or unsubscribing.
  bool subscribing = false;
  WebFeedInFlightChangeStrategy strategy;
  // Set only when subscribing from a web page.
  std::optional<WebFeedPageInformation> page_information;
  // We may or may not know about this web feed when subscribing; always known
  // when unsubscribing.
  std::optional<feedstore::WebFeedInfo> web_feed_info;
  feedwire::webfeed::WebFeedChangeReason change_reason = feedwire::webfeed::
      WebFeedChangeReason::WEB_FEED_CHANGE_REASON_UNSPECIFIED;
};
std::ostream& operator<<(std::ostream& os, const WebFeedInFlightChange& change);

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTIONS_WEB_FEED_TYPES_H_
