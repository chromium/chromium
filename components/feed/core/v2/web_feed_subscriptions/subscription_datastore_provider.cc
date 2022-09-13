// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/web_feed_subscriptions/subscription_datastore_provider.h"

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "components/feed/core/proto/v2/xsurface.pb.h"
#include "components/feed/core/v2/public/types.h"
#include "components/feed/core/v2/xsurface_datastore.h"

namespace feed {
namespace {
constexpr const char kWebFeedFollowStateKeyPrefix[] =
    "/app/webfeed-follow-state/";

feedxsurface::WebFeedFollowState::FollowState ToProtoState(
    WebFeedSubscriptionStatus status) {
  switch (status) {
    case WebFeedSubscriptionStatus::kUnknown:
      return feedxsurface::WebFeedFollowState::UNSPECIFIED;
    case WebFeedSubscriptionStatus::kSubscribed:
      return feedxsurface::WebFeedFollowState::FOLLOWED;
    case WebFeedSubscriptionStatus::kNotSubscribed:
      return feedxsurface::WebFeedFollowState::NOT_FOLLOWED;
    case WebFeedSubscriptionStatus::kSubscribeInProgress:
      return feedxsurface::WebFeedFollowState::FOLLOW_IN_PROGRESS;
    case WebFeedSubscriptionStatus::kUnsubscribeInProgress:
      return feedxsurface::WebFeedFollowState::UNFOLLOW_IN_PROGRESS;
  }
}

std::string MakeKey(const std::string& web_feed_id) {
  return base::StrCat({kWebFeedFollowStateKeyPrefix, web_feed_id});
}

std::string MakeEntry(WebFeedSubscriptionStatus status) {
  feedxsurface::WebFeedFollowState pb;
  pb.set_follow_state(ToProtoState(status));
  std::string binary;
  pb.SerializeToString(&binary);
  return binary;
}
}  // namespace

SubscriptionDatastoreProvider::SubscriptionDatastoreProvider(
    XsurfaceDatastoreDataWriter* writer)
    : writer_(writer) {}

SubscriptionDatastoreProvider::~SubscriptionDatastoreProvider() = default;

void SubscriptionDatastoreProvider::Update(
    std::vector<std::pair<std::string, WebFeedSubscriptionStatus>>
        new_state_list) {
  base::flat_map<std::string, WebFeedSubscriptionStatus> new_state(
      std::move(new_state_list));

  for (const auto& entry : new_state) {
    auto iter = state_.find(entry.first);
    if (iter != state_.end() && iter->second == entry.second)
      continue;
    writer_->UpdateDatastoreEntry(MakeKey(entry.first),
                                  MakeEntry(entry.second));
  }
  for (const auto& entry : state_) {
    if (!new_state.contains(entry.first))
      writer_->RemoveDatastoreEntry(MakeKey(entry.first));
  }
  state_ = std::move(new_state);
}

}  // namespace feed
