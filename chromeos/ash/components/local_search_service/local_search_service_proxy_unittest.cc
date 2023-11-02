// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_proxy.h"

#include <memory>

#include "base/test/task_environment.h"
#include "chromeos/ash/components/local_search_service/local_search_service_provider_for_testing.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::local_search_service {

class LocalSearchServiceProxyTest : public testing::Test {
 public:
  void SetUp() override {
    service_proxy_ =
        std::make_unique<LocalSearchServiceProxy>(/*for_testing=*/true);
  }

 protected:
  void CheckReporter(bool is_null_expected) {
    bool is_null = false;
    if (!service_proxy_->reporter_)
      is_null = true;
    ASSERT_EQ(is_null, is_null_expected);
  }

  void IndexGetSizeAndCheckResults(mojo::Remote<mojom::Index>* index_remote,
                                   uint32_t expected_num_items) {
    bool callback_done = false;
    uint32_t num_items = 0;
    (*index_remote)
        ->GetSize(base::BindOnce(
            [](bool* callback_done, uint32_t* num_items, uint64_t size) {
              *callback_done = true;
              *num_items = size;
            },
            &callback_done, &num_items));
    task_environment_.RunUntilIdle();
    ASSERT_TRUE(callback_done);
    EXPECT_EQ(num_items, expected_num_items);
  }

  TestingPrefServiceSimple pref_service_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::DEFAULT,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};

  std::unique_ptr<LocalSearchServiceProxy> service_proxy_;
  std::unique_ptr<LocalSearchServiceProvider> provider_;
};

TEST_F(LocalSearchServiceProxyTest, TestWithLocalState) {
  mojo::Remote<mojom::Index> index_remote;
  SearchMetricsReporter::RegisterLocalStatePrefs(pref_service_.registry());
  service_proxy_->SetLocalState(&pref_service_);
  service_proxy_->GetIndex(IndexId::kCrosSettings, Backend::kLinearMap,
                           index_remote.BindNewPipeAndPassReceiver());
  task_environment_.RunUntilIdle();

  // Check that index_remote is bound.
  IndexGetSizeAndCheckResults(&index_remote, 0u);

  // Check reporter
  CheckReporter(/*is_null_expected*/ false);
}

TEST_F(LocalSearchServiceProxyTest, TestWithoutLocalState) {
  mojo::Remote<mojom::Index> index_remote;
  service_proxy_->GetIndex(IndexId::kCrosSettings, Backend::kLinearMap,
                           index_remote.BindNewPipeAndPassReceiver());
  task_environment_.RunUntilIdle();

  // Check that index_remote is bound.
  IndexGetSizeAndCheckResults(&index_remote, 0u);

  // Check reporter
  CheckReporter(/*is_null_expected*/ true);
}

}  // namespace ash::local_search_service
