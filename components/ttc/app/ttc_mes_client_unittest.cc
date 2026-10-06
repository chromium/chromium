// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ttc/app/ttc_mes_client.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/core/model_execution/test/mock_remote_model_executor.h"
#include "components/optimization_guide/proto/features/ttc.pb.h"
#include "components/ttc/app/public/error_codes.h"
#include "components/ttc/app/ttc_backend.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ttc {

namespace {

class FakeObserver : public TtcBackend::Observer {
 public:
  void OnApplicationInitialized() override {}
  void OnApplicationClosed() override {}
  void OnApplicationError(ErrorCode error) override { last_error_ = error; }
  void OnTranscriptions(const std::string& input_transcription,
                        const std::string& output_transcription) override {}
  void OnAudioOutput(base::span<const int16_t> audio_data,
                     int64_t sequence_number) override {
    ++audio_output_count_;
    last_samples_.assign(audio_data.begin(), audio_data.end());
    last_sequence_number_ = sequence_number;
  }
  void OnGenerationStateChanged(bool started,
                                bool completed,
                                bool interrupted) override {}

  int audio_output_count() const { return audio_output_count_; }
  const std::vector<int16_t>& last_samples() const { return last_samples_; }
  int64_t last_sequence_number() const { return last_sequence_number_; }
  std::optional<ErrorCode> last_error() const { return last_error_; }

 private:
  int audio_output_count_ = 0;
  std::vector<int16_t> last_samples_;
  int64_t last_sequence_number_ = -1;
  std::optional<ErrorCode> last_error_;
};

// Stands in for a live MES session so that Connect() succeeds without any
// network access.
class FakeRemoteModelExecutionSession
    : public optimization_guide::RemoteModelExecutionSession {
 public:
  void Send(const google::protobuf::MessageLite& message) override {
    ++sent_frame_count_;
  }
  void AddObserver(Observer* observer) override { observer_ = observer; }
  void RemoveObserver(Observer* observer) override { observer_ = nullptr; }

 private:
  int sent_frame_count_ = 0;
  raw_ptr<Observer> observer_ = nullptr;
};

// Re-exposes the frame handler so tests can deliver server frames as if they
// had arrived over the session.
class TestTtcMesClient : public TtcMesClient {
 public:
  explicit TestTtcMesClient(
      optimization_guide::RemoteModelExecutor* model_executor)
      : TtcMesClient(model_executor) {}

  using TtcMesClient::HandleServerFrame;
};

optimization_guide::proto::TtcServerFrame MakeAudioFrame(
    const std::string& audio_bytes,
    int64_t seq) {
  optimization_guide::proto::TtcServerFrame frame;
  auto* audio_output = frame.mutable_server_content()->mutable_audio_output();
  audio_output->set_audio_data(audio_bytes);
  audio_output->set_sequence_number(seq);
  return frame;
}

optimization_guide::proto::TtcServerFrame MakeServerErrorFrame(
    optimization_guide::proto::ServerErrorNotification::ErrorCode code) {
  optimization_guide::proto::TtcServerFrame frame;
  frame.mutable_server_error()->set_error_code(code);
  return frame;
}

}  // namespace

class TtcMesClientTest : public testing::Test {
 public:
  TtcMesClientTest() {
    ON_CALL(model_executor_, StartStreamingSession).WillByDefault([] {
      return std::make_unique<FakeRemoteModelExecutionSession>();
    });

    client_ = std::make_unique<TestTtcMesClient>(&model_executor_);
    client_->Connect(&observer_);
  }

 protected:
  TestTtcMesClient& client() { return *client_; }

  base::test::TaskEnvironment task_environment_;
  FakeObserver observer_;
  testing::NiceMock<optimization_guide::MockRemoteModelExecutor>
      model_executor_;
  std::unique_ptr<TestTtcMesClient> client_;
};

TEST_F(TtcMesClientTest, ForwardsWellFormedAudioChunkAsSamples) {
  // 3 samples of PCM16, little-endian on the wire: 300, -300, 4096.
  const std::string audio_bytes("\x2c\x01\xd4\xfe\x00\x10", 6);

  client().HandleServerFrame(MakeAudioFrame(audio_bytes, /*seq=*/7));

  EXPECT_EQ(observer_.audio_output_count(), 1);
  EXPECT_THAT(observer_.last_samples(), testing::ElementsAre(300, -300, 4096));
  EXPECT_EQ(observer_.last_sequence_number(), 7);
}

TEST_F(TtcMesClientTest, DiscardsAudioChunkWithPartialTrailingSample) {
  // A truncated or malicious frame whose length is not a whole number of
  // PCM16 samples must be dropped rather than reinterpreted.
  const std::string audio_bytes("\x2c\x01\xd4\xfe\x00", 5);

  client().HandleServerFrame(MakeAudioFrame(audio_bytes, /*seq=*/7));

  EXPECT_EQ(observer_.audio_output_count(), 0);
}

TEST_F(TtcMesClientTest, ForwardsEmptyAudioChunk) {
  client().HandleServerFrame(MakeAudioFrame(std::string(), /*seq=*/7));

  EXPECT_EQ(observer_.audio_output_count(), 1);
  EXPECT_TRUE(observer_.last_samples().empty());
}

TEST_F(TtcMesClientTest, ForwardsServerError) {
  client().HandleServerFrame(
      MakeServerErrorFrame(optimization_guide::proto::ServerErrorNotification::
                               ERROR_CODE_RATE_LIMITED));

  EXPECT_EQ(observer_.last_error(), ErrorCode::kRateLimited);
}

// Server error codes are open enum values, so they may be outside of the range
// of known codes and must then be reported as unknown.
TEST_F(TtcMesClientTest, ReportsOutOfRangeServerErrorAsUnknown) {
  for (int code :
       {-1, static_cast<int>(ErrorCode::kMaxServerErrorCode) + 1,
        static_cast<int>(ErrorCode::kOptimizationGuideUnavailable)}) {
    SCOPED_TRACE(code);
    client().HandleServerFrame(MakeServerErrorFrame(
        static_cast<
            optimization_guide::proto::ServerErrorNotification::ErrorCode>(
            code)));

    EXPECT_EQ(observer_.last_error(), ErrorCode::kUnknown);
  }
}

TEST_F(TtcMesClientTest, ReportsErrorWhenModelExecutorIsNull) {
  FakeObserver observer;
  TestTtcMesClient client(/*model_executor=*/nullptr);
  client.Connect(&observer);

  EXPECT_EQ(observer.last_error(), ErrorCode::kOptimizationGuideUnavailable);
}

}  // namespace ttc
