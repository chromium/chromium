// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/web_feed_subscriptions/web_feed_types.h"

#include <ostream>

namespace feed {

WebFeedInFlightChange::WebFeedInFlightChange() = default;
WebFeedInFlightChange::WebFeedInFlightChange(const WebFeedInFlightChange&) =
    default;
WebFeedInFlightChange::WebFeedInFlightChange(WebFeedInFlightChange&&) = default;
WebFeedInFlightChange& WebFeedInFlightChange::operator=(
    WebFeedInFlightChange&&) = default;
WebFeedInFlightChange& WebFeedInFlightChange::operator=(
    const WebFeedInFlightChange&) = default;
WebFeedInFlightChange::~WebFeedInFlightChange() = default;

std::ostream& operator<<(std::ostream& os,
                         const WebFeedInFlightChange& change) {
  os << "Change{";
  if (change.strategy != WebFeedInFlightChangeStrategy::kPending &&
      !change.token)
    os << "Expired ";

  os << (change.subscribing ? "subscribing " : "unsubscribing ");
  os << change.strategy << " ";
  if (change.page_information)
    os << *change.page_information << ' ';

  if (change.web_feed_info)
    os << "web_feed_id=" << change.web_feed_info->web_feed_id() << ' ';

  return os << "}";
}

std::ostream& operator<<(std::ostream& os,
                         const WebFeedInFlightChangeStrategy& strategy) {
  switch (strategy) {
    case WebFeedInFlightChangeStrategy::kNotDurableRequest:
      return os << "regular";
    case WebFeedInFlightChangeStrategy::kNewDurableRequest:
      return os << "durable";
    case WebFeedInFlightChangeStrategy::kRetry:
      return os << "retry";
    case WebFeedInFlightChangeStrategy::kPending:
      return os << "pending";
  }
}
}  // namespace feed
