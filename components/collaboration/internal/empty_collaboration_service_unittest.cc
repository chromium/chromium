// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/empty_collaboration_service.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace collaboration {

class EmptyCollaborationServiceTest : public testing::Test {
 public:
  EmptyCollaborationServiceTest() = default;

  ~EmptyCollaborationServiceTest() override = default;
};

TEST_F(EmptyCollaborationServiceTest, ConstructionAndEmptyServiceCheck) {
  auto service = std::make_unique<EmptyCollaborationService>();
  EXPECT_TRUE(service->IsEmptyService());
}

}  // namespace collaboration
