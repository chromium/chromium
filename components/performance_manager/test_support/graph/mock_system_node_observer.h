// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_GRAPH_MOCK_SYSTEM_NODE_OBSERVER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_GRAPH_MOCK_SYSTEM_NODE_OBSERVER_H_

#include "components/performance_manager/public/graph/system_node.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace performance_manager {

class LenientMockSystemNodeObserver : public SystemNodeObserver {
 public:
  LenientMockSystemNodeObserver();
  ~LenientMockSystemNodeObserver() override;

  MOCK_METHOD(void,
              OnProcessMemoryMetricsAvailable,
              (const SystemNode*),
              (override));
  MOCK_METHOD(void,
              OnMemoryPressure,
              (base::MemoryPressureListener::MemoryPressureLevel),
              (override));
  MOCK_METHOD(void,
              OnBeforeMemoryPressure,
              (base::MemoryPressureListener::MemoryPressureLevel),
              (override));
};

using MockSystemNodeObserver =
    ::testing::StrictMock<LenientMockSystemNodeObserver>;

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_GRAPH_MOCK_SYSTEM_NODE_OBSERVER_H_
