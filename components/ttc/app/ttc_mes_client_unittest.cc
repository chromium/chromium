// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ttc/app/ttc_mes_client.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/core/model_execution/test/mock_remote_model_executor.h"
#include "components/optimization_guide/proto/features/streaming/ttc.pb.h"
#include "components/ttc/app/public/debug_sink.h"
#include "components/ttc/app/public/error_codes.h"
#include "components/ttc/app/public/tool_types.h"
#include "components/ttc/app/ttc_backend.h"
#include "components/ttc/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace ttc {

namespace {

class FakeObserver : public TtcBackend::Observer {
 public:
  void OnBackendInitialized() override {}
  void OnBackendClosed() override {}
  void OnBackendError(ErrorCode error) override { last_error_ = error; }
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
  void OnToolCall(const ToolRequest& tool_request,
                  ToolResponseCallback response_callback) override {
    last_tool_call_id_ = tool_request.call_id;
    tool_response_callback_ = std::move(response_callback);
  }
  void OnJournalEvent(const ServerJournalEvent& journal_event) override {}

  int audio_output_count() const { return audio_output_count_; }
  const std::vector<int16_t>& last_samples() const { return last_samples_; }
  int64_t last_sequence_number() const { return last_sequence_number_; }
  std::optional<ErrorCode> last_error() const { return last_error_; }
  const std::string& last_tool_call_id() const { return last_tool_call_id_; }
  ToolResponseCallback TakeToolResponseCallback() {
    return std::move(tool_response_callback_);
  }

 private:
  std::string last_tool_call_id_;
  ToolResponseCallback tool_response_callback_;
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

// Records everything reported to the debug tools.
class FakeDebugSink : public DebugSink {
 public:
  void OnClientFrame(const ClientFrame& frame) override {
    std::visit(
        absl::Overload{
            [&](const SetupRequest&) {
              client_frames.push_back("setup_request");
            },
            [&](const AudioInput& audio) {
              client_frames.push_back("audio_input");
              sent.push_back(audio.pcm);
            },
            [&](const TextInput& text) {
              client_frames.push_back(base::StrCat({"text_input:", text.text}));
            },
            [&](const ContextUpdate& context) {
              client_frames.push_back(
                  base::StrCat({"context_update:", context.url, "|",
                                base::NumberToString(context.content_bytes)}));
            },
            [&](const ToolsetUpdate& update) {
              client_frames.push_back(
                  base::StrCat({"toolset_update:",
                                base::JoinString(update.tool_names, ",")}));
            },
            [&](const ToolCallResponse& response) {
              client_frames.push_back("tool_response");
              tool_events.push_back(
                  base::StrCat({"response:", response.call_id}));
              last_response_json = response.response_json;
            },
            [&](const PlaybackStatus& status) {
              client_frames.push_back(base::StrCat(
                  {"playback_status:",
                   base::NumberToString(status.last_played_sequence_number)}));
            },
            [&](const CloseRequest& close) {
              client_frames.push_back(
                  base::StrCat({"close_request:", close.reason}));
            },
        },
        frame);
  }
  void OnFrameDropped() override { ++dropped; }
  void OnTransportStateChanged(bool connected) override {
    transport_states.push_back(connected);
  }
  void OnServerFrame(const ServerFrame& frame) override {
    std::visit(
        absl::Overload{
            [&](const SessionStatus& status) {
              frames.push_back("session_status");
              session_id = status.server_session_id;
              status_diagnostic =
                  base::StrCat({status.state, "|", status.diagnostic});
            },
            [&](const AudioOutput& audio) {
              frames.push_back("audio_output");
              received.push_back(audio.pcm);
              last_sequence = audio.sequence_number;
            },
            [&](const Transcript& transcript) {
              frames.push_back("transcript");
              transcripts.push_back(base::StrCat(
                  {transcript.is_input ? "in:" : "out:", transcript.text}));
            },
            [&](const TurnComplete&) { frames.push_back("turn_complete"); },
            [&](const Interrupted&) { frames.push_back("interrupted"); },
            [&](const ToolCall& tool_call) {
              frames.push_back("tool_call");
              tool_events.push_back(base::StrCat(
                  {"received:", tool_call.call_id, ":", tool_call.name, ":",
                   tool_call.arguments_json}));
            },
            [&](const ServerError& error) {
              frames.push_back("server_error");
              server_errors.push_back(
                  base::StrCat({error.code, "|", error.message}));
            },
            [&](const GoAway& go_away) {
              frames.push_back("go_away");
              go_away_ms = go_away.time_left_ms;
            },
            [&](const ServerJournalEvent& event) {
              frames.push_back("journal");
              journal = event.name;
              for (const ServerJournalEvent::Details& detail : event.details) {
                base::StrAppend(&journal, {"|", detail.key, "=", detail.value});
              }
            },
            [&](const UnknownFrame&) { frames.push_back("unknown"); },
        },
        frame);
  }

  // The kind of each client frame, with its content for the small ones.
  std::vector<std::string> client_frames;
  std::vector<std::vector<int16_t>> sent;
  int dropped = 0;
  std::vector<bool> transport_states;
  std::string session_id;
  // The kind of each server frame.
  std::vector<std::string> frames;
  std::vector<std::string> transcripts;
  std::vector<std::vector<int16_t>> received;
  int64_t last_sequence = -1;
  std::string status_diagnostic;
  // "code|message" for each ServerError.
  std::vector<std::string> server_errors;
  std::optional<int64_t> go_away_ms;
  std::string journal;
  std::vector<std::string> tool_events;
  std::string last_response_json;
};

// Re-exposes the frame handler so tests can deliver server frames as if they
// had arrived over the session.
class TestTtcMesClient : public TtcMesClient {
 public:
  explicit TestTtcMesClient(
      optimization_guide::RemoteModelExecutor* model_executor,
      DebugSink* debug_sink = nullptr)
      : TtcMesClient(model_executor, debug_sink) {}

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

    client_ =
        std::make_unique<TestTtcMesClient>(&model_executor_, &debug_sink_);
    client_->Connect(&observer_);
  }

 protected:
  TestTtcMesClient& client() { return *client_; }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeDebugSink debug_sink_;
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

TEST_F(TtcMesClientTest, ReportsSetupTimeoutAsError) {
  task_environment_.FastForwardBy(kTtcSetupTimeout.Get() - base::Seconds(1));
  EXPECT_EQ(observer_.last_error(), std::nullopt);

  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_EQ(observer_.last_error(), ErrorCode::kSetupTimeout);
  EXPECT_EQ(client().GetExecutionSessionForTesting(), nullptr);
}

TEST_F(TtcMesClientTest, SessionConnectedStatusStopsSetupTimeout) {
  optimization_guide::proto::TtcServerFrame frame;
  frame.mutable_session_status()->set_state(
      optimization_guide::proto::SessionStatusResponse::STATE_CONNECTED);
  client().HandleServerFrame(frame);

  task_environment_.FastForwardBy(kTtcSetupTimeout.Get());

  EXPECT_EQ(observer_.last_error(), std::nullopt);
  EXPECT_NE(client().GetExecutionSessionForTesting(), nullptr);
}

TEST_F(TtcMesClientTest, CloseStopsSetupTimeout) {
  client().Close();

  task_environment_.FastForwardBy(kTtcSetupTimeout.Get());

  EXPECT_EQ(observer_.last_error(), std::nullopt);
}

TEST_F(TtcMesClientTest, DebugSinkGetsExactSentAudio) {
  const std::vector<int16_t> pcm = {1, -1, 32767, -32768};
  client().SendAudioChunk(pcm);
  ASSERT_EQ(debug_sink_.sent.size(), 1u);
  EXPECT_EQ(debug_sink_.sent[0], pcm);
}

TEST_F(TtcMesClientTest, DebugSinkReportsDroppedFramesWithoutSession) {
  client().Close();
  const std::vector<int16_t> pcm = {1, 2};
  client().SendAudioChunk(pcm);
  EXPECT_TRUE(debug_sink_.sent.empty());
  EXPECT_EQ(debug_sink_.dropped, 1);
}

TEST_F(TtcMesClientTest, DebugSinkGetsClientFrames) {
  // Connect() sent the setup request.
  EXPECT_THAT(debug_sink_.client_frames, testing::ElementsAre("setup_request"));
  debug_sink_.client_frames.clear();

  client().SendTextInput("hi");
  optimization_guide::proto::AnnotatedPageContent apc;
  apc.mutable_main_frame_data()->set_title("page");
  client().SendContextUpdate(GURL("https://a.test/"), "title", apc);
  std::vector<ToolDefinition> tools(2);
  tools[0].name = "open_tab";
  tools[1].name = "scroll";
  client().SendToolSetUpdate(tools);
  client().ReportPlaybackStatus(12);

  EXPECT_THAT(debug_sink_.client_frames,
              testing::ElementsAre(
                  "text_input:hi",
                  base::StrCat({"context_update:https://a.test/|",
                                base::NumberToString(apc.ByteSizeLong())}),
                  "toolset_update:open_tab,scroll", "playback_status:12"));
  EXPECT_EQ(debug_sink_.dropped, 0);
}

TEST_F(TtcMesClientTest, DebugSinkGetsTransportState) {
  client().OnConnectionStateChanged(
      optimization_guide::RemoteModelExecutionSession::ConnectionState::
          kConnecting,
      /*disconnect_reason=*/std::nullopt);
  client().OnConnectionStateChanged(
      optimization_guide::RemoteModelExecutionSession::ConnectionState::
          kConnected,
      /*disconnect_reason=*/std::nullopt);
  client().OnConnectionStateChanged(
      optimization_guide::RemoteModelExecutionSession::ConnectionState::
          kDisconnected,
      optimization_guide::RemoteModelExecutionSession::DisconnectReason::
          kServerClosed);
  // Connecting isn't a change of state, so it's not reported.
  EXPECT_THAT(debug_sink_.transport_states, testing::ElementsAre(true, false));
}

TEST_F(TtcMesClientTest, DebugSinkGetsExactReceivedAudio) {
  const std::string audio_bytes("\x2c\x01\xd4\xfe", 4);
  client().HandleServerFrame(MakeAudioFrame(audio_bytes, 7));
  ASSERT_EQ(debug_sink_.received.size(), 1u);
  EXPECT_EQ(debug_sink_.received[0], (std::vector<int16_t>{300, -300}));
  EXPECT_EQ(debug_sink_.last_sequence, 7);
  EXPECT_THAT(debug_sink_.frames, testing::ElementsAre("audio_output"));
}

TEST_F(TtcMesClientTest, DebugSinkReportsMalformedAudio) {
  client().HandleServerFrame(MakeAudioFrame(std::string("\x01", 1), 1));
  EXPECT_TRUE(debug_sink_.received.empty());
  ASSERT_EQ(debug_sink_.server_errors.size(), 1u);
  EXPECT_THAT(debug_sink_.server_errors[0],
              testing::StartsWith("MalformedAudioChunk|malformed audio chunk"));
}

TEST_F(TtcMesClientTest, DebugSinkReportsFrameKinds) {
  optimization_guide::proto::TtcServerFrame status;
  status.mutable_session_status()->set_server_session_id("srv-42");
  client().HandleServerFrame(status);
  EXPECT_EQ(debug_sink_.session_id, "srv-42");

  optimization_guide::proto::TtcServerFrame transcript;
  transcript.mutable_server_content()->set_input_transcription("hello");
  client().HandleServerFrame(transcript);

  optimization_guide::proto::TtcServerFrame tool;
  tool.mutable_tool_call()->set_name("open_tab");
  client().HandleServerFrame(tool);

  optimization_guide::proto::TtcServerFrame done;
  done.mutable_turn_complete();
  client().HandleServerFrame(done);

  client().HandleServerFrame(MakeServerErrorFrame(
      optimization_guide::proto::ServerErrorNotification::ErrorCode(3)));

  EXPECT_THAT(debug_sink_.frames,
              testing::ElementsAre("session_status", "transcript", "tool_call",
                                   "turn_complete", "server_error"));
  EXPECT_THAT(debug_sink_.transcripts, testing::ElementsAre("in:hello"));
}

TEST_F(TtcMesClientTest, DebugSinkReportsFrameWithoutContentAsUnknown) {
  client().HandleServerFrame(optimization_guide::proto::TtcServerFrame());
  EXPECT_THAT(debug_sink_.frames, testing::ElementsAre("unknown"));
}

optimization_guide::proto::TtcServerFrame MakeToolCallFrame(
    const std::string& call_id,
    const std::string& arguments_json) {
  optimization_guide::proto::TtcServerFrame frame;
  auto* tool_call = frame.mutable_tool_call();
  tool_call->set_name("navigate");
  tool_call->set_call_id(call_id);
  tool_call->set_arguments_json(arguments_json);
  return frame;
}

TEST_F(TtcMesClientTest, ToolCallIdIsPropagatedAndReported) {
  client().HandleServerFrame(
      MakeToolCallFrame("call-1", R"({"url":"https://a.test"})"));
  EXPECT_EQ(observer_.last_tool_call_id(), "call-1");
  ASSERT_EQ(debug_sink_.tool_events.size(), 1u);
  EXPECT_EQ(debug_sink_.tool_events[0],
            R"(received:call-1:navigate:{"url":"https://a.test"})");

  observer_.TakeToolResponseCallback().Run(ToolResponse::Success());
  ASSERT_EQ(debug_sink_.tool_events.size(), 2u);
  EXPECT_EQ(debug_sink_.tool_events[1], "response:call-1");
  EXPECT_FALSE(debug_sink_.last_response_json.empty());
}

// The error response is the only report of the rejection.
TEST_F(TtcMesClientTest, ToolCallWithInvalidJsonIsAnsweredWithError) {
  client().HandleServerFrame(MakeToolCallFrame("call-2", "{not json"));
  EXPECT_THAT(debug_sink_.tool_events,
              testing::ElementsAre("received:call-2:navigate:{not json",
                                   "response:call-2"));
  EXPECT_THAT(debug_sink_.last_response_json,
              testing::HasSubstr("Invalid JSON"));
}

TEST_F(TtcMesClientTest, ToolResponseAfterCloseIsOnlyReportedAsDropped) {
  client().HandleServerFrame(MakeToolCallFrame("call-3", "{}"));
  ToolResponseCallback callback = observer_.TakeToolResponseCallback();
  client().Close();
  std::move(callback).Run(ToolResponse::Success());
  EXPECT_THAT(debug_sink_.tool_events,
              testing::ElementsAre("received:call-3:navigate:{}"));
  EXPECT_EQ(debug_sink_.dropped, 1);
}

TEST_F(TtcMesClientTest, DebugSinkGetsServerErrorDetails) {
  optimization_guide::proto::TtcServerFrame error =
      MakeServerErrorFrame(optimization_guide::proto::ServerErrorNotification::
                               ERROR_CODE_INTERNAL_BACKEND_ERROR);
  error.mutable_server_error()->set_error_message("backend exploded");
  client().HandleServerFrame(error);
  EXPECT_THAT(debug_sink_.server_errors,
              testing::ElementsAre(
                  "ERROR_CODE_INTERNAL_BACKEND_ERROR|backend exploded"));

  optimization_guide::proto::TtcServerFrame status;
  status.mutable_session_status()->set_state(
      optimization_guide::proto::SessionStatusResponse::
          STATE_REJECTED_QUOTA_EXCEEDED);
  status.mutable_session_status()->set_diagnostic_error_detail("over quota");
  client().HandleServerFrame(status);
  EXPECT_EQ(debug_sink_.status_diagnostic,
            "STATE_REJECTED_QUOTA_EXCEEDED|over quota");

  optimization_guide::proto::TtcServerFrame go_away;
  go_away.mutable_go_away()->set_time_left_ms(5000);
  client().HandleServerFrame(go_away);
  EXPECT_EQ(debug_sink_.go_away_ms, 5000);

  optimization_guide::proto::TtcServerFrame journal;
  journal.mutable_journal_event()->set_name("ModelCall");
  auto* detail = journal.mutable_journal_event()->add_details();
  detail->set_key("latency");
  detail->set_value("12ms");
  detail = journal.mutable_journal_event()->add_details();
  detail->set_key("model");
  detail->set_value("m1");
  client().HandleServerFrame(journal);
  EXPECT_EQ(debug_sink_.journal, "ModelCall|latency=12ms|model=m1");
}

// The debug tools are off by default, so the client gets no sink. Every
// instrumented path must then be a no-op.
TEST_F(TtcMesClientTest, NullDebugSinkIsSafe) {
  FakeObserver observer;
  TestTtcMesClient client(&model_executor_, /*debug_sink=*/nullptr);
  client.Connect(&observer);

  std::vector<optimization_guide::proto::TtcServerFrame> frames;
  frames.emplace_back();  // FRAME_NOT_SET.
  frames.emplace_back().mutable_session_status()->set_server_session_id("s");
  auto& content = *frames.emplace_back().mutable_server_content();
  content.set_input_transcription("in");
  content.set_output_transcription("out");
  frames.push_back(MakeAudioFrame(std::string("\x2c\x01", 2), /*seq=*/1));
  frames.push_back(MakeAudioFrame(std::string("\x01", 1), /*seq=*/2));
  frames.emplace_back().mutable_interrupted();
  frames.emplace_back().mutable_turn_complete();
  frames.push_back(MakeToolCallFrame("call-1", "{}"));
  frames.push_back(MakeToolCallFrame("call-2", "{not json"));
  frames.push_back(
      MakeServerErrorFrame(optimization_guide::proto::ServerErrorNotification::
                               ERROR_CODE_INTERNAL_BACKEND_ERROR));
  frames.emplace_back().mutable_go_away()->set_time_left_ms(1000);
  auto& journal = *frames.emplace_back().mutable_journal_event();
  journal.set_name("ModelCall");
  auto* detail = journal.add_details();
  detail->set_key("k");
  detail->set_value("v");

  for (const auto& frame : frames) {
    client.HandleServerFrame(frame);
  }
  EXPECT_EQ(observer.audio_output_count(), 1);

  const std::vector<int16_t> pcm = {1, 2};
  client.SendAudioChunk(pcm);
  client.SendTextInput("hi");
  client.SendContextUpdate(GURL("https://a.test"), "title",
                           optimization_guide::proto::AnnotatedPageContent());
  client.SendToolSetUpdate(std::vector<ToolDefinition>(1));
  client.ReportPlaybackStatus(1);
  ToolResponseCallback callback = observer.TakeToolResponseCallback();
  ASSERT_TRUE(callback);
  std::move(callback).Run(ToolResponse::Success());
  client.OnConnectionStateChanged(
      optimization_guide::RemoteModelExecutionSession::ConnectionState::
          kConnected,
      /*disconnect_reason=*/std::nullopt);

  client.Close();
  client.SendAudioChunk(pcm);
}

}  // namespace ttc
