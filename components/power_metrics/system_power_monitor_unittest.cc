// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_metrics/system_power_monitor.h"

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace power_metrics {

class FakeProvider : public EnergyMetricsProvider {
 public:
  void set_metrics(EnergyMetrics metrics) { metrics_ = metrics; }

  std::optional<EnergyMetrics> CaptureMetrics() override { return metrics_; }

 private:
  std::optional<EnergyMetrics> metrics_;
};

class FakeDelegate : public SystemPowerMonitorDelegate {
 public:
  void set_trace_category_enabled(bool enabled) {
    trace_category_enabled_ = enabled;
  }

  void RecordSystemPower(const char* category,
                         base::TimeTicks timestamp,
                         int64_t power) override {
    timestamp_ = timestamp;
    if (strcmp(category, "Package Power (mW)") == 0) {
      system_power_.package_nanojoules = static_cast<uint64_t>(power);
    } else if (strcmp(category, "CPU Power (mW)") == 0) {
      system_power_.cpu_nanojoules = static_cast<uint64_t>(power);
    } else if (strcmp(category, "iGPU Power (mW)") == 0) {
      system_power_.gpu_nanojoules = static_cast<uint64_t>(power);
    } else if (strcmp(category, "DRAM Power (mW)") == 0) {
      system_power_.dram_nanojoules = static_cast<uint64_t>(power);
    } else if (strcmp(category, "Psys Power (mW)") == 0) {
      system_power_.psys_nanojoules = static_cast<uint64_t>(power);
    } else if (strcmp(category, "VDDCR VDD (mW)") == 0) {
      system_power_.vdd_nanojoules = static_cast<uint64_t>(power);
    } else if (strcmp(category, "VDDCR SOC (mW)") == 0) {
      system_power_.soc_nanojoules = static_cast<uint64_t>(power);
    } else if (strcmp(category, "Current Socket (mW)") == 0) {
      system_power_.socket_nanojoules = static_cast<uint64_t>(power);
    } else if (strcmp(category, "APU Power (mW)") == 0) {
      system_power_.apu_nanojoules = static_cast<uint64_t>(power);
    }
  }

  bool IsTraceCategoryEnabled() const override {
    return trace_category_enabled_;
  }

  EnergyMetricsProvider::EnergyMetrics& SystemPower() { return system_power_; }

  base::TimeTicks timestamp() { return timestamp_; }

 private:
  // We use EnergyMetrics to save recorded power data in milliwatts for
  // simplicity.
  EnergyMetricsProvider::EnergyMetrics system_power_;
  base::TimeTicks timestamp_;
  bool trace_category_enabled_{true};
};

class SystemPowerMonitorHelperTest : public testing::Test {
 public:
  SystemPowerMonitorHelperTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    auto provider = std::make_unique<FakeProvider>();
    provider_ = provider.get();
    auto delegate = std::make_unique<FakeDelegate>();
    delegate_ = delegate.get();
    helper_ = std::make_unique<SystemPowerMonitorHelper>(std::move(provider),
                                                         std::move(delegate));
  }

  void TearDown() override { helper_.reset(); }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }
  SystemPowerMonitorHelper* helper() { return helper_.get(); }
  FakeDelegate* delegate() { return delegate_.get(); }
  FakeProvider* provider() { return provider_.get(); }

 protected:
  base::test::TaskEnvironment task_environment_;

 private:
  std::unique_ptr<SystemPowerMonitorHelper> helper_;
  raw_ptr<FakeDelegate, DanglingUntriaged> delegate_;
  raw_ptr<FakeProvider, DanglingUntriaged> provider_;
};

class SystemPowerMonitorTest : public testing::Test {
 public:
  SystemPowerMonitorTest() : task_environment_() {}

  void SetUp() override {
    auto provider = std::make_unique<FakeProvider>();

    // Assign a valid metric to provider, so the timer can start successfully.
    provider->set_metrics({1llu});
    monitor_.reset(new SystemPowerMonitor(std::move(provider),
                                          std::make_unique<FakeDelegate>()));
  }

  void TearDown() override { monitor_.reset(); }

  SystemPowerMonitor* monitor() { return monitor_.get(); }
  base::SequenceBound<SystemPowerMonitorHelper>* helper() {
    return monitor_->GetHelperForTesting();
  }

 protected:
  base::test::TaskEnvironment task_environment_;

 private:
  std::unique_ptr<SystemPowerMonitor> monitor_;
};

TEST_F(SystemPowerMonitorHelperTest, MonitorHelperStartStop) {
  provider()->set_metrics({1llu});

  helper()->Start();
  ASSERT_TRUE(helper()->IsTimerRunningForTesting());
  helper()->Stop();
  ASSERT_FALSE(helper()->IsTimerRunningForTesting());
  helper()->Start();
  ASSERT_TRUE(helper()->IsTimerRunningForTesting());
  helper()->Stop();
  ASSERT_FALSE(helper()->IsTimerRunningForTesting());
}

TEST_F(SystemPowerMonitorHelperTest, TimerStartFailed_InvalidSample) {
  // We haven't set metrics for provider, so monitor gets an
  // std::nullopt sample at the beginning and it will not start.
  helper()->Start();
  ASSERT_FALSE(helper()->IsTimerRunningForTesting());
}

TEST_F(SystemPowerMonitorHelperTest, TimerStartFailed_MetricsAllZero) {
  // If the metrics are all 0, we determine that there is no valid metric
  // provided, so monitor will not start.
  provider()->set_metrics({});
  helper()->Start();
  ASSERT_FALSE(helper()->IsTimerRunningForTesting());
}

TEST_F(SystemPowerMonitorHelperTest, TraceCategoryEnableDisable) {
  provider()->set_metrics({1llu});

  delegate()->set_trace_category_enabled(false);
  ASSERT_FALSE(delegate()->IsTraceCategoryEnabled());
  helper()->Start();
  ASSERT_FALSE(helper()->IsTimerRunningForTesting());

  delegate()->set_trace_category_enabled(true);
  ASSERT_TRUE(delegate()->IsTraceCategoryEnabled());
  helper()->Start();
  ASSERT_TRUE(helper()->IsTimerRunningForTesting());
}

TEST_F(SystemPowerMonitorHelperTest, TestSample) {
  EnergyMetricsProvider::EnergyMetrics sample1 = {
      100000llu, 100000llu, 100000llu, 100000llu, 100000llu,
      100000llu, 100000llu, 100000llu, 100000llu};
  EnergyMetricsProvider::EnergyMetrics sample2 = {
      200000llu, 300000llu, 400000llu, 500000llu, 600000llu,
      700000llu, 800000llu, 900000llu, 1000000llu};

  provider()->set_metrics(sample1);
  helper()->Start();
  ASSERT_TRUE(helper()->IsTimerRunningForTesting());

  provider()->set_metrics(sample2);
  task_environment().FastForwardBy(
      SystemPowerMonitorHelper::kDefaultSampleInterval);
  auto power = delegate()->SystemPower();
  EXPECT_EQ(delegate()->timestamp() +
                SystemPowerMonitorHelper::kDefaultSampleInterval,
            task_environment().NowTicks());
  EXPECT_EQ(
      power.package_nanojoules,
      (sample2.package_nanojoules - sample1.package_nanojoules) /
          SystemPowerMonitorHelper::kDefaultSampleInterval.InMicroseconds());
  EXPECT_EQ(
      power.cpu_nanojoules,
      (sample2.cpu_nanojoules - sample1.cpu_nanojoules) /
          SystemPowerMonitorHelper::kDefaultSampleInterval.InMicroseconds());
  EXPECT_EQ(
      power.gpu_nanojoules,
      (sample2.gpu_nanojoules - sample1.gpu_nanojoules) /
          SystemPowerMonitorHelper::kDefaultSampleInterval.InMicroseconds());
  EXPECT_EQ(
      power.dram_nanojoules,
      (sample2.dram_nanojoules - sample1.dram_nanojoules) /
          SystemPowerMonitorHelper::kDefaultSampleInterval.InMicroseconds());
  EXPECT_EQ(
      power.psys_nanojoules,
      (sample2.psys_nanojoules - sample1.psys_nanojoules) /
          SystemPowerMonitorHelper::kDefaultSampleInterval.InMicroseconds());
  EXPECT_EQ(
      power.vdd_nanojoules,
      (sample2.vdd_nanojoules - sample1.vdd_nanojoules) /
          SystemPowerMonitorHelper::kDefaultSampleInterval.InMicroseconds());
  EXPECT_EQ(
      power.soc_nanojoules,
      (sample2.soc_nanojoules - sample1.soc_nanojoules) /
          SystemPowerMonitorHelper::kDefaultSampleInterval.InMicroseconds());
  EXPECT_EQ(
      power.socket_nanojoules,
      (sample2.socket_nanojoules - sample1.socket_nanojoules) /
          SystemPowerMonitorHelper::kDefaultSampleInterval.InMicroseconds());
  EXPECT_EQ(
      power.apu_nanojoules,
      (sample2.apu_nanojoules - sample1.apu_nanojoules) /
          SystemPowerMonitorHelper::kDefaultSampleInterval.InMicroseconds());
}

TEST_F(SystemPowerMonitorTest, TraceLogEnableDisable) {
  ASSERT_NE(helper(), nullptr);

  base::test::TestFuture<bool> future_enable;
  monitor()->OnTraceLogEnabled();
  helper()
      ->AsyncCall(&SystemPowerMonitorHelper::IsTimerRunningForTesting)
      .Then(base::BindOnce(
          [](base::OnceCallback<void(bool)> callback, bool is_running) {
            std::move(callback).Run(is_running);
          },
          future_enable.GetCallback()));
  EXPECT_TRUE(future_enable.Get());

  base::test::TestFuture<bool> future_disable;
  monitor()->OnTraceLogDisabled();
  helper()
      ->AsyncCall(&SystemPowerMonitorHelper::IsTimerRunningForTesting)
      .Then(base::BindOnce(
          [](base::OnceCallback<void(bool)> callback, bool is_running) {
            std::move(callback).Run(is_running);
          },
          future_disable.GetCallback()));
  EXPECT_FALSE(future_disable.Get());
}

}  // namespace power_metrics
