// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_FILTER_TAB_CONTROLLER_TEST_API_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_FILTER_TAB_CONTROLLER_TEST_API_H_

#include "base/memory/raw_ref.h"
#include "components/multistep_filter/core/extraction/filter_extractor.h"
#include "components/multistep_filter/core/filter_tab_controller.h"
#include "components/multistep_filter/core/suggestion/filter_suggestion_generator.h"

namespace multistep_filter {

// Helper class for testing `FilterTabController`.
class FilterTabControllerTestApi {
 public:
  explicit FilterTabControllerTestApi(FilterTabController& controller)
      : controller_(controller) {}

  FilterTabControllerTestApi(const FilterTabControllerTestApi&) = delete;
  FilterTabControllerTestApi& operator=(const FilterTabControllerTestApi&) =
      delete;

  ~FilterTabControllerTestApi() = default;

  void SetObserverForTest(FilterTabController::ObserverForTest* observer) {
    controller_->observer_for_test_ = observer;
  }

 private:
  const raw_ref<FilterTabController> controller_;
};

inline FilterTabControllerTestApi test_api(FilterTabController& controller) {
  return FilterTabControllerTestApi(controller);
}

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_FILTER_TAB_CONTROLLER_TEST_API_H_
