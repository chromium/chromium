// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_TOPICS_BROWSING_TOPICS_SERVICE_H_
#define COMPONENTS_BROWSING_TOPICS_BROWSING_TOPICS_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"
#include "components/privacy_sandbox/canonical_topic.h"

namespace browsing_topics {

// A profile keyed service for providing the topics to a requesting context or
// to other internal components (e.g. UX).
class BrowsingTopicsService : public KeyedService {
 public:
  // Return the top topics from all the past epochs. Up to
  // `kBrowsingTopicsNumberOfEpochsToExpose + 1` epochs' topics are kept in
  // the browser. Padded top topics won't be returned.
  virtual std::vector<privacy_sandbox::CanonicalTopic> GetTopTopicsForDisplay()
      const = 0;

  // Clear all topics data (both raw and derived).
  virtual void ClearAllTopicsData() = 0;

  ~BrowsingTopicsService() override = default;
};

}  // namespace browsing_topics

#endif  // COMPONENTS_BROWSING_TOPICS_BROWSING_TOPICS_SERVICE_H_
