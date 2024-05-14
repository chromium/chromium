// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/feedstore_util.h"

#include <string_view>

#include "base/base64url.h"
#include "base/hash/hash.h"
#include "base/strings/strcat.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/wire/consistency_token.pb.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/feed_store.h"
#include "components/feed/core/v2/public/stream_type.h"

namespace feedstore {
using feed::LocalActionId;
using feed::StreamType;

// This returns a string version of StreamType which can be used in datastore
// keys.
std::string StreamKey(const StreamType& stream_type) {
  if (stream_type.IsForYou())
    return kForYouStreamKey;
  if (stream_type.IsWebFeed())
    return kFollowStreamKey;
  if (stream_type.IsForSupervisedUser()) {
    return kSupervisedUserStreamKey;
  }
  DCHECK(stream_type.IsSingleWebFeed());
  std::string encoding;
  base::Base64UrlEncode(stream_type.GetWebFeedId(),
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &encoding);
  if (stream_type.IsSingleWebFeedEntryMenu()) {
    return base::StrCat({std::string(kSingleWebFeedStreamKeyPrefix), "/",
                         std::string(kSingleWebFeedMenuStreamKeyPrefix),
                         encoding});
  } else {
    return base::StrCat({std::string(kSingleWebFeedStreamKeyPrefix), "/",
                         std::string(kSingleWebFeedOtherStreamKeyPrefix),
                         encoding});
  }
}

std::string_view StreamPrefix(feed::StreamKind stream_kind) {
  switch (stream_kind) {
    case feed::StreamKind::kForYou:
      return kForYouStreamKey;
    case feed::StreamKind::kFollowing:
      return kFollowStreamKey;
    case feed::StreamKind::kSupervisedUser:
      return kSupervisedUserStreamKey;
    case feed::StreamKind::kSingleWebFeed:
      return kSingleWebFeedStreamKeyPrefix;
    case feed::StreamKind::kUnknown:
      NOTREACHED_IN_MIGRATION();
      return kSingleWebFeedStreamKeyPrefix;
  }
}

StreamType DecodeSingleWebFeedKeySuffix(
    std::string_view suffix,
    feed::SingleWebFeedEntryPoint entry_point,
    std::string_view prefix) {
  if (base::StartsWith(suffix, prefix, base::CompareCase::SENSITIVE)) {
    suffix.remove_prefix(prefix.size());
    std::string single_web_feed_key;
    if (base::Base64UrlDecode(suffix,
                              base::Base64UrlDecodePolicy::IGNORE_PADDING,
                              &single_web_feed_key)) {
      return StreamType(feed::StreamKind::kSingleWebFeed, single_web_feed_key,
                        entry_point);
    }
  }
  return {};
}

StreamType StreamTypeFromKey(std::string_view id) {
  if (id == kForYouStreamKey)
    return StreamType(feed::StreamKind::kForYou);
  if (id == kFollowStreamKey)
    return StreamType(feed::StreamKind::kFollowing);
  if (id == kSupervisedUserStreamKey) {
    return StreamType(feed::StreamKind::kSupervisedUser);
  }
  if (base::StartsWith(id, kSingleWebFeedStreamKeyPrefix,
                       base::CompareCase::SENSITIVE)) {
    if ((id.size() < (kSingleWebFeedStreamKeyPrefix.size() +
                      kSingleWebFeedMenuStreamKeyPrefix.size() + 1))) {
      return {};
    }
    // add  +1 to account for the '/' separating the c/[mo]/webid
    std::string_view substr =
        id.substr(kSingleWebFeedStreamKeyPrefix.size() + 1);
    StreamType result = DecodeSingleWebFeedKeySuffix(
        substr, feed::SingleWebFeedEntryPoint::kMenu,
        kSingleWebFeedMenuStreamKeyPrefix);
    if (!result.IsValid()) {
      result = DecodeSingleWebFeedKeySuffix(
          substr, feed::SingleWebFeedEntryPoint::kOther,
          kSingleWebFeedOtherStreamKeyPrefix);
    }
    return result;
  }
  return {};
}

int64_t ToTimestampMillis(base::Time t) {
  return t.is_null() ? 0L : (t - base::Time::UnixEpoch()).InMilliseconds();
}

base::Time FromTimestampMillis(int64_t millis) {
  return base::Time::UnixEpoch() + base::Milliseconds(millis);
}

int64_t ToTimestampNanos(base::Time t) {
  return t.is_null() ? 0L : (t - base::Time::UnixEpoch()).InNanoseconds();
}

base::Time FromTimestampMicros(int64_t micros) {
  return base::Time::UnixEpoch() + base::Microseconds(micros);
}

void SetLastAddedTime(base::Time t, feedstore::StreamData& data) {
  data.set_last_added_time_millis(ToTimestampMillis(t));
}

base::Time GetLastAddedTime(const feedstore::StreamData& data) {
  return FromTimestampMillis(data.last_added_time_millis());
}

base::Time GetSessionIdExpiryTime(const Metadata& metadata) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::Milliseconds(metadata.session_id().expiry_time_ms()));
}

void SetSessionId(Metadata& metadata,
                  std::string token,
                  base::Time expiry_time) {
  Metadata::SessionID* session_id = metadata.mutable_session_id();
  session_id->set_token(std::move(token));
  session_id->set_expiry_time_ms(
      expiry_time.ToDeltaSinceWindowsEpoch().InMilliseconds());
}

void SetContentLifetime(
    feedstore::Metadata& metadata,
    const StreamType& stream_type,
    feedstore::Metadata::StreamMetadata::ContentLifetime content_lifetime) {
  feedstore::Metadata::StreamMetadata& stream_metadata =
      feedstore::MetadataForStream(metadata, stream_type);
  *stream_metadata.mutable_content_lifetime() = std::move(content_lifetime);
}

void MaybeUpdateSessionId(Metadata& metadata,
                          std::optional<std::string> token) {
  if (token && metadata.session_id().token() != *token) {
    base::Time expiry_time =
        token->empty()
            ? base::Time()
            : base::Time::Now() + feed::GetFeedConfig().session_id_max_age;
    SetSessionId(metadata, *token, expiry_time);
  }
}

std::optional<Metadata> MaybeUpdateConsistencyToken(
    const feedstore::Metadata& metadata,
    const feedwire::ConsistencyToken& token) {
  if (token.has_token() && metadata.consistency_token() != token.token()) {
    auto metadata_copy = metadata;
    metadata_copy.set_consistency_token(token.token());
    return metadata_copy;
  }
  return std::nullopt;
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
  std::string key = StreamKey(stream_type);
  for (const auto& sm : metadata.stream_metadata()) {
    if (sm.stream_key() == key)
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
  sm->set_stream_key(std::string(StreamKey(stream_type)));
  return *sm;
}

void SetStreamViewContentHashes(Metadata& metadata,
                                const StreamType& stream_type,
                                const feed::ContentHashSet& content_hashes) {
  Metadata::StreamMetadata& stream_metadata =
      MetadataForStream(metadata, stream_type);
  stream_metadata.clear_view_content_hashes();
  stream_metadata.mutable_view_content_hashes()->Add(
      content_hashes.original_hashes().begin(),
      content_hashes.original_hashes().end());
}

bool IsKnownStale(const Metadata& metadata, const StreamType& stream_type) {
  const Metadata::StreamMetadata* sm =
      FindMetadataForStream(metadata, stream_type);
  return sm ? sm->is_known_stale() : false;
}

base::Time GetLastFetchTime(const Metadata& metadata,
                            const feed::StreamType& stream_type) {
  const Metadata::StreamMetadata* sm =
      FindMetadataForStream(metadata, stream_type);
  return sm ? FromTimestampMillis(sm->last_fetch_time_millis()) : base::Time();
}

void SetLastFetchTime(Metadata& metadata,
                      const StreamType& stream_type,
                      const base::Time& fetch_time) {
  Metadata::StreamMetadata& stream_metadata =
      MetadataForStream(metadata, stream_type);
  stream_metadata.set_last_fetch_time_millis(ToTimestampMillis(fetch_time));
}

feedstore::Metadata MakeMetadata(const std::string& gaia) {
  feedstore::Metadata md;
  md.set_stream_schema_version(feed::FeedStore::kCurrentStreamSchemaVersion);
  md.set_gaia(gaia);
  return md;
}

feedstore::DocView CreateDocView(uint64_t docid, base::Time timestamp) {
  feedstore::DocView doc_view;
  doc_view.set_docid(docid);
  doc_view.set_view_time_millis(feedstore::ToTimestampMillis(timestamp));
  return doc_view;
}

std::optional<Metadata> SetStreamViewContentHashes(
    const Metadata& metadata,
    const StreamType& stream_type,
    const feed::ContentHashSet& content_hashes) {
  std::optional<Metadata> result;
  if (!(GetViewContentIds(metadata, stream_type) == content_hashes)) {
    result = metadata;
    SetStreamViewContentHashes(*result, stream_type, content_hashes);
  }
  return result;
}

feed::ContentHashSet GetContentIds(const StreamData& stream_data) {
  return feed::ContentHashSet{{stream_data.content_hashes().begin(),
                               stream_data.content_hashes().end()}};
}
feed::ContentHashSet GetViewContentIds(const Metadata& metadata,
                                       const StreamType& stream_type) {
  const Metadata::StreamMetadata* stream_metadata =
      FindMetadataForStream(metadata, stream_type);
  if (stream_metadata) {
    return feed::ContentHashSet({stream_metadata->view_content_hashes().begin(),
                                 stream_metadata->view_content_hashes().end()});
  }
  return {};
}

void SetLastServerResponseTime(Metadata& metadata,
                               const feed::StreamType& stream_type,
                               const base::Time& server_time) {
  Metadata::StreamMetadata& stream_metadata =
      MetadataForStream(metadata, stream_type);
  stream_metadata.set_last_server_response_time_millis(
      ToTimestampMillis(server_time));
}

int32_t ContentHashFromPrefetchMetadata(
    const feedwire::PrefetchMetadata& prefetch_metadata) {
  return base::PersistentHash(prefetch_metadata.uri());
}

base::flat_set<uint32_t> GetViewedContentHashes(const Metadata& metadata,
                                                const StreamType& stream_type) {
  const Metadata::StreamMetadata* stream_metadata =
      FindMetadataForStream(metadata, stream_type);
  if (stream_metadata) {
    return base::flat_set<uint32_t>(
        stream_metadata->viewed_content_hashes().begin(),
        stream_metadata->viewed_content_hashes().end());
  }
  return {};
}

}  // namespace feedstore
