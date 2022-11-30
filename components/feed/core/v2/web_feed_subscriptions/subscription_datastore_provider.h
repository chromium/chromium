// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTIONS_SUBSCRIPTION_DATASTORE_PROVIDER_H_
#define COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTIONS_SUBSCRIPTION_DATASTORE_PROVIDER_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "components/feed/core/v2/public/types.h"

namespace feed {
class XsurfaceDatastoreDataWriter;

// Sends status of WebFeed subscriptions to xsurface's datastore.
class SubscriptionDatastoreProvider {
 public:
  explicit SubscriptionDatastoreProvider(XsurfaceDatastoreDataWriter* writer);
  SubscriptionDatastoreProvider(const SubscriptionDatastoreProvider&) = delete;
  SubscriptionDatastoreProvider& operator=(
      const SubscriptionDatastoreProvider&) = delete;
  ~SubscriptionDatastoreProvider();

  void Update(std::vector<std::pair<std::string, WebFeedSubscriptionStatus>>
                  current_state);

 private:
  raw_ptr<XsurfaceDatastoreDataWriter> writer_;

  // State that has been written to `writer_`.
  base::flat_map<std::string, WebFeedSubscriptionStatus> state_;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTIONS_SUBSCRIPTION_DATASTORE_PROVIDER_H_
