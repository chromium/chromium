// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notebooks/internal/notebooks_service_impl.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notebooks {

class NotebooksServiceImplTest : public testing::Test {
 public:
  NotebooksServiceImplTest() = default;

  ~NotebooksServiceImplTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(NotebooksServiceImplTest, ConstructionAndInitialization) {
  auto processor = std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
      syncer::NOTEBOOK, base::DoNothing());
  auto* processor_ptr = processor.get();
  auto service = std::make_unique<NotebooksServiceImpl>(
      std::move(processor),
      syncer::DataTypeStoreTestUtil::FactoryForInMemoryStoreForTest());
  EXPECT_FALSE(service->IsEmptyForTesting());
  EXPECT_TRUE(service->GetSyncControllerDelegate());
  EXPECT_TRUE(base::test::RunUntil(
      [processor_ptr] { return processor_ptr->IsModelReadyToSyncForTest(); }));
}

}  // namespace notebooks
