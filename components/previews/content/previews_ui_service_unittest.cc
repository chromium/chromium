// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/previews_ui_service.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/time/default_clock.h"
#include "components/blocklist/opt_out_blocklist/opt_out_blocklist_data.h"
#include "components/previews/content/previews_decider_impl.h"
#include "components/previews/core/previews_block_list.h"
#include "components/previews/core/previews_experiments.h"
#include "services/network/test/test_network_quality_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace previews {

namespace {

// Dummy method for creating TestPreviewsUIService.
bool MockedPreviewsIsEnabled(previews::PreviewsType type) {
  return true;
}

class TestPreviewsUIService : public PreviewsUIService {
 public:
  TestPreviewsUIService(
      std::unique_ptr<PreviewsDeciderImpl> previews_decider_impl,
      std::unique_ptr<blocklist::OptOutStore> previews_opt_out_store,
      std::unique_ptr<PreviewsOptimizationGuide> previews_opt_guide,
      network::TestNetworkQualityTracker* test_network_quality_tracker)
      : PreviewsUIService(std::move(previews_decider_impl),
                          std::move(previews_opt_out_store),
                          std::move(previews_opt_guide),
                          base::BindRepeating(&MockedPreviewsIsEnabled),
                          blocklist::BlocklistData::AllowedTypesAndVersions(),
                          test_network_quality_tracker) {}
  ~TestPreviewsUIService() override {}
};

class TestPreviewsDeciderImpl : public PreviewsDeciderImpl {
 public:
  TestPreviewsDeciderImpl()
      : PreviewsDeciderImpl(base::DefaultClock::GetInstance()) {}

  // PreviewsDeciderImpl:
  void SetIgnorePreviewsBlocklistDecision(bool ignored) override {
    blocklist_ignored_ = ignored;
  }

  // Exposed the status of blocklist decisions ignored for testing
  // PreviewsUIService.
  bool blocklist_ignored() const { return blocklist_ignored_; }

 private:
  // Whether the blocklist decisions are ignored or not.
  bool blocklist_ignored_ = false;
};

class PreviewsUIServiceTest : public testing::Test {
 public:
  PreviewsUIServiceTest() {}

  ~PreviewsUIServiceTest() override {}

  void SetUp() override {
    std::unique_ptr<TestPreviewsDeciderImpl> previews_decider_impl =
        std::make_unique<TestPreviewsDeciderImpl>();
    previews_decider_impl_ = previews_decider_impl.get();

    ui_service_ = std::make_unique<TestPreviewsUIService>(
        std::move(previews_decider_impl), nullptr /* previews_opt_out_store */,
        nullptr /* previews_opt_guide */, &test_network_quality_tracker_);
  }

  TestPreviewsDeciderImpl* previews_decider_impl() {
    return previews_decider_impl_;
  }

  TestPreviewsUIService* ui_service() { return ui_service_.get(); }

 protected:
  // Run this test on a single thread.
  base::test::SingleThreadTaskEnvironment task_environment_;
  network::TestNetworkQualityTracker test_network_quality_tracker_;

 private:
  TestPreviewsDeciderImpl* previews_decider_impl_;
  std::unique_ptr<TestPreviewsUIService> ui_service_;
};

}  // namespace

TEST_F(PreviewsUIServiceTest, TestInitialization) {
  // After the outstanding posted tasks have run, SetIOData should have been
  // called for |ui_service_|.
  EXPECT_TRUE(ui_service()->previews_decider_impl());
}

TEST_F(PreviewsUIServiceTest,
       TestSetIgnorePreviewsBlocklistDecisionPassesCorrectParams) {
  ui_service()->SetIgnorePreviewsBlocklistDecision(true /* ignored */);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(previews_decider_impl()->blocklist_ignored());

  ui_service()->SetIgnorePreviewsBlocklistDecision(false /* ignored */);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(previews_decider_impl()->blocklist_ignored());
}

}  // namespace previews
