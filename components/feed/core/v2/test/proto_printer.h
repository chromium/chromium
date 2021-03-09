// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_TEST_PROTO_PRINTER_H_
#define COMPONENTS_FEED_CORE_V2_TEST_PROTO_PRINTER_H_

#include <ostream>
#include <string>

#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/ui.pb.h"

namespace feedwire {
class ActionPayload;
class ClientInfo;
class ContentId;
class DisplayInfo;
class Version;
}  // namespace feedwire
namespace feed {
struct StreamModelUpdateRequest;
#define DECLARE_PRINTER(PROTO_TYPE)                                        \
  std::string ToTextProto(const PROTO_TYPE& v);                            \
  inline std::ostream& operator<<(std::ostream& os, const PROTO_TYPE& v) { \
    return os << ToTextProto(v);                                           \
  }

DECLARE_PRINTER(feedwire::ContentId)
DECLARE_PRINTER(feedwire::Version)
DECLARE_PRINTER(feedwire::DisplayInfo)
DECLARE_PRINTER(feedwire::ClientInfo)
DECLARE_PRINTER(feedwire::ActionPayload)
DECLARE_PRINTER(feedstore::StreamData)
DECLARE_PRINTER(feedstore::Metadata)
DECLARE_PRINTER(feedstore::StreamStructureSet)
DECLARE_PRINTER(feedstore::StreamStructure)
DECLARE_PRINTER(feedstore::Content)
DECLARE_PRINTER(feedstore::StreamSharedState)
DECLARE_PRINTER(feedstore::StoredAction)
DECLARE_PRINTER(feedstore::Record)
DECLARE_PRINTER(feedstore::DataOperation)
DECLARE_PRINTER(feedstore::WebFeedInfo)
DECLARE_PRINTER(feedstore::RecommendedWebFeedIndex)
DECLARE_PRINTER(feedstore::SubscribedWebFeeds)
DECLARE_PRINTER(feedstore::Image)
DECLARE_PRINTER(feedstore::UriMatcher)
DECLARE_PRINTER(feedui::StreamUpdate)

#undef DECLARE_PRINTER

std::ostream& operator<<(std::ostream& os, const StreamModelUpdateRequest& v);

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_TEST_PROTO_PRINTER_H_
