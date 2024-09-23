// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/web_feed_subscriptions/web_feed_metadata_model.h"

#include <ostream>
#include <string_view>

#include "components/feed/core/proto/v2/wire/web_feeds.pb.h"

namespace feed {

WebFeedMetadataModel::WebFeedMetadataModel(
    FeedStore* store,
    std::vector<feedstore::PendingWebFeedOperation> pending_operations)
    : store_(store) {
  for (const feedstore::PendingWebFeedOperation& operation :
       pending_operations) {
    if (next_id_ <= operation.id()) {
      next_id_ = operation.id() + 1;
    }
    pending_operations_.push_back(
        {operation, MakePendingInFlightChange(operation)});
  }
}

WebFeedMetadataModel::~WebFeedMetadataModel() = default;

void WebFeedMetadataModel::AddPendingOperation(
    feedstore::PendingWebFeedOperation::Kind kind,
    const std::string& web_feed_id,
    feedwire::webfeed::WebFeedChangeReason change_reason) {
  // Don't allow more than one operation for a web feed, just overwrite the
  // old one.
  RemovePendingOperationsForWebFeed(web_feed_id);
  feedstore::PendingWebFeedOperation operation;
  operation.set_kind(kind);
  operation.set_web_feed_id(web_feed_id);
  operation.set_id(next_id_++);
  operation.set_change_reason(change_reason);
  store_->WritePendingWebFeedOperation(operation);

  pending_operations_.push_back(
      {operation, MakePendingInFlightChange(operation)});
}

void WebFeedMetadataModel::RemovePendingOperationsForWebFeed(
    std::string_view web_feed_id) {
  for (auto it = pending_operations_.begin(); it != pending_operations_.end();
       ++it) {
    if (it->operation.web_feed_id() != web_feed_id)
      continue;
    store_->RemovePendingWebFeedOperation(it->operation.id());
    pending_operations_.erase(it);
    break;
  }
}

void WebFeedMetadataModel::RecordPendingOperationsForWebFeedAttempt(
    std::string_view web_feed_id) {
  for (Operation& op : pending_operations_) {
    if (op.operation.web_feed_id() != web_feed_id)
      continue;
    if (op.operation.attempts() >=
        WebFeedInFlightChange::kMaxDurableOperationAttempts) {
      RemovePendingOperationsForWebFeed(web_feed_id);
      return;
    }
    op.operation.set_attempts(op.operation.attempts() + 1);
    store_->WritePendingWebFeedOperation(op.operation);
    return;
  }
}

WebFeedInFlightChange* WebFeedMetadataModel::FindInFlightChange(
    std::string_view web_feed_id) {
  Operation* op = FindOperation(web_feed_id);
  return op ? &op->change : nullptr;
}

// static
WebFeedInFlightChange WebFeedMetadataModel::MakePendingInFlightChange(
    const feedstore::PendingWebFeedOperation& operation) {
  WebFeedInFlightChange change;
  change.subscribing =
      operation.kind() == feedstore::PendingWebFeedOperation::SUBSCRIBE;
  change.strategy = WebFeedInFlightChangeStrategy::kPending;
  change.web_feed_info = feedstore::WebFeedInfo();
  change.web_feed_info->set_web_feed_id(operation.web_feed_id());
  change.change_reason = operation.change_reason();
  return change;
}

WebFeedMetadataModel::Operation* WebFeedMetadataModel::FindOperation(
    std::string_view web_feed_id) {
  for (auto& op : pending_operations_) {
    if (op.operation.web_feed_id() == web_feed_id)
      return &op;
  }
  return nullptr;
}

std::ostream& operator<<(std::ostream& os,
                         const WebFeedMetadataModel::Operation& op) {
  return os << feedstore::PendingWebFeedOperation_Kind_Name(op.operation.kind())
            << " " << op.operation.web_feed_id()
            << " attempts=" << op.operation.attempts();
}

}  // namespace feed
