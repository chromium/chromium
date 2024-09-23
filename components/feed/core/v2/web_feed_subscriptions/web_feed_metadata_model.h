// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTIONS_WEB_FEED_METADATA_MODEL_H_
#define COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTIONS_WEB_FEED_METADATA_MODEL_H_

#include <iosfwd>
#include <string_view>
#include <vector>

#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/wire/web_feeds.pb.h"
#include "components/feed/core/v2/feed_store.h"
#include "components/feed/core/v2/operation_token.h"
#include "components/feed/core/v2/public/types.h"
#include "components/feed/core/v2/web_feed_subscriptions/web_feed_types.h"

// This file has some in-memory model definitions used internally by
// WebFeedSubscriptionCoordinator.
namespace feed {

// Unlike WebFeedSubscriptionModel, this model is loaded in at startup, but it
// has less data.
class WebFeedMetadataModel {
 public:
  struct Operation {
    // The stored operation.
    feedstore::PendingWebFeedOperation operation;
    // Data from `operation`, mirrored in an `WebFeedInFlightChange`.
    WebFeedInFlightChange change;
  };

  WebFeedMetadataModel(
      FeedStore* store,
      std::vector<feedstore::PendingWebFeedOperation> pending_operations);
  WebFeedMetadataModel(const WebFeedMetadataModel&) = delete;
  WebFeedMetadataModel& operator=(const WebFeedMetadataModel&) = delete;
  ~WebFeedMetadataModel();

  void AddPendingOperation(
      feedstore::PendingWebFeedOperation::Kind kind,
      const std::string& web_feed_id,
      feedwire::webfeed::WebFeedChangeReason change_reason);
  void RemovePendingOperationsForWebFeed(std::string_view web_feed_id);
  void RecordPendingOperationsForWebFeedAttempt(std::string_view web_feed_id);

  WebFeedInFlightChange* FindInFlightChange(std::string_view web_feed_id);
  const std::vector<Operation>& pending_operations() const {
    return pending_operations_;
  }

 private:
  static WebFeedInFlightChange MakePendingInFlightChange(
      const feedstore::PendingWebFeedOperation& operation);
  Operation* FindOperation(std::string_view web_feed_id);
  raw_ptr<FeedStore, DanglingUntriaged> store_;
  std::vector<Operation> pending_operations_;
  int next_id_ = 0;
};

std::ostream& operator<<(std::ostream& os,
                         const WebFeedMetadataModel::Operation& op);
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTIONS_WEB_FEED_METADATA_MODEL_H_
