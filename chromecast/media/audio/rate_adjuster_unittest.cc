// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/rate_adjuster.h"
#include "base/rand_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

class RateAdjusterTest : public testing::Test {
 public:
  RateAdjusterTest() = default;
  ~RateAdjusterTest() override = default;

  double clock_rate() const { return clock_rate_; }
  int64_t rate_change_count() const { return rate_change_count_; }

  void set_underlying_rate(double rate) { underlying_rate_ = rate; }

  void Configure(const RateAdjuster::Config& config) {
    rate_adjuster_ = std::make_unique<RateAdjuster>(
        config,
        base::BindRepeating(&RateAdjusterTest::ChangeRate,
                            base::Unretained(this)),
        clock_rate_);
  }

  void AddSample(int64_t timestamp, int max_jitter) {
    int64_t delta = timestamp - base_timestamp_;
    // Clock rate < 1 causes the events to happen later than they should.
    int64_t value = base_value_ + delta / (clock_rate_ * underlying_rate_);
    last_timestamp_ = timestamp;
    last_value_ = value;

    int64_t jittered_value = value + base::RandInt(-max_jitter, max_jitter);

    rate_adjuster_->AddError(jittered_value - timestamp, timestamp);
  }

 private:
  double ChangeRate(double desired_clock_rate,
                    double error_slope,
                    double current_error) {
    ++rate_change_count_;
    clock_rate_ = desired_clock_rate;
    base_timestamp_ = last_timestamp_;
    base_value_ = last_value_;
    return clock_rate_;
  }

  int64_t base_timestamp_ = 0;
  int64_t last_timestamp_ = 0;

  int64_t base_value_ = 0;
  int64_t last_value_ = 0;

  double clock_rate_ = 1.0;
  double underlying_rate_ = 1.0;

  int64_t rate_change_count_ = 0;

  std::unique_ptr<RateAdjuster> rate_adjuster_;
};

TEST_F(RateAdjusterTest, NoError) {
  RateAdjuster::Config config;
  config.max_ignored_current_error = 0;
  config.min_rate_change = 0;
  Configure(config);

  for (int i = 0; i < 600; ++i) {
    AddSample(i * 100000, 0);
  }

  EXPECT_EQ(clock_rate(), 1.0);
  EXPECT_EQ(rate_change_count(), 0);
}

TEST_F(RateAdjusterTest, MaxInterval) {
  RateAdjuster::Config config;
  config.max_ignored_current_error = 0;
  config.min_rate_change = 0;
  Configure(config);

  for (int i = 0; i < 302; ++i) {
    AddSample(i * 1000000, 0);
  }

  EXPECT_EQ(clock_rate(), 1.0);
  EXPECT_EQ(rate_change_count(), 1);
}

TEST_F(RateAdjusterTest, SlowUnderlying) {
  constexpr double kUnderlyingRate = 0.9995;
  set_underlying_rate(kUnderlyingRate);

  RateAdjuster::Config config;
  config.max_ignored_current_error = 0;
  config.min_rate_change = 0;
  Configure(config);

  for (int i = 0; i < 600; ++i) {
    AddSample(i * 20000, 0);
  }

  EXPECT_NEAR(clock_rate() * kUnderlyingRate, 1.0, 1e-5);
  EXPECT_GT(rate_change_count(), 1);
}

TEST_F(RateAdjusterTest, FastUnderlyingWithJitter) {
  constexpr double kUnderlyingRate = 1.0005;
  set_underlying_rate(kUnderlyingRate);

  RateAdjuster::Config config;
  config.max_ignored_current_error = 0;
  config.min_rate_change = 0;
  Configure(config);

  // Need more samples to converge.
  for (int i = 0; i < 3000; ++i) {
    AddSample(i * 20000, 200);
  }

  EXPECT_NEAR(clock_rate() * kUnderlyingRate, 1.0, 5e-5);
  EXPECT_GT(rate_change_count(), 1);
}

}  // namespace media
}  // namespace chromecast
