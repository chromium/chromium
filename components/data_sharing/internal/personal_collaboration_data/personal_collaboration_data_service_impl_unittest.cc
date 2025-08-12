// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/personal_collaboration_data/personal_collaboration_data_service_impl.h"

#include <memory>

#include "base/test/task_environment.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_sharing::personal_collaboration_data {

class PersonalCollaborationDataServiceImplTest : public testing::Test {
 public:
  PersonalCollaborationDataServiceImplTest()
      : data_type_store_(
            syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest()) {}

  void SetUp() override {
    service_ = std::make_unique<PersonalCollaborationDataServiceImpl>(
        mock_processor_.CreateForwardingProcessor(),
        syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(
            data_type_store_.get()));
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<syncer::DataTypeStore> data_type_store_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
  std::unique_ptr<PersonalCollaborationDataServiceImpl> service_;
};

TEST_F(PersonalCollaborationDataServiceImplTest, TestServiceConstruction) {
  EXPECT_FALSE(service_->IsInitialized());
}

}  // namespace data_sharing::personal_collaboration_data
