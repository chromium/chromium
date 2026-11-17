// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/simulated_network_link.h"

#include <stdint.h>

#include <vector>

#include "base/containers/span.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mirroring {
namespace {

class SimulatedNetworkLinkTest : public ::testing::Test {
 protected:
  struct DeliveredPacket {
    std::vector<uint8_t> data;
    base::TimeTicks arrival_time;
  };

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(SimulatedNetworkLinkTest,
       SerializesPacketsAndTailDropsWhenQueueExceedsLimit) {
  SimulatedNetworkLink::Config config;
  config.capacity_bps = 1'000'000;
  config.per_packet_overhead_bytes = 28;
  config.queue_limit_bytes = 2500;
  config.propagation_delay = base::Milliseconds(5);
  config.max_jitter = base::TimeDelta();

  std::vector<DeliveredPacket> delivered;
  const base::TimeTicks start_time = base::TimeTicks::Now();
  SimulatedNetworkLink link(
      config, task_environment_.GetMockTickClock(),
      task_environment_.GetMainThreadTaskRunner(),
      base::BindLambdaForTesting([&](const std::vector<uint8_t>& packet) {
        delivered.push_back({packet, base::TimeTicks::Now()});
      }));

  // Each packet adds 1028 bytes on the wire (1000 payload + 28 overhead). At
  // 1 Mbps that takes 8224 us to serialize. Two fit in the 2500-byte queue
  // (2056 bytes), and the third exceeds it (3084 > 2500) and is tail-dropped.
  const std::vector<uint8_t> packet1(1000, 1);
  const std::vector<uint8_t> packet2(1000, 2);
  const std::vector<uint8_t> packet3(1000, 3);
  link.Send(packet1);
  link.Send(packet2);
  link.Send(packet3);

  EXPECT_EQ(link.stats().packets_sent, 3);
  EXPECT_EQ(link.stats().packets_dropped_by_queue, 1);
  EXPECT_EQ(link.GetQueuedBytes(), 2056u);

  task_environment_.FastForwardBy(base::Milliseconds(50));

  ASSERT_EQ(delivered.size(), 2u);
  EXPECT_EQ(delivered[0].data[0], 1);
  EXPECT_EQ(delivered[1].data[0], 2);
  EXPECT_EQ(delivered[0].arrival_time - start_time, base::Microseconds(13224));
  EXPECT_EQ(delivered[1].arrival_time - start_time, base::Microseconds(21448));
  EXPECT_EQ(link.stats().packets_delivered, 2);
  EXPECT_EQ(link.stats().bytes_delivered, 2000);
  EXPECT_EQ(link.stats().max_queueing_delay, base::Microseconds(8224));
}

TEST_F(SimulatedNetworkLinkTest, JitterNeverReordersPackets) {
  SimulatedNetworkLink::Config config;
  config.capacity_bps = 100'000'000;
  config.propagation_delay = base::Milliseconds(5);
  config.max_jitter = base::Milliseconds(25);
  config.seed = 123;

  std::vector<DeliveredPacket> delivered;
  SimulatedNetworkLink link(
      config, task_environment_.GetMockTickClock(),
      task_environment_.GetMainThreadTaskRunner(),
      base::BindLambdaForTesting([&](const std::vector<uint8_t>& packet) {
        delivered.push_back({packet, base::TimeTicks::Now()});
      }));

  constexpr int kNumPackets = 40;
  for (int i = 0; i < kNumPackets; ++i) {
    const uint8_t payload = static_cast<uint8_t>(i);
    link.Send(base::span_from_ref(payload));
  }

  task_environment_.FastForwardBy(base::Seconds(1));

  ASSERT_EQ(delivered.size(), static_cast<size_t>(kNumPackets));
  for (int i = 0; i < kNumPackets; ++i) {
    EXPECT_EQ(delivered[i].data, std::vector<uint8_t>{static_cast<uint8_t>(i)});
    if (i > 0) {
      EXPECT_GE(delivered[i].arrival_time, delivered[i - 1].arrival_time);
    }
  }
}

TEST_F(SimulatedNetworkLinkTest, DeterministicSeededLossAndDynamicUpdates) {
  SimulatedNetworkLink::Config config;
  config.capacity_bps = 10'000'000;
  config.propagation_delay = base::Milliseconds(2);
  config.max_jitter = base::TimeDelta();
  config.random_loss_rate = 0.05;
  config.loss_bursts_per_second = 10.0;
  config.max_loss_burst_packets = 4;
  config.loss_burst_duration = base::Milliseconds(20);
  config.seed = 7;

  std::vector<uint8_t> delivered_a;
  std::vector<uint8_t> delivered_b;
  SimulatedNetworkLink link_a(
      config, task_environment_.GetMockTickClock(),
      task_environment_.GetMainThreadTaskRunner(),
      base::BindLambdaForTesting([&](const std::vector<uint8_t>& packet) {
        delivered_a.push_back(packet[0]);
      }));
  SimulatedNetworkLink link_b(
      config, task_environment_.GetMockTickClock(),
      task_environment_.GetMainThreadTaskRunner(),
      base::BindLambdaForTesting([&](const std::vector<uint8_t>& packet) {
        delivered_b.push_back(packet[0]);
      }));

  constexpr int kNumPackets = 200;
  for (int i = 0; i < kNumPackets; ++i) {
    const uint8_t payload = static_cast<uint8_t>(i);
    link_a.Send(base::span_from_ref(payload));
    link_b.Send(base::span_from_ref(payload));
    task_environment_.FastForwardBy(base::Milliseconds(5));
  }
  task_environment_.FastForwardBy(base::Milliseconds(50));

  EXPECT_GT(link_a.stats().packets_lost_randomly, 0);
  EXPECT_LT(delivered_a.size(), static_cast<size_t>(kNumPackets));
  EXPECT_EQ(delivered_a, delivered_b);

  // Verify dynamic capacity and random loss updates take effect cleanly.
  SimulatedNetworkLink::Config dynamic_config;
  dynamic_config.capacity_bps = 5'000'000;
  dynamic_config.propagation_delay = base::Milliseconds(2);
  dynamic_config.max_jitter = base::TimeDelta();
  dynamic_config.random_loss_rate = 0.5;
  dynamic_config.seed = 9;

  int dynamic_delivered = 0;
  SimulatedNetworkLink dynamic_link(
      dynamic_config, task_environment_.GetMockTickClock(),
      task_environment_.GetMainThreadTaskRunner(),
      base::BindLambdaForTesting(
          [&](const std::vector<uint8_t>& packet) { ++dynamic_delivered; }));

  for (int i = 0; i < 40; ++i) {
    const uint8_t payload = static_cast<uint8_t>(i);
    dynamic_link.Send(base::span_from_ref(payload));
  }
  task_environment_.FastForwardBy(base::Milliseconds(50));
  EXPECT_GT(dynamic_link.stats().packets_lost_randomly, 0);

  const int64_t losses_before_update =
      dynamic_link.stats().packets_lost_randomly;
  const int delivered_before_update = dynamic_delivered;
  dynamic_link.SetRandomLossRate(0.0);
  dynamic_link.SetCapacityBps(50'000'000);

  for (int i = 0; i < 20; ++i) {
    const uint8_t payload = static_cast<uint8_t>(i);
    dynamic_link.Send(base::span_from_ref(payload));
  }
  task_environment_.FastForwardBy(base::Milliseconds(50));
  EXPECT_EQ(dynamic_link.stats().packets_lost_randomly, losses_before_update);
  EXPECT_EQ(dynamic_delivered - delivered_before_update, 20);
}

}  // namespace
}  // namespace mirroring
