// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_MULTISTEP_FILTER_SERVICE_TEST_API_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_MULTISTEP_FILTER_SERVICE_TEST_API_H_

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "components/multistep_filter/core/extraction/filter_extractor.h"
#include "components/multistep_filter/core/multistep_filter_service.h"
#include "components/multistep_filter/core/suggestion/filter_suggestion_generator.h"

namespace multistep_filter {

// Helper class for testing `MultistepFilterService`.
class MultistepFilterServiceTestApi {
 public:
  explicit MultistepFilterServiceTestApi(MultistepFilterService* service)
      : service_(service) {}

  MultistepFilterServiceTestApi(const MultistepFilterServiceTestApi&) = delete;
  MultistepFilterServiceTestApi& operator=(
      const MultistepFilterServiceTestApi&) = delete;

  ~MultistepFilterServiceTestApi() = default;

  void set_filter_extractor(std::unique_ptr<FilterExtractor> filter_extractor) {
    service_->filter_extractor_ = std::move(filter_extractor);
  }
  void set_filter_suggestion_generator(
      std::unique_ptr<FilterSuggestionGenerator> filter_suggestion_generator) {
    service_->filter_suggestion_generator_ =
        std::move(filter_suggestion_generator);
  }

 private:
  raw_ptr<MultistepFilterService> service_;
};

inline MultistepFilterServiceTestApi test_api(MultistepFilterService& service) {
  return MultistepFilterServiceTestApi(&service);
}

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_MULTISTEP_FILTER_SERVICE_TEST_API_H_
