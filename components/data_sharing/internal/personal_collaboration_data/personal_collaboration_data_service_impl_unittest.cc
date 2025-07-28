// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/personal_collaboration_data/personal_collaboration_data_service_impl.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace data_sharing::personal_collaboration_data {

class PersonalCollaborationDataServiceImplTest : public testing::Test {
 public:
  void SetUp() override {
    service_ = std::make_unique<PersonalCollaborationDataServiceImpl>();
  }

 protected:
  std::unique_ptr<PersonalCollaborationDataServiceImpl> service_;
};

TEST_F(PersonalCollaborationDataServiceImplTest, TestServiceConstruction) {
  EXPECT_FALSE(service_->IsInitialized());
}

}  // namespace data_sharing::personal_collaboration_data
