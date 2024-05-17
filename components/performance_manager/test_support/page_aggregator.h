// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_PAGE_AGGREGATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_PAGE_AGGREGATOR_H_

#include "components/performance_manager/public/decorators/page_live_state_decorator.h"

namespace performance_manager {
namespace testing {

void CreatePageAggregatorAndPassItToGraph();

}  // namespace testing
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_PAGE_AGGREGATOR_H_
