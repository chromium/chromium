// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/feedstore_util.h"

#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/feed_store.h"

namespace feedstore {
using feed::LocalActionId;
using feed::StreamType;

base::StringPiece StreamId(const StreamType& stream_type) {
  if (stream_type.IsForYou())
    return kForYouStreamId;
  DCHECK(stream_type.IsWebFeed());
  return kFollowStreamId;
}

int64_t ToTimestampMillis(base::Time t) {
  return (t - base::Time::UnixEpoch()).InMilliseconds();
}

base::Time FromTimestampMillis(int64_t millis) {
  return base::Time::UnixEpoch() + base::TimeDelta::FromMilliseconds(millis);
}

void SetLastAddedTime(base::Time t, feedstore::StreamData& data) {
  data.set_last_added_time_millis(ToTimestampMillis(t));
}

base::Time GetLastAddedTime(const feedstore::StreamData& data) {
  return FromTimestampMillis(data.last_added_time_millis());
}

base::Time GetSessionIdExpiryTime(const Metadata& metadata) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMilliseconds(
          metadata.session_id().expiry_time_ms()));
}

void SetSessionId(Metadata& metadata,
                  std::string token,
                  base::Time expiry_time) {
  Metadata::SessionID* session_id = metadata.mutable_session_id();
  session_id->set_token(std::move(token));
  session_id->set_expiry_time_ms(
      expiry_time.ToDeltaSinceWindowsEpoch().InMilliseconds());
}

base::Optional<Metadata> MaybeUpdateSessionId(
    const Metadata& metadata,
    base::Optional<std::string> token) {
  if (token && metadata.session_id().token() != *token) {
    base::Time expiry_time =
        token->empty()
            ? base::Time()
            : base::Time::Now() + feed::GetFeedConfig().session_id_max_age;
    auto new_metadata = metadata;
    SetSessionId(new_metadata, *token, expiry_time);
    return new_metadata;
  }
  return base::nullopt;
}

LocalActionId GetNextActionId(Metadata& metadata) {
  uint32_t id = metadata.next_action_id();
  // Never use 0, as that's an invalid LocalActionId.
  if (id == 0)
    ++id;
  metadata.set_next_action_id(id + 1);
  return LocalActionId(id);
}

const Metadata::StreamMetadata* FindMetadataForStream(
    const Metadata& metadata,
    const StreamType& stream_type) {
  base::StringPiece id = StreamId(stream_type);
  for (const auto& sm : metadata.stream_metadata()) {
    if (sm.stream_id() == id)
      return &sm;
  }
  return nullptr;
}

Metadata::StreamMetadata& MetadataForStream(Metadata& metadata,
                                            const StreamType& stream_type) {
  const Metadata::StreamMetadata* existing =
      FindMetadataForStream(metadata, stream_type);
  if (existing)
    return *const_cast<Metadata::StreamMetadata*>(existing);
  Metadata::StreamMetadata* sm = metadata.add_stream_metadata();
  sm->set_stream_id(StreamId(stream_type).as_string());
  return *sm;
}

void SetStreamViewTime(Metadata& metadata,
                       const StreamType& stream_type,
                       base::Time stream_last_added_time) {
  Metadata::StreamMetadata& sm = MetadataForStream(metadata, stream_type);
  sm.set_view_time_millis(ToTimestampMillis(stream_last_added_time));
}

base::Time GetStreamViewTime(const Metadata& metadata,
                             const StreamType& stream_type) {
  base::Time result;
  const Metadata::StreamMetadata* sm =
      FindMetadataForStream(metadata, stream_type);
  if (sm)
    result = FromTimestampMillis(sm->view_time_millis());
  return result;
}

feedstore::Metadata MakeMetadata(const std::string& gaia) {
  feedstore::Metadata md;
  md.set_stream_schema_version(feed::FeedStore::kCurrentStreamSchemaVersion);
  md.set_gaia(gaia);
  return md;
}

base::Optional<Metadata> SetStreamViewTime(const Metadata& metadata,
                                           const StreamType& stream_type,
                                           base::Time stream_last_added_time) {
  base::Optional<Metadata> result;
  if (GetStreamViewTime(metadata, stream_type) != stream_last_added_time) {
    result = metadata;
    SetStreamViewTime(*result, stream_type, stream_last_added_time);
  }
  return result;
}

}  // namespace feedstore
