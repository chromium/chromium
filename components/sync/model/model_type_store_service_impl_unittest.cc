// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/model_type_store_service_impl.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "components/sync/base/model_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

using testing::NotNull;

// Regression test for http://crbug.com/1190187.
TEST(ModelTypeStoreServiceImplTest, ShouldSupportFactoryOutlivingService) {
  base::test::TaskEnvironment task_environment;
  auto service = std::make_unique<ModelTypeStoreServiceImpl>(
      base::CreateUniqueTempDirectoryScopedToTest());

  const RepeatingModelTypeStoreFactory store_factory =
      service->GetStoreFactory();
  ASSERT_TRUE(store_factory);

  // Destroy the service and wait until all backend cleanup work is done.
  service.reset();
  task_environment.RunUntilIdle();

  // Verify that the factory continues to work, even if it outlives the service.
  base::RunLoop loop;
  store_factory.Run(
      syncer::PREFERENCES,
      base::BindLambdaForTesting([&](const absl::optional<ModelError>& error,
                                     std::unique_ptr<ModelTypeStore> store) {
        EXPECT_FALSE(error.has_value());
        EXPECT_THAT(store, NotNull());
        loop.Quit();
      }));
  loop.Run();
}

}  // namespace
}  // namespace syncer
