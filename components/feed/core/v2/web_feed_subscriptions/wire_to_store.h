// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTIONS_WIRE_TO_STORE_H_
#define COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTIONS_WIRE_TO_STORE_H_

#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/wire/web_feeds.pb.h"

namespace feed {

feedstore::Image ConvertToStore(feedwire::webfeed::Image value);
feedstore::WebFeedInfo::State ConvertToStore(
    feedwire::webfeed::WebFeed::State value);
feedstore::WebFeedInfo ConvertToStore(feedwire::webfeed::WebFeed web_feed);

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTIONS_WIRE_TO_STORE_H_
