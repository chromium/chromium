// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_TOPICS_BROWSING_TOPICS_SERVICE_H_
#define COMPONENTS_BROWSING_TOPICS_BROWSING_TOPICS_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"
#include "components/privacy_sandbox/canonical_topic.h"
#include "url/origin.h"

namespace browsing_topics {

// A profile keyed service for providing the topics to a requesting context or
// to other internal components (e.g. UX).
class BrowsingTopicsService : public KeyedService {
 public:
  // Return the topics (i.e. one topic from each epoch) that can be potentially
  // exposed to a given site. Up to `kBrowsingTopicsNumberOfEpochsToExpose`
  // epochs' topics can be returned.
  virtual std::vector<privacy_sandbox::CanonicalTopic>
  GetTopicsForSiteForDisplay(const url::Origin& top_origin) const = 0;

  // Return the top topics from all the past epochs. Up to
  // `kBrowsingTopicsNumberOfEpochsToExpose + 1` epochs' topics are kept in
  // the browser.
  virtual std::vector<privacy_sandbox::CanonicalTopic> GetTopTopicsForDisplay()
      const = 0;

  ~BrowsingTopicsService() override = default;
};

}  // namespace browsing_topics

#endif  // COMPONENTS_BROWSING_TOPICS_BROWSING_TOPICS_SERVICE_H_
