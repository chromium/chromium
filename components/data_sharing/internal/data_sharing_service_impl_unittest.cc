// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/data_sharing_service_impl.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace data_sharing {

class DataSharingServiceImplTest : public testing::Test {
 public:
  DataSharingServiceImplTest() = default;

  ~DataSharingServiceImplTest() override = default;
};

TEST_F(DataSharingServiceImplTest, ConstructionAndEmptyServiceCheck) {
  auto service = std::make_unique<DataSharingServiceImpl>();
  EXPECT_FALSE(service->IsEmptyService());
}

}  // namespace data_sharing
