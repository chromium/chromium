// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/net_log/net_log_proxy_source.h"

#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "net/log/net_log.h"
#include "net/log/net_log_with_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FakeNetLogProxySink : public network::mojom::NetLogProxySink {
 public:
  FakeNetLogProxySink(mojo::PendingReceiver<network::mojom::NetLogProxySink>
                          proxy_sink_receiver,
                      int expected_number_of_events)
      : proxy_sink_receiver_(this, std::move(proxy_sink_receiver)),
        run_loop_quit_after_count_(expected_number_of_events) {}
  FakeNetLogProxySink(const FakeNetLogProxySink&) = delete;
  FakeNetLogProxySink& operator=(const FakeNetLogProxySink&) = delete;

  struct ProxiedEntry {
    ProxiedEntry(uint32_t type,
                 const net::NetLogSource& net_log_source,
                 net::NetLogEventPhase phase,
                 base::TimeTicks time,
                 base::Value::Dict params)
        : type(type),
          net_log_source(net_log_source),
          phase(phase),
          time(time),
          params(std::move(params)) {}

    ProxiedEntry(const ProxiedEntry& other)
        : ProxiedEntry(other.type,
                       other.net_log_source,
                       other.phase,
                       other.time,
                       other.params.Clone()) {}

    uint32_t type;
    net::NetLogSource net_log_source;
    net::NetLogEventPhase phase;
    base::TimeTicks time;
    base::Value::Dict params;
  };

  std::vector<ProxiedEntry> entries() const {
    base::AutoLock lock(lock_);
    return entries_;
  }

  void WaitForExpectedEntries() { run_loop_.Run(); }

  // mojom::NetLogProxySink:
  void AddEntry(uint32_t type,
                const net::NetLogSource& net_log_source,
                net::NetLogEventPhase phase,
                base::TimeTicks time,
                base::Value::Dict params) override {
    base::AutoLock lock(lock_);
    entries_.emplace_back(type, net_log_source, phase, time, std::move(params));
    run_loop_quit_after_count_--;
    EXPECT_LE(0, run_loop_quit_after_count_);
    if (run_loop_quit_after_count_ == 0)
      run_loop_.Quit();
  }

 private:
  mutable base::Lock lock_;
  std::vector<ProxiedEntry> entries_;
  mojo::Receiver<network::mojom::NetLogProxySink> proxy_sink_receiver_;

  int run_loop_quit_after_count_;
  base::RunLoop run_loop_;
};

class NetLogCaptureModeWaiter
    : public net::NetLog::ThreadSafeCaptureModeObserver {
 public:
  explicit NetLogCaptureModeWaiter(
      const std::vector<net::NetLogCaptureModeSet>& expected_modes)
      : expected_modes_(expected_modes) {
    net::NetLog::Get()->AddCaptureModeObserver(this);
  }

  ~NetLogCaptureModeWaiter() override {
    net::NetLog::Get()->RemoveCaptureModeObserver(this);
  }

  void WaitForCaptureModeUpdate() { run_loop_.Run(); }

  // net::NetLog::ThreadSafeCaptureModeObserver:
  void OnCaptureModeUpdated(net::NetLogCaptureModeSet modes) override {
    ASSERT_FALSE(expected_modes_.empty())
        << "NetLogCaptureModeWaiter called too many times, modes = " << modes;
    EXPECT_EQ(expected_modes_.front(), modes);
    expected_modes_.erase(expected_modes_.begin());
    if (expected_modes_.empty())
      run_loop_.Quit();
  }

 private:
  std::vector<net::NetLogCaptureModeSet> expected_modes_;
  base::RunLoop run_loop_;
};

base::Value::Dict NetLogCaptureModeToParams(
    net::NetLogCaptureMode capture_mode) {
  base::Value::Dict dict;
  switch (capture_mode) {
    case net::NetLogCaptureMode::kDefault:
      dict.Set("capture_mode", "kDefault");
      break;
    case net::NetLogCaptureMode::kIncludeSensitive:
      dict.Set("capture_mode", "kIncludeSensitive");
      break;
    case net::NetLogCaptureMode::kEverything:
      dict.Set("capture_mode", "kEverything");
      break;
  }
  return dict;
}

}  // namespace

TEST(NetLogProxySource, OnlyProxiesEventsWhenCaptureModeSetIsNonZero) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  const size_t kExpectedEventCount = 2;

  mojo::Remote<network::mojom::NetLogProxySource> proxy_source_remote;
  mojo::Remote<network::mojom::NetLogProxySink> proxy_sink_remote;
  FakeNetLogProxySink sink(proxy_sink_remote.BindNewPipeAndPassReceiver(),
                           kExpectedEventCount);
  net_log::NetLogProxySource net_log_source(
      proxy_source_remote.BindNewPipeAndPassReceiver(),
      std::move(proxy_sink_remote));

  // No capture modes are set, so should not get proxied.
  task_environment.FastForwardBy(base::Seconds(9876));

  net::NetLogWithSource source0 = net::NetLogWithSource::Make(
      net::NetLog::Get(), net::NetLogSourceType::URL_REQUEST);
  source0.BeginEvent(net::NetLogEventType::REQUEST_ALIVE);

  auto capture_mode_waiter = std::make_unique<NetLogCaptureModeWaiter>(
      std::vector<net::NetLogCaptureModeSet>{net::NetLogCaptureModeToBit(
          net::NetLogCaptureMode::kIncludeSensitive)});
  proxy_source_remote->UpdateCaptureModes(
      net::NetLogCaptureModeToBit(net::NetLogCaptureMode::kIncludeSensitive));
  // Wait for the mojo message to be delivered and the NetLogProxySource to
  // start listening for NetLog events.
  capture_mode_waiter->WaitForCaptureModeUpdate();

  task_environment.FastForwardBy(base::Seconds(5432));
  base::TimeTicks source1_start_ticks = base::TimeTicks::Now();

  net::NetLogWithSource source1 = net::NetLogWithSource::Make(
      net::NetLog::Get(), net::NetLogSourceType::SOCKET);
  task_environment.FastForwardBy(base::Seconds(1));
  base::TimeTicks source1_event0_ticks = base::TimeTicks::Now();
  source1.BeginEvent(net::NetLogEventType::SOCKET_ALIVE);

  task_environment.FastForwardBy(base::Seconds(10));
  base::TimeTicks source1_event1_ticks = base::TimeTicks::Now();
  // Add the second event from a different thread. Use a lambda instead of
  // binding to NetLogWithSource::EndEvent since EndEvent is overloaded and
  // templatized which seems to confuse BindOnce. Capturing is safe here as
  // the test will WaitForExpectedEntries() before completing.
  base::ThreadPool::PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        source1.EndEvent(net::NetLogEventType::SOCKET_ALIVE);
      }));

  // Wait for all the expected events to be proxied over the mojo pipe and
  // recorded.
  sink.WaitForExpectedEntries();

  capture_mode_waiter = std::make_unique<NetLogCaptureModeWaiter>(
      std::vector<net::NetLogCaptureModeSet>{0});
  proxy_source_remote->UpdateCaptureModes(0);
  // Wait for the mojo message to be delivered and the NetLogProxySource to
  // stop listening for NetLog events.
  capture_mode_waiter->WaitForCaptureModeUpdate();

  // No capture modes are set, so should not get proxied.
  net::NetLog::Get()->AddGlobalEntry(net::NetLogEventType::CANCELLED);

  // Run any remaining tasks, just in case there any unexpected events getting
  // proxied, this should give them a chance to get recorded so the test would
  // fail.
  task_environment.RunUntilIdle();

  const auto& entries = sink.entries();
  ASSERT_EQ(kExpectedEventCount, entries.size());

  EXPECT_EQ(static_cast<uint32_t>(net::NetLogEventType::SOCKET_ALIVE),
            entries[0].type);
  EXPECT_EQ(net::NetLogSourceType::SOCKET, entries[0].net_log_source.type);
  EXPECT_EQ(source1.source().id, entries[0].net_log_source.id);
  EXPECT_EQ(source1_start_ticks, entries[0].net_log_source.start_time);
  EXPECT_EQ(net::NetLogEventPhase::BEGIN, entries[0].phase);
  EXPECT_EQ(source1_event0_ticks, entries[0].time);
  EXPECT_TRUE(entries[0].params.empty());

  EXPECT_EQ(static_cast<uint32_t>(net::NetLogEventType::SOCKET_ALIVE),
            entries[1].type);
  EXPECT_EQ(net::NetLogSourceType::SOCKET, entries[1].net_log_source.type);
  EXPECT_EQ(source1.source().id, entries[1].net_log_source.id);
  EXPECT_EQ(source1_start_ticks, entries[1].net_log_source.start_time);
  EXPECT_EQ(net::NetLogEventPhase::END, entries[1].phase);
  EXPECT_EQ(source1_event1_ticks, entries[1].time);
  EXPECT_TRUE(entries[1].params.empty());
}

TEST(NetLogProxySource, ProxiesParamsOfLeastSensitiveCaptureMode) {
  base::test::TaskEnvironment task_environment;

  const size_t kExpectedEventCount = 3;

  mojo::Remote<network::mojom::NetLogProxySource> proxy_source_remote;
  mojo::Remote<network::mojom::NetLogProxySink> proxy_sink_remote;
  FakeNetLogProxySink sink(proxy_sink_remote.BindNewPipeAndPassReceiver(),
                           kExpectedEventCount);
  net_log::NetLogProxySource net_log_source(
      proxy_source_remote.BindNewPipeAndPassReceiver(),
      std::move(proxy_sink_remote));
  net::NetLogWithSource source0 = net::NetLogWithSource::Make(
      net::NetLog::Get(), net::NetLogSourceType::URL_REQUEST);

  auto capture_mode_waiter = std::make_unique<NetLogCaptureModeWaiter>(
      std::vector<net::NetLogCaptureModeSet>{net::NetLogCaptureModeToBit(
          net::NetLogCaptureMode::kIncludeSensitive)});
  proxy_source_remote->UpdateCaptureModes(
      net::NetLogCaptureModeToBit(net::NetLogCaptureMode::kIncludeSensitive));
  // Wait for the mojo message to be delivered and the NetLogProxySource to
  // start listening for NetLog events.
  capture_mode_waiter->WaitForCaptureModeUpdate();
  source0.BeginEvent(net::NetLogEventType::REQUEST_ALIVE,
                     &NetLogCaptureModeToParams);

  capture_mode_waiter = std::make_unique<NetLogCaptureModeWaiter>(
      std::vector<net::NetLogCaptureModeSet>{
          0, net::NetLogCaptureModeToBit(net::NetLogCaptureMode::kDefault)});
  proxy_source_remote->UpdateCaptureModes(
      net::NetLogCaptureModeToBit(net::NetLogCaptureMode::kDefault) |
      net::NetLogCaptureModeToBit(net::NetLogCaptureMode::kIncludeSensitive));
  // Wait for the mojo message to be delivered and the NetLogProxySource to
  // update the level it is listening for NetLog events.
  // Should be listening at the default level only, as that is the lowest
  // level in the capture mode set.
  capture_mode_waiter->WaitForCaptureModeUpdate();
  source0.AddEvent(net::NetLogEventType::FAILED, &NetLogCaptureModeToParams);

  proxy_source_remote->UpdateCaptureModes(
      net::NetLogCaptureModeToBit(net::NetLogCaptureMode::kDefault));
  // NetLogProxySource's observer capture mode shouldn't change, so can't use
  // NetLogCaptureModeWaiter here. Just run the task loops.
  task_environment.RunUntilIdle();
  source0.EndEvent(net::NetLogEventType::REQUEST_ALIVE,
                   &NetLogCaptureModeToParams);

  // Wait for all the expected events to be proxied over the mojo pipe and
  // recorded.
  sink.WaitForExpectedEntries();

  const auto& entries = sink.entries();
  ASSERT_EQ(kExpectedEventCount, entries.size());

  EXPECT_EQ(static_cast<uint32_t>(net::NetLogEventType::REQUEST_ALIVE),
            entries[0].type);
  EXPECT_EQ(net::NetLogSourceType::URL_REQUEST, entries[0].net_log_source.type);
  EXPECT_EQ(source0.source().id, entries[0].net_log_source.id);
  EXPECT_EQ(net::NetLogEventPhase::BEGIN, entries[0].phase);
  EXPECT_EQ(1U, entries[0].params.size());
  const std::string* param = entries[0].params.FindString("capture_mode");
  ASSERT_TRUE(param);
  EXPECT_EQ("kIncludeSensitive", *param);

  EXPECT_EQ(static_cast<uint32_t>(net::NetLogEventType::FAILED),
            entries[1].type);
  EXPECT_EQ(net::NetLogSourceType::URL_REQUEST, entries[1].net_log_source.type);
  EXPECT_EQ(source0.source().id, entries[1].net_log_source.id);
  EXPECT_EQ(net::NetLogEventPhase::NONE, entries[1].phase);
  EXPECT_EQ(1U, entries[1].params.size());
  param = entries[1].params.FindString("capture_mode");
  ASSERT_TRUE(param);
  EXPECT_EQ("kDefault", *param);

  EXPECT_EQ(static_cast<uint32_t>(net::NetLogEventType::REQUEST_ALIVE),
            entries[2].type);
  EXPECT_EQ(net::NetLogSourceType::URL_REQUEST, entries[2].net_log_source.type);
  EXPECT_EQ(source0.source().id, entries[2].net_log_source.id);
  EXPECT_EQ(net::NetLogEventPhase::END, entries[2].phase);
  EXPECT_EQ(1U, entries[2].params.size());
  param = entries[2].params.FindString("capture_mode");
  ASSERT_TRUE(param);
  EXPECT_EQ("kDefault", *param);
}
