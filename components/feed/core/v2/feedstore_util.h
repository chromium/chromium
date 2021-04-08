// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_FEEDSTORE_UTIL_H_
#define COMPONENTS_FEED_CORE_V2_FEEDSTORE_UTIL_H_

#include <string>
#include "base/optional.h"
#include "base/time/time.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/core/v2/types.h"

namespace feedstore {
class Metadata;

constexpr base::StringPiece kForYouStreamId{"i"};
constexpr base::StringPiece kFollowStreamId{"w"};

base::StringPiece StreamId(const feed::StreamType& stream_type);

///////////////////////////////////////////////////
// Functions that operate on feedstore proto types.

int64_t ToTimestampMillis(base::Time t);
base::Time FromTimestampMillis(int64_t millis);
void SetLastAddedTime(base::Time t, feedstore::StreamData& data);

base::Time GetLastAddedTime(const feedstore::StreamData& data);
base::Time GetSessionIdExpiryTime(const feedstore::Metadata& metadata);
base::Time GetStreamViewTime(const Metadata& metadata,
                             const feed::StreamType& stream_type);
feedstore::Metadata MakeMetadata(const std::string& gaia);

// Mutations of Metadata. Metadata will need stored again after being changed,
// call `FeedStream::SetMetadata()`.
void SetSessionId(feedstore::Metadata& metadata,
                  std::string token,
                  base::Time expiry_time);
base::Optional<Metadata> MaybeUpdateSessionId(
    const feedstore::Metadata& metadata,
    base::Optional<std::string> token);
feed::LocalActionId GetNextActionId(feedstore::Metadata& metadata);
const feedstore::Metadata::StreamMetadata* FindMetadataForStream(
    const feed::StreamType& stream_type);
base::Optional<Metadata> SetStreamViewTime(const Metadata& metadata,
                                           const feed::StreamType& stream_type,
                                           base::Time stream_last_added_time);

}  // namespace feedstore

#endif  // COMPONENTS_FEED_CORE_V2_FEEDSTORE_UTIL_H_
