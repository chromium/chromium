// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "chrome/renderer/media/webrtc_logging_agent_impl.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"

namespace chrome {
namespace {

class WebRtcLoggingClientRecorder : public mojom::WebRtcLoggingClient {
 public:
  struct Log {
    std::string buffer;
    int on_stopped_count = 0;
  };

  explicit WebRtcLoggingClientRecorder(Log* log) : log_(log) {}
  ~WebRtcLoggingClientRecorder() override = default;

  // mojom::WebRtcLoggingClient methods:
  void OnAddMessages(
      std::vector<mojom::WebRtcLoggingMessagePtr> messages) override {
    for (auto& message : messages) {
      log_->buffer.append(message->data);
      log_->buffer.append("\n");
    }
  }
  void OnStopped() override { log_->on_stopped_count++; }

 private:
  Log* const log_;
};

}  // namespace

TEST(WebRtcLoggingAgentImplTest, Basic) {
  constexpr char kTestString[] = "abcdefghijklmnopqrstuvwxyz";

  base::test::TaskEnvironment task_environment;

  mojo::UniqueReceiverSet<mojom::WebRtcLoggingClient> client_set;

  WebRtcLoggingAgentImpl agent;
  WebRtcLoggingClientRecorder::Log log;

  // Start agent.
  {
    mojo::PendingRemote<mojom::WebRtcLoggingClient> client;
    client_set.Add(std::make_unique<WebRtcLoggingClientRecorder>(&log),
                   client.InitWithNewPipeAndPassReceiver());
    agent.Start(std::move(client));
  }

  base::RunLoop().RunUntilIdle();

  // These log messages should be added to the log buffer.
  blink::WebRtcLogMessage(kTestString);
  blink::WebRtcLogMessage(kTestString);

  base::RunLoop().RunUntilIdle();

  // Stop logging messages.
  agent.Stop();

  base::RunLoop().RunUntilIdle();

  // This log message should not be added to the log buffer.
  blink::WebRtcLogMessage(kTestString);

  base::RunLoop().RunUntilIdle();

  // Size is calculated as (sizeof(kTestString) - 1 for terminating null
  // + 1 for eol added for each log message in LogMessage) * 2.
  constexpr uint32_t kExpectedSize = sizeof(kTestString) * 2;
  EXPECT_EQ(kExpectedSize, log.buffer.size());

  std::string ref_output = kTestString;
  ref_output.append("\n");
  ref_output.append(kTestString);
  ref_output.append("\n");
  EXPECT_STREQ(ref_output.c_str(), log.buffer.c_str());

  EXPECT_EQ(1, log.on_stopped_count);
}

}  // namespace chrome
