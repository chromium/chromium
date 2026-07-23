// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/debug/runtime_mutable_features_handler_base.h"

#include <string>
#include <vector>

#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/service/test_variations_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {
namespace {

class MockDelegate : public RuntimeMutableFeaturesHandlerBase::Delegate {
 public:
  void ResolvePageCallback(const base::ValueView callback_id,
                           const base::ValueView response) override {
    is_callback_resolved_ = true;
  }

  bool is_callback_resolved_ = false;
};

class MockVariationsService : public variations::TestVariationsService {
 public:
  MockVariationsService(PrefService* prefs,
                        metrics::MetricsStateManager* state_manager)
      : variations::TestVariationsService(prefs, state_manager) {}

  void SimulateAndApplyRuntimeMutableChanges(
      const variations::VariationsSeed& seed) override {
    called_simulate_ = true;
  }

  bool called_simulate_ = false;
};

class RuntimeMutableFeaturesHandlerBaseTest : public testing::Test {
 protected:
  RuntimeMutableFeaturesHandlerBaseTest() {
    variations::TestVariationsService::RegisterPrefs(prefs_.registry());

    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        &prefs_, &enabled_state_provider_, std::wstring(), base::FilePath());
    variations_service_ = std::make_unique<MockVariationsService>(
        &prefs_, metrics_state_manager_.get());
    handler_ = std::make_unique<RuntimeMutableFeaturesHandlerBase>(
        &delegate_, variations_service_.get());
  }

  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple prefs_;
  metrics::TestEnabledStateProvider enabled_state_provider_{false, false};
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  MockDelegate delegate_;
  std::unique_ptr<MockVariationsService> variations_service_;
  std::unique_ptr<RuntimeMutableFeaturesHandlerBase> handler_;
};

TEST_F(RuntimeMutableFeaturesHandlerBaseTest, UploadSeed) {
  variations::VariationsSeed seed;
  seed.set_serial_number("123");
  std::string seed_str;
  seed.SerializeToString(&seed_str);

  std::vector<uint8_t> seed_bytes(seed_str.begin(), seed_str.end());

  base::Value seed_value(seed_bytes);
  base::Value callback_id("callback_id");

  handler_->HandleUploadSeed(callback_id, seed_value);

  EXPECT_TRUE(delegate_.is_callback_resolved_);
  EXPECT_TRUE(variations_service_->called_simulate_);
}

}  // namespace
}  // namespace metrics
