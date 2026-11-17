// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/simulated_network_link.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/time/tick_clock.h"

namespace mirroring {

SimulatedNetworkLink::Config::Config() = default;
SimulatedNetworkLink::Config::Config(const Config&) = default;
SimulatedNetworkLink::Config& SimulatedNetworkLink::Config::operator=(
    const Config&) = default;
SimulatedNetworkLink::Config::~Config() = default;

SimulatedNetworkLink::SimulatedNetworkLink(
    const Config& config,
    const base::TickClock* clock,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    DeliverCallback deliver_cb)
    : config_(config),
      clock_(clock),
      task_runner_(std::move(task_runner)),
      deliver_cb_(std::move(deliver_cb)),
      rng_state_(config.seed % 2147483647u == 0 ? 1u
                                                : config.seed % 2147483647u) {
  CHECK(clock_);
  CHECK(task_runner_);
  CHECK(deliver_cb_);
  CHECK_GT(config_.capacity_bps, 0);
  CHECK_GE(config_.random_loss_rate, 0.0);
  CHECK_LT(config_.random_loss_rate, 1.0);
  CHECK_GE(config_.loss_bursts_per_second, 0.0);
  CHECK_GE(config_.max_loss_burst_packets, 1);
  CHECK(!config_.max_jitter.is_negative());
  CHECK(!config_.propagation_delay.is_negative());

  const base::TimeTicks now = clock_->NowTicks();
  busy_until_ = now;
  last_arrival_time_ = now;
  ScheduleNextLossBurst(now);
}

SimulatedNetworkLink::~SimulatedNetworkLink() = default;

void SimulatedNetworkLink::Send(base::span<const uint8_t> packet) {
  const base::TimeTicks now = clock_->NowTicks();
  const size_t wire_size = packet.size() + config_.per_packet_overhead_bytes;
  ++stats_.packets_sent;
  stats_.bytes_sent += packet.size();

  // Tail drop if the queue cannot hold the packet.
  busy_until_ = std::max(busy_until_, now);
  if (GetQueuedBytes() + wire_size > config_.queue_limit_bytes) {
    ++stats_.packets_dropped_by_queue;
    return;
  }

  const base::TimeDelta queueing_delay = busy_until_ - now;
  stats_.max_queueing_delay =
      std::max(stats_.max_queueing_delay, queueing_delay);

  // Serialize.
  const base::TimeDelta serialization_time = base::Microseconds(
      static_cast<int64_t>(wire_size) * 8 * base::Time::kMicrosecondsPerSecond /
      config_.capacity_bps);
  busy_until_ += serialization_time;

  // Lost packets still occupy the link.
  if (ShouldLosePacket(now)) {
    ++stats_.packets_lost_randomly;
    return;
  }

  const base::TimeDelta jitter = config_.max_jitter * NextUniform();
  const base::TimeTicks arrival_time = std::max(
      busy_until_ + config_.propagation_delay + jitter, last_arrival_time_);
  last_arrival_time_ = arrival_time;

  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SimulatedNetworkLink::Deliver, weak_factory_.GetWeakPtr(),
                     std::vector<uint8_t>(packet.begin(), packet.end())),
      arrival_time - now);
}

void SimulatedNetworkLink::SetCapacityBps(int64_t capacity_bps) {
  CHECK_GT(capacity_bps, 0);
  config_.capacity_bps = capacity_bps;
}

void SimulatedNetworkLink::SetRandomLossRate(double random_loss_rate) {
  CHECK_GE(random_loss_rate, 0.0);
  CHECK_LT(random_loss_rate, 1.0);
  config_.random_loss_rate = random_loss_rate;
}

size_t SimulatedNetworkLink::GetQueuedBytes() const {
  const base::TimeDelta remaining = busy_until_ - clock_->NowTicks();
  if (!remaining.is_positive()) {
    return 0;
  }
  return static_cast<size_t>(remaining.InMicroseconds() * config_.capacity_bps /
                             (8 * base::Time::kMicrosecondsPerSecond));
}

double SimulatedNetworkLink::NextUniform() {
  constexpr uint32_t kModulus = 2147483647u;
  constexpr uint64_t kMultiplier = 48271u;
  rng_state_ =
      static_cast<uint32_t>((uint64_t{rng_state_} * kMultiplier) % kModulus);
  return static_cast<double>(rng_state_ - 1u) /
         static_cast<double>(kModulus - 1u);
}

bool SimulatedNetworkLink::ShouldLosePacket(base::TimeTicks now) {
  if (config_.loss_bursts_per_second > 0) {
    // Skip bursts that ended before this packet was sent.
    while (next_loss_burst_time_ + config_.loss_burst_duration <= now) {
      ScheduleNextLossBurst(next_loss_burst_time_);
    }
    if (now >= next_loss_burst_time_) {
      remaining_burst_losses_ =
          1 + static_cast<int>(NextUniform() * config_.max_loss_burst_packets);
      burst_end_time_ = next_loss_burst_time_ + config_.loss_burst_duration;
      ScheduleNextLossBurst(next_loss_burst_time_);
    }
  }
  bool lose = false;
  if (remaining_burst_losses_ > 0 && now < burst_end_time_) {
    lose = true;
  } else if (config_.random_loss_rate > 0) {
    lose = NextUniform() < config_.random_loss_rate;
  }
  if (!lose) {
    return false;
  }
  if (remaining_burst_losses_ > 0 && now < burst_end_time_) {
    --remaining_burst_losses_;
  }
  return true;
}

void SimulatedNetworkLink::ScheduleNextLossBurst(base::TimeTicks after) {
  if (config_.loss_bursts_per_second <= 0) {
    return;
  }
  // Exponentially distributed inter-arrival time.
  const double seconds =
      -std::log(1.0 - NextUniform()) / config_.loss_bursts_per_second;
  next_loss_burst_time_ = after + base::Seconds(seconds);
}

void SimulatedNetworkLink::Deliver(std::vector<uint8_t> packet) {
  ++stats_.packets_delivered;
  stats_.bytes_delivered += packet.size();
  deliver_cb_.Run(packet);
}

}  // namespace mirroring
