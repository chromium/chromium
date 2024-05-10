// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/public/test/stub_feed_api.h"

#include <string_view>
#include <type_traits>

namespace feed {

static_assert(!std::is_abstract_v<StubFeedApi>);

SurfaceId StubFeedApi::CreateSurface(const StreamType& type,
                                     SingleWebFeedEntryPoint entry_point) {
  return {};
}
WebFeedSubscriptions& StubFeedApi::subscriptions() {
  return web_feed_subscriptions_;
}
bool StubFeedApi::IsArticlesListVisible() {
  return {};
}
std::string StubFeedApi::GetSessionId() const {
  return {};
}
ImageFetchId StubFeedApi::FetchImage(
    const GURL& url,
    base::OnceCallback<void(NetworkResponse)> callback) {
  return {};
}
PersistentKeyValueStore& StubFeedApi::GetPersistentKeyValueStore() {
  return persistent_key_value_store_;
}
EphemeralChangeId StubFeedApi::CreateEphemeralChange(
    SurfaceId surface_id,
    std::vector<feedstore::DataOperation> operations) {
  return {};
}
EphemeralChangeId StubFeedApi::CreateEphemeralChangeFromPackedData(
    SurfaceId surface_id,
    std::string_view data) {
  return {};
}
bool StubFeedApi::CommitEphemeralChange(SurfaceId surface_id,
                                        EphemeralChangeId id) {
  return {};
}
bool StubFeedApi::RejectEphemeralChange(SurfaceId surface_id,
                                        EphemeralChangeId id) {
  return {};
}
bool StubFeedApi::WasUrlRecentlyNavigatedFromFeed(const GURL& url) {
  return {};
}
DebugStreamData StubFeedApi::GetDebugStreamData() {
  return {};
}
std::string StubFeedApi::DumpStateForDebugging() {
  return {};
}

base::Time StubFeedApi::GetLastFetchTime(SurfaceId surface_id) {
  return base::Time();
}

ContentOrder StubFeedApi::GetContentOrder(const StreamType& stream_type) const {
  return ContentOrder::kUnspecified;
}

ContentOrder StubFeedApi::GetContentOrderFromPrefs(
    const StreamType& stream_type) {
  return ContentOrder::kUnspecified;
}

}  // namespace feed
