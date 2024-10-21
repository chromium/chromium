// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/collaboration_service_impl.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace collaboration {

class CollaborationServiceImplTest : public testing::Test {
 public:
  CollaborationServiceImplTest() = default;

  ~CollaborationServiceImplTest() override = default;
};

TEST_F(CollaborationServiceImplTest, ConstructionAndEmptyServiceCheck) {
  auto service = std::make_unique<CollaborationServiceImpl>(
      /*tab_group_sync_service=*/nullptr,
      /*data_sharing_service=*/nullptr,
      /*identity_manager=*/nullptr,
      /*sync_service=*/nullptr);
  EXPECT_FALSE(service->IsEmptyService());
}

}  // namespace collaboration
