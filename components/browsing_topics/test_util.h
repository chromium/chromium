// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_TOPICS_TEST_UTIL_H_
#define COMPONENTS_BROWSING_TOPICS_TEST_UTIL_H_

#include "components/browsing_topics/browsing_topics_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace browsing_topics {

class MockBrowsingTopicsService : public BrowsingTopicsService {
 public:
  MockBrowsingTopicsService();
  ~MockBrowsingTopicsService() override;

  MOCK_METHOD(std::vector<privacy_sandbox::CanonicalTopic>,
              GetTopTopicsForDisplay,
              (),
              (const override));
  MOCK_METHOD(void, ClearAllTopicsData, (), (override));
};

}  // namespace browsing_topics

#endif  // COMPONENTS_BROWSING_TOPICS_TEST_UTIL_H_
