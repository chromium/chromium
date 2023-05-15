// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PROTO_UTIL_H_
#define COMPONENTS_FEED_CORE_V2_PROTO_UTIL_H_

#include <string>

#include "components/feed/core/proto/v2/wire/client_info.pb.h"
#include "components/feed/core/proto/v2/wire/content_id.pb.h"
#include "components/feed/core/proto/v2/wire/feed_query.pb.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/core/v2/types.h"
#include "components/feed/core/v2/view_demotion.h"

namespace feedwire {
class Request;
}  // namespace feedwire
namespace feedstore {
class Content;
}  // namespace feedstore

// Helper functions/classes for dealing with feed proto messages.

namespace feed {
using ContentId = feedwire::ContentId;

std::string ContentIdString(const feedwire::ContentId&);
bool Equal(const feedwire::ContentId& a, const feedwire::ContentId& b);
bool CompareContentId(const feedwire::ContentId& a,
                      const feedwire::ContentId& b);
bool CompareContent(const feedstore::Content& a, const feedstore::Content& b);

class ContentIdCompareFunctor {
 public:
  bool operator()(const feedwire::ContentId& a,
                  const feedwire::ContentId& b) const {
    return CompareContentId(a, b);
  }
};

class ContentCompareFunctor {
 public:
  bool operator()(const feedstore::Content& a,
                  const feedstore::Content& b) const {
    return CompareContent(a, b);
  }
};

feedwire::ClientInfo CreateClientInfo(const RequestMetadata& request_metadata);

feedwire::Request CreateFeedQueryRefreshRequest(
    const StreamType& stream_type,
    feedwire::FeedQuery::RequestReason request_reason,
    const RequestMetadata& request_metadata,
    const std::string& consistency_token,
    const SingleWebFeedEntryPoint single_feed_entry_point,
    const std::vector<DocViewCount> doc_view_counts);

feedwire::Request CreateFeedQueryLoadMoreRequest(
    const RequestMetadata& request_metadata,
    const std::string& consistency_token,
    const std::string& next_page_token);

template <typename MESSAGE>
void SetConsistencyToken(MESSAGE& msg, const std::string& consistency_token) {
  if (!consistency_token.empty()) {
    msg.mutable_consistency_token()->set_token(consistency_token);
  }
}

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_PROTO_UTIL_H_
