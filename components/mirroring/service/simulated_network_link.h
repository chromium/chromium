// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_SIMULATED_NETWORK_LINK_H_
#define COMPONENTS_MIRRORING_SERVICE_SIMULATED_NETWORK_LINK_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"

namespace base {
class TickClock;
}  // namespace base

namespace mirroring {

// A deterministic, test-only model of one direction of a network link, used to
// run Cast Streaming end-to-end tests over a realistic network on mock time.
//
// The model is a single FIFO bottleneck:
//   * Packets are serialized at `capacity_bps` (the link is "busy until" the
//     previous packet has finished transmitting).
//   * Packets that would make the queue exceed `queue_limit_bytes` are tail
//     dropped.
//   * After serialization, each packet is delayed by `propagation_delay` plus
//     a uniformly distributed jitter in [0, `max_jitter`]. Packets are never
//     reordered.
//   * Packets may be lost at random, either independently (`random_loss_rate`)
//     or in short bursts (`loss_bursts_per_second`, each burst dropping between
//     1 and `max_loss_burst_packets` consecutive packets sent within
//     `loss_burst_duration`), which resembles the occasional retry exhaustion
//     seen on otherwise healthy Wi-Fi links. Lost packets still consume link
//     capacity.
//
// All randomness comes from a seeded Lehmer generator, so a given seed and
// input sequence always produce the same output. Delivery is posted to
// `task_runner` with PostDelayedTask, so it follows mock time.
class SimulatedNetworkLink {
 public:
  struct Config {
    // Bottleneck capacity, in bits per second.
    int64_t capacity_bps = 30'000'000;

    // Per-packet overhead added to the payload size when computing
    // serialization time and queue occupancy (e.g. IP + UDP headers).
    size_t per_packet_overhead_bytes = 28;

    // Maximum number of bytes that can be queued at the bottleneck, including
    // the packet currently being serialized.
    size_t queue_limit_bytes = 256 * 1024;

    // Fixed one-way propagation delay.
    base::TimeDelta propagation_delay = base::Milliseconds(5);

    // Maximum additional, uniformly distributed, per-packet delay.
    base::TimeDelta max_jitter = base::Milliseconds(20);

    // Probability that any given packet is lost, independently of others.
    double random_loss_rate = 0.0;

    // Average number of loss bursts per second (Poisson process in time).
    double loss_bursts_per_second = 0.0;

    // Maximum number of consecutive packets lost in a single burst. The
    // burst length is uniformly distributed in [1, max_loss_burst_packets].
    int max_loss_burst_packets = 5;

    // A burst only affects packets sent within this long after it starts, so
    // that bursts model short periods of bad radio conditions rather than
    // targeting whichever packets happen to be sent next.
    base::TimeDelta loss_burst_duration = base::Milliseconds(20);

    // Seed for the pseudo-random number generator.
    uint32_t seed = 1;

    Config();
    Config(const Config&);
    Config& operator=(const Config&);
    ~Config();
  };

  struct Stats {
    int64_t packets_sent = 0;
    int64_t packets_delivered = 0;
    int64_t packets_lost_randomly = 0;
    int64_t packets_dropped_by_queue = 0;
    int64_t bytes_sent = 0;
    int64_t bytes_delivered = 0;
    base::TimeDelta max_queueing_delay;
  };

  using DeliverCallback =
      base::RepeatingCallback<void(const std::vector<uint8_t>& packet)>;

  SimulatedNetworkLink(const Config& config,
                       const base::TickClock* clock,
                       scoped_refptr<base::SequencedTaskRunner> task_runner,
                       DeliverCallback deliver_cb);
  SimulatedNetworkLink(const SimulatedNetworkLink&) = delete;
  SimulatedNetworkLink& operator=(const SimulatedNetworkLink&) = delete;
  ~SimulatedNetworkLink();

  // Enqueues `packet` for transmission over the link.
  void Send(base::span<const uint8_t> packet);

  // Updates link bottleneck capacity or random packet loss rate dynamically.
  void SetCapacityBps(int64_t capacity_bps);
  void SetRandomLossRate(double random_loss_rate);

  // The number of bytes currently queued at the bottleneck.
  size_t GetQueuedBytes() const;

  const Stats& stats() const { return stats_; }
  const Config& config() const { return config_; }

 private:
  // Returns a uniformly distributed value in [0, 1).
  double NextUniform();

  // Returns true if the next packet should be lost.
  bool ShouldLosePacket(base::TimeTicks now);

  void ScheduleNextLossBurst(base::TimeTicks after);

  void Deliver(std::vector<uint8_t> packet);

  Config config_;
  const raw_ptr<const base::TickClock> clock_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  const DeliverCallback deliver_cb_;

  uint32_t rng_state_;

  // Time at which the bottleneck finishes serializing all queued packets.
  base::TimeTicks busy_until_;

  // Arrival time of the most recently scheduled packet, used to prevent
  // reordering.
  base::TimeTicks last_arrival_time_;

  // Time at which the next loss burst begins, if bursts are enabled.
  base::TimeTicks next_loss_burst_time_;

  // Number of packets still to be lost in the current burst, and when the
  // current burst ends.
  int remaining_burst_losses_ = 0;
  base::TimeTicks burst_end_time_;

  Stats stats_;

  base::WeakPtrFactory<SimulatedNetworkLink> weak_factory_{this};
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_SIMULATED_NETWORK_LINK_H_
