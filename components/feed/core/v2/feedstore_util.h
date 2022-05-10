// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_FEEDSTORE_UTIL_H_
#define COMPONENTS_FEED_CORE_V2_FEEDSTORE_UTIL_H_

#include <string>
#include "base/strings/string_piece_forward.h"
#include "base/time/time.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/v2/public/stream_type.h"
#include "components/feed/core/v2/types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace feedwire {
class ConsistencyToken;
}

namespace feedstore {
class Metadata;

constexpr base::StringPiece kForYouStreamId{"i"};
constexpr base::StringPiece kFollowStreamId{"w"};

base::StringPiece StreamId(const feed::StreamType& stream_type);
feed::StreamType StreamTypeFromId(base::StringPiece id);

///////////////////////////////////////////////////
// Functions that operate on feedstore proto types.

int64_t ToTimestampMillis(base::Time t);
base::Time FromTimestampMillis(int64_t millis);
void SetLastAddedTime(base::Time t, feedstore::StreamData& data);
void SetLastServerResponseTime(base::Time t, feedstore::StreamData& data);

base::Time GetLastAddedTime(const feedstore::StreamData& data);
base::Time GetSessionIdExpiryTime(const feedstore::Metadata& metadata);
base::Time GetStreamViewTime(const Metadata& metadata,
                             const feed::StreamType& stream_type);
base::Time GetLastServerResponseTime(const feedstore::StreamData& data);

bool IsKnownStale(const Metadata& metadata,
                  const feed::StreamType& stream_type);
base::Time GetLastFetchTime(const Metadata& metadata,
                            const feed::StreamType& stream_type);
void SetLastFetchTime(Metadata& metadata,
                      const feed::StreamType& stream_type,
                      const base::Time& fetch_time);
feedstore::Metadata MakeMetadata(const std::string& gaia);

// Mutations of Metadata. Metadata will need stored again after being changed,
// call `FeedStream::SetMetadata()`.
void SetSessionId(feedstore::Metadata& metadata,
                  std::string token,
                  base::Time expiry_time);
void SetContentLifetime(
    feedstore::Metadata& metadata,
    const feed::StreamType& stream_type,
    feedstore::Metadata::StreamMetadata::ContentLifetime content_lifetime);
void MaybeUpdateSessionId(feedstore::Metadata& metadata,
                          absl::optional<std::string> token);
absl::optional<Metadata> MaybeUpdateConsistencyToken(
    const feedstore::Metadata& metadata,
    const feedwire::ConsistencyToken& token);
feed::LocalActionId GetNextActionId(feedstore::Metadata& metadata);
const Metadata::StreamMetadata* FindMetadataForStream(
    const Metadata& metadata,
    const feed::StreamType& stream_type);
Metadata::StreamMetadata& MetadataForStream(
    Metadata& metadata,
    const feed::StreamType& stream_type);
absl::optional<Metadata> SetStreamViewContentIds(
    const Metadata& metadata,
    const feed::StreamType& stream_type,
    const feed::ContentIdSet& content_ids);
feed::ContentIdSet GetContentIds(const StreamData& stream_data);
feed::ContentIdSet GetViewContentIds(const Metadata& metadata,
                                     const feed::StreamType& stream_type);

}  // namespace feedstore

#endif  // COMPONENTS_FEED_CORE_V2_FEEDSTORE_UTIL_H_
