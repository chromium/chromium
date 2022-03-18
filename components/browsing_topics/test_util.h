// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_TOPICS_TEST_UTIL_H_
#define COMPONENTS_BROWSING_TOPICS_TEST_UTIL_H_

#include "base/containers/queue.h"
#include "components/browsing_topics/browsing_topics_calculator.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace browsing_topics {

// A tester class that allows mocking the generated random numbers.
class TesterBrowsingTopicsCalculator : public BrowsingTopicsCalculator {
 public:
  // Initialize a regular `BrowsingTopicsCalculator` with an additional
  // `rand_uint64_queue` member for generating random numbers.
  TesterBrowsingTopicsCalculator(
      privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
      history::HistoryService* history_service,
      content::BrowsingTopicsSiteDataManager* site_data_manager,
      optimization_guide::PageContentAnnotationsService* annotations_service,
      CalculateCompletedCallback callback,
      base::queue<uint64_t> rand_uint64_queue);

  ~TesterBrowsingTopicsCalculator() override;

  TesterBrowsingTopicsCalculator(const TesterBrowsingTopicsCalculator&) =
      delete;
  TesterBrowsingTopicsCalculator& operator=(
      const TesterBrowsingTopicsCalculator&) = delete;
  TesterBrowsingTopicsCalculator(TesterBrowsingTopicsCalculator&&) = delete;
  TesterBrowsingTopicsCalculator& operator=(TesterBrowsingTopicsCalculator&&) =
      delete;

  // Pop and return the next number in `rand_uint64_queue_`. Precondition:
  // `rand_uint64_queue_` is not empty.
  uint64_t GenerateRandUint64() override;

 private:
  base::queue<uint64_t> rand_uint64_queue_;
};

}  // namespace browsing_topics

#endif  // COMPONENTS_BROWSING_TOPICS_TEST_UTIL_H_
