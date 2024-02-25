// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/empty_data_sharing_service.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace data_sharing {

class EmptyDataSharingServiceTest : public testing::Test {
 public:
  EmptyDataSharingServiceTest() = default;

  ~EmptyDataSharingServiceTest() override = default;
};

TEST_F(EmptyDataSharingServiceTest, ConstructionAndEmptyServiceCheck) {
  auto service = std::make_unique<EmptyDataSharingService>();
  EXPECT_TRUE(service->IsEmptyService());
}

}  // namespace data_sharing
