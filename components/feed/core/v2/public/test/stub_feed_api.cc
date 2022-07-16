// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/public/test/stub_feed_api.h"

#include "base/compiler_specific.h"

namespace feed {

namespace {
ALLOW_UNUSED_TYPE void EnsureStubFeedApiHasNoPureVirtualFunctions() {
  (void)StubFeedApi();
}
}  // namespace

WebFeedSubscriptions& StubFeedApi::subscriptions() {
  return web_feed_subscriptions_;
}
bool StubFeedApi::IsArticlesListVisible() {
  return {};
}
bool StubFeedApi::IsActivityLoggingEnabled(
    const StreamType& stream_type) const {
  return {};
}
std::string StubFeedApi::GetClientInstanceId() const {
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
    const StreamType& stream_type,
    std::vector<feedstore::DataOperation> operations) {
  return {};
}
EphemeralChangeId StubFeedApi::CreateEphemeralChangeFromPackedData(
    const StreamType& stream_type,
    base::StringPiece data) {
  return {};
}
bool StubFeedApi::CommitEphemeralChange(const StreamType& stream_type,
                                        EphemeralChangeId id) {
  return {};
}
bool StubFeedApi::RejectEphemeralChange(const StreamType& stream_type,
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

base::Time StubFeedApi::GetLastFetchTime(const StreamType& stream_type) {
  return base::Time();
}

ContentOrder StubFeedApi::GetContentOrder(const StreamType& stream_type) {
  return ContentOrder::kUnspecified;
}

ContentOrder StubFeedApi::GetContentOrderFromPrefs(
    const StreamType& stream_type) {
  return ContentOrder::kUnspecified;
}

}  // namespace feed
