// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/local_search_service/local_search_service_provider_for_testing.h"

#include <memory>

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::local_search_service {

class LocalSearchServiceProviderTest : public testing::Test {
 public:
  void SetUp() override {
    provider_ = std::make_unique<LocalSearchServiceProviderForTesting>();
  }

 protected:
  std::unique_ptr<LocalSearchServiceProvider> provider_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::DEFAULT,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
};

TEST_F(LocalSearchServiceProviderTest, SetUpAndRun) {
  auto* provider = LocalSearchServiceProvider::Get();
  mojo::Remote<mojom::LocalSearchService> service;
  provider->BindLocalSearchService(service.BindNewPipeAndPassReceiver());
  mojo::Remote<mojom::Index> index_remote;

  // BindIndex
  bool callback_done = false;
  std::string error = "";
  service->BindIndex(IndexId::kCrosSettings, Backend::kLinearMap,
                     index_remote.BindNewPipeAndPassReceiver(),
                     mojo::NullRemote(),
                     base::BindOnce(
                         [](bool* callback_done, std::string* error,
                            const std::optional<std::string>& error_callback) {
                           *callback_done = true;
                           if (error_callback)
                             *error = error_callback.value();
                         },
                         &callback_done, &error));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(callback_done);
  EXPECT_EQ(error, "");

  // GetSize
  callback_done = false;
  uint32_t num_items = 0;
  index_remote->GetSize(base::BindOnce(
      [](bool* callback_done, uint32_t* num_items, uint64_t size) {
        *callback_done = true;
        *num_items = size;
      },
      &callback_done, &num_items));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(callback_done);
  EXPECT_EQ(num_items, 0u);
}

}  // namespace ash::local_search_service
