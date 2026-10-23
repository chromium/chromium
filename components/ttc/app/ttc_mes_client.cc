// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ttc/app/ttc_mes_client.h"

#include <stdint.h>

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/time/time.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/optimization_guide/core/feature_registry/feature_keys.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/ttc/app/public/server_journal_event.h"
#include "components/ttc/features.h"

namespace ttc {

namespace {

template <typename EnumType>
std::string EnumToString(EnumType value,
                         const std::string& (*name_fn)(EnumType)) {
  const std::string& name = name_fn(value);
  return name.empty() ? base::ToString(static_cast<int>(value)) : name;
}

optimization_guide::proto::ToolDefinition_ExecutionBehavior ToProtoBehavior(
    ToolDefinition::Behavior behavior) {
  switch (behavior) {
    case ToolDefinition::Behavior::kNonBlocking:
      return optimization_guide::proto::ToolDefinition::BEHAVIOR_NON_BLOCKING;
    case ToolDefinition::Behavior::kBlocking:
      return optimization_guide::proto::ToolDefinition::BEHAVIOR_BLOCKING;
  }
}

optimization_guide::proto::ToolDefinition_VerbalizationBehavior
ToProtoVerbalization(ToolDefinition::Verbalization verbalization) {
  switch (verbalization) {
    case ToolDefinition::Verbalization::kSilentAction:
      return optimization_guide::proto::ToolDefinition::
          VERBALIZATION_SILENT_ACTION;
    case ToolDefinition::Verbalization::kStandard:
      return optimization_guide::proto::ToolDefinition::VERBALIZATION_STANDARD;
  }
}

// Encodes `response` as the JSON object the server expects: successful calls
// report {"status": "ok"} alongside any tool-specific values, and failures
// report {"error": {"message": <message>, "code": <numeric actor error code>}},
// where each of `message` and `code` is present only if set on the error.
base::DictValue ToResponseDict(ToolResponse response) {
  if (!response.Ok()) {
    const ToolError& error = response.error();
    VLOG(1) << "Tool call failed: "
            << (error.code ? base::ToString(*error.code) : "no code") << ": "
            << error.message.value_or("no message");
    CHECK(error.message || error.code);

    base::DictValue error_dict;
    if (error.message) {
      error_dict.Set("message", *error.message);
    }
    if (error.code) {
      error_dict.Set("code", static_cast<int>(*error.code));
    }

    base::DictValue dict;
    dict.Set("error", std::move(error_dict));
    return dict;
  }

  base::DictValue dict = std::move(response).TakeResult();
  dict.Set("status", "ok");
  return dict;
}

ServerJournalEvent::Type ToServerJournalEventType(
    optimization_guide::proto::JournalEvent_JournalEventType type) {
  switch (type) {
    case optimization_guide::proto::JournalEvent::JOURNAL_EVENT_TYPE_INSTANT:
      return ServerJournalEvent::Type::kInstant;
    case optimization_guide::proto::JournalEvent::
        JOURNAL_EVENT_TYPE_ASYNC_BEGIN:
      return ServerJournalEvent::Type::kAsyncBegin;
    case optimization_guide::proto::JournalEvent::JOURNAL_EVENT_TYPE_ASYNC_END:
      return ServerJournalEvent::Type::kAsyncEnd;
    case optimization_guide::proto::JournalEvent::JOURNAL_EVENT_TYPE_CLOCK_SYNC:
      return ServerJournalEvent::Type::kClockSync;
    case optimization_guide::proto::JournalEvent::
        JOURNAL_EVENT_TYPE_UNSPECIFIED:
    default:
      return ServerJournalEvent::Type::kUnspecified;
  }
}

ServerJournalEvent::Track ToServerJournalEventTrack(
    optimization_guide::proto::JournalEvent_Track track) {
  switch (track) {
    case optimization_guide::proto::JournalEvent::TRACK_MAIN:
      return ServerJournalEvent::Track::kMain;
    case optimization_guide::proto::JournalEvent::TRACK_TURN:
      return ServerJournalEvent::Track::kTurn;
    case optimization_guide::proto::JournalEvent::TRACK_UNSPECIFIED:
    default:
      return ServerJournalEvent::Track::kUnspecified;
  }
}

base::Time ToTime(const optimization_guide::proto::Timestamp& proto_timestamp) {
  return base::Time::UnixEpoch() + base::Seconds(proto_timestamp.seconds()) +
         base::Nanoseconds(proto_timestamp.nanos());
}

ServerJournalEvent ToServerJournalEvent(
    const optimization_guide::proto::JournalEvent& proto_event) {
  ServerJournalEvent event;
  if (proto_event.has_timestamp()) {
    event.timestamp = ToTime(proto_event.timestamp());
  }
  event.type = ToServerJournalEventType(proto_event.type());
  event.name = proto_event.name();
  event.details.reserve(proto_event.details_size());
  for (const auto& detail : proto_event.details()) {
    event.details.push_back({detail.key(), detail.value()});
  }
  event.async_event_id = proto_event.async_event_id();
  if (proto_event.has_sync_timestamp()) {
    event.sync_timestamp = ToTime(proto_event.sync_timestamp());
  }
  event.track = ToServerJournalEventTrack(proto_event.track());
  return event;
}

}  // namespace

TtcMesClient::TtcMesClient(
    optimization_guide::RemoteModelExecutor* model_executor,
    SessionJournal* journal)
    : model_executor_(model_executor), journal_(journal) {}

TtcMesClient::~TtcMesClient() {
  Close();
}

void TtcMesClient::Connect(Observer* observer) {
  CHECK(observer);

  if (session_ && is_transport_connected_) {
    return;
  }

  observer_ = observer;

  if (!model_executor_) {
    observer_->OnBackendError(ErrorCode::kOptimizationGuideUnavailable);
    return;
  }

  optimization_guide::StreamingModelExecutionOptions options;
  options.idle_disconnect_timeout = base::Minutes(10);

  session_ = model_executor_->StartStreamingSession(
      optimization_guide::ModelBasedCapabilityKey::kTtc, options,
      base::BindRepeating(&TtcMesClient::OnStreamingResult,
                          weak_factory_.GetWeakPtr()));

  if (!session_) {
    observer_->OnBackendError(ErrorCode::kExecutionSessionCreationFailed);
    return;
  }

  session_->AddObserver(this);

  // Send initial session setup request.
  optimization_guide::proto::TtcClientFrame setup_frame;
  setup_frame.set_client_timestamp_ms(
      base::Time::Now().InMillisecondsSinceUnixEpoch());
  auto* setup = setup_frame.mutable_setup_request();
  setup->set_persona("standard");
  setup->set_audio_codec(optimization_guide::proto::AUDIO_CODEC_PCM16K16BIT);
  setup->set_enable_turn_metrics(true);
  SendFrame(setup_frame);
  setup_timer_.Start(
      FROM_HERE, kTtcSetupTimeout.Get(),
      base::BindOnce(&TtcMesClient::OnSetupTimeout, base::Unretained(this)));
}

void TtcMesClient::OnConnectionStateChanged(
    optimization_guide::RemoteModelExecutionSession::ConnectionState state) {
  // kConnecting is sent before being connected. That's not a disconnection (or
  // a connection) so avoid reporting it.
  if (state == optimization_guide::RemoteModelExecutionSession::
                   ConnectionState::kConnecting) {
    return;
  }

  is_transport_connected_ =
      (state == optimization_guide::RemoteModelExecutionSession::
                    ConnectionState::kConnected);
  if (state == optimization_guide::RemoteModelExecutionSession::
                   ConnectionState::kDisconnected) {
    observer_->OnBackendClosed();
  }
}

void TtcMesClient::OnStreamingResult(
    optimization_guide::OptimizationGuideModelStreamingResult result) {
  if (!result.response.has_value()) {
    Log("ModelExecutionError",
        actor::JournalDetailsBuilder()
            .AddError(base::ToString(result.response.error().error()))
            .Build(),
        SessionJournal::Track::kMesClientRecv);
    observer_->OnBackendError(ErrorCode::kInternalBackendError);
    return;
  }

  optimization_guide::proto::TtcServerFrame server_frame;
  if (!server_frame.ParseFromString(result.response->value())) {
    return;
  }

  HandleServerFrame(server_frame);
}

void TtcMesClient::OnSetupTimeout() {
  Observer* observer = observer_;
  Close();
  observer->OnBackendError(ErrorCode::kSetupTimeout);
}

void TtcMesClient::HandleServerFrame(
    const optimization_guide::proto::TtcServerFrame& frame) {
  LogServerFrame(frame);

  if (frame.has_session_status()) {
    setup_timer_.Stop();
    session_id_ = frame.session_status().server_session_id();
    // TODO(b/564241442): SessionStatus could be unsuccessful.
    observer_->OnBackendInitialized();
  }

  if (frame.has_server_content()) {
    const auto& content = frame.server_content();
    if (content.has_audio_output()) {
      // The payload length is peer-controlled so guard that it can indeed be
      // split into 16-bit frames.
      base::span<const uint8_t> raw_bytes =
          base::as_byte_span(content.audio_output().audio_data());
      if (raw_bytes.size() % sizeof(int16_t) != 0) {
        // TODO(bokan): We should probably show an error and close down the
        // session here.
        VLOG(1) << "Discarding malformed audio chunk of " << raw_bytes.size()
                << " bytes; not a whole number of PCM16 samples";
      } else {
        observer_->OnAudioOutput(
            base::subtle::reinterpret_span<const int16_t>(raw_bytes),
            content.audio_output().sequence_number());
      }
    }

    if (!content.input_transcription().empty() ||
        !content.output_transcription().empty()) {
      observer_->OnTranscriptions(content.input_transcription(),
                                  content.output_transcription());
    }
  }

  if (frame.has_interrupted()) {
    observer_->OnGenerationStateChanged(/*started=*/false,
                                        /*completed=*/false,
                                        /*interrupted=*/true);
  }

  if (frame.has_turn_complete()) {
    observer_->OnGenerationStateChanged(/*started=*/false,
                                        /*completed=*/true,
                                        /*interrupted=*/false);
  }

  if (frame.has_tool_call()) {
    HandleToolCall(frame.tool_call());
  }

  if (frame.has_server_error()) {
    optimization_guide::proto::ServerErrorNotification_ErrorCode
        server_error_code = frame.server_error().error_code();
    ErrorCode ttc_error_code = ErrorCode::kUnknown;
    // The proto enum is open, so the code can be any int.
    if (server_error_code >= 0 &&
        server_error_code <= static_cast<int>(ErrorCode::kMaxServerErrorCode)) {
      ttc_error_code = static_cast<ErrorCode>(server_error_code);
    }
    observer_->OnBackendError(ttc_error_code);
  }

  if (frame.has_go_away()) {
    observer_->OnBackendClosed();
  }

  if (frame.has_journal_event()) {
    observer_->OnJournalEvent(ToServerJournalEvent(frame.journal_event()));
  }
}

void TtcMesClient::SendToolSetUpdate(const std::vector<ToolDefinition>& tools) {
  optimization_guide::proto::TtcClientFrame frame;
  frame.set_client_timestamp_ms(
      base::Time::Now().InMillisecondsSinceUnixEpoch());
  auto* update = frame.mutable_toolset_update();
  for (const ToolDefinition& tool_def : tools) {
    auto* proto_tool = update->add_active_tools();
    proto_tool->set_name(tool_def.name);
    proto_tool->set_description(tool_def.description);
    if (!tool_def.parameters_json_schema.empty()) {
      std::optional<std::string> params_json =
          base::WriteJson(tool_def.parameters_json_schema);
      CHECK(params_json);
      proto_tool->set_parameters_json_schema(std::move(*params_json));
    }
    proto_tool->set_behavior(ToProtoBehavior(tool_def.behavior));
    proto_tool->set_verbalization(ToProtoVerbalization(tool_def.verbalization));
  }
  SendFrame(frame);
}

void TtcMesClient::HandleToolCall(
    const optimization_guide::proto::ToolCall& tool_call) {
  if (!observer_) {
    return;
  }
  ToolRequest tool_request;
  tool_request.name = tool_call.name();
  if (!tool_call.arguments_json().empty()) {
    std::optional<base::DictValue> args_dict = base::JSONReader::ReadDict(
        tool_call.arguments_json(), base::JSON_PARSE_RFC);
    if (!args_dict) {
      LOG(ERROR) << "Failed to parse tool call arguments as JSON: "
                 << tool_call.arguments_json();
      OnToolExecutionComplete(
          tool_call.call_id(), tool_call.name(),
          ToolResponse::Error(actor::mojom::ActionResultCode::kArgumentsInvalid,
                              "Invalid JSON provided for tool arguments"));
      return;
    }
    tool_request.arguments = std::move(*args_dict);
  }

  auto callback = base::BindOnce(&TtcMesClient::OnToolExecutionComplete,
                                 weak_factory_.GetWeakPtr(),
                                 tool_call.call_id(), tool_call.name());
  observer_->OnToolCall(std::move(tool_request), std::move(callback));
}

void TtcMesClient::OnToolExecutionComplete(const std::string& call_id,
                                           const std::string& tool_name,
                                           ToolResponse response) {
  optimization_guide::proto::TtcClientFrame frame;
  frame.set_client_timestamp_ms(
      base::Time::Now().InMillisecondsSinceUnixEpoch());
  auto* tool_response = frame.mutable_tool_response();
  tool_response->set_call_id(call_id);
  tool_response->set_name(tool_name);
  std::optional<std::string> json =
      base::WriteJson(ToResponseDict(std::move(response)));
  CHECK(json);
  tool_response->set_response_json(std::move(*json));
  SendFrame(frame);
}

void TtcMesClient::SendAudioChunk(base::span<const int16_t> audio_data) {
  optimization_guide::proto::TtcClientFrame frame;
  frame.set_client_timestamp_ms(
      base::Time::Now().InMillisecondsSinceUnixEpoch());
  auto* chunk = frame.mutable_audio_input();
  base::span<const uint8_t> audio_bytes = base::as_byte_span(audio_data);
  chunk->set_audio_data(audio_bytes.data(), audio_bytes.size());
  chunk->set_timestamp_ms(base::Time::Now().InMillisecondsSinceUnixEpoch());
  SendFrame(frame);
}

void TtcMesClient::SendTextInput(const std::string& text) {
  optimization_guide::proto::TtcClientFrame frame;
  frame.set_client_timestamp_ms(
      base::Time::Now().InMillisecondsSinceUnixEpoch());
  frame.mutable_text_input()->set_text_query(text);
  SendFrame(frame);
}

void TtcMesClient::SendContextUpdate(
    const GURL& url,
    const std::string& title,
    const optimization_guide::proto::AnnotatedPageContent& apc) {
  optimization_guide::proto::TtcClientFrame frame;
  frame.set_client_timestamp_ms(
      base::Time::Now().InMillisecondsSinceUnixEpoch());
  auto* ctx = frame.mutable_context_update();
  ctx->mutable_active_tab()->set_url(url.spec());
  ctx->mutable_active_tab()->set_title(title);
  *ctx->mutable_annotated_page_content() = apc;
  SendFrame(frame);
}

void TtcMesClient::ReportPlaybackStatus(int64_t last_played_sequence_number) {
  optimization_guide::proto::TtcClientFrame frame;
  frame.set_client_timestamp_ms(
      base::Time::Now().InMillisecondsSinceUnixEpoch());
  frame.mutable_playback_status()->set_last_played_sequence_number(
      last_played_sequence_number);
  SendFrame(frame);
}

void TtcMesClient::SendFrame(
    const optimization_guide::proto::TtcClientFrame& frame) {
  if (!session_) {
    return;
  }
  LogClientFrame(frame);
  session_->Send(frame);
}

void TtcMesClient::LogClientFrame(
    const optimization_guide::proto::TtcClientFrame& frame) {
  actor::JournalDetailsBuilder details;
  details.Add("client_timestamp_ms", frame.client_timestamp_ms());

  std::string_view event_name = "TtcClientFrame";
  switch (frame.frame_case()) {
    case optimization_guide::proto::TtcClientFrame::kSetupRequest: {
      event_name = "SessionSetupRequest";
      const auto& setup = frame.setup_request();
      details.Add("persona", setup.persona())
          .Add("audio_codec",
               EnumToString(setup.audio_codec(),
                            &optimization_guide::proto::AudioCodec_Name))
          .Add("enable_turn_metrics", setup.enable_turn_metrics());
      break;
    }
    case optimization_guide::proto::TtcClientFrame::kAudioInput:
      // Don't log, too noisy
      return;
    case optimization_guide::proto::TtcClientFrame::kTextInput:
      event_name = "TextInput";
      details.Add("text_query", frame.text_input().text_query());
      break;
    case optimization_guide::proto::TtcClientFrame::kContextUpdate: {
      event_name = "ContextUpdate";
      const auto& ctx = frame.context_update();
      details.Add("url", ctx.active_tab().url())
          .Add("title", ctx.active_tab().title())
          .Add("annotated_page_content_bytes",
               ctx.annotated_page_content().ByteSizeLong());
      break;
    }
    case optimization_guide::proto::TtcClientFrame::kToolsetUpdate: {
      event_name = "ToolsetUpdate";
      const auto& update = frame.toolset_update();
      for (int i = 0; i < update.active_tools_size(); ++i) {
        const auto& tool = update.active_tools(i);
        const std::string prefix = base::StringPrintf("active_tools[%d].", i);
        details.Add(base::StrCat({prefix, "name"}), tool.name())
            .Add(base::StrCat({prefix, "description"}), tool.description())
            .Add(base::StrCat({prefix, "parameters_json_schema"}),
                 tool.parameters_json_schema())
            .Add(base::StrCat({prefix, "behavior"}),
                 EnumToString(tool.behavior(),
                              &optimization_guide::proto::
                                  ToolDefinition_ExecutionBehavior_Name))
            .Add(base::StrCat({prefix, "verbalization"}),
                 EnumToString(tool.verbalization(),
                              &optimization_guide::proto::
                                  ToolDefinition_VerbalizationBehavior_Name));
      }
      break;
    }
    case optimization_guide::proto::TtcClientFrame::kToolResponse: {
      event_name = "ToolResponse";
      const auto& resp = frame.tool_response();
      details.Add("name", resp.name())
          .Add("call_id", resp.call_id())
          .Add("response_json", resp.response_json());
      if (resp.has_visual_content()) {
        details
            .Add("visual_content.image_data_bytes",
                 resp.visual_content().image_data().size())
            .Add("visual_content.mime_type", resp.visual_content().mime_type());
      }
      break;
    }
    case optimization_guide::proto::TtcClientFrame::kPlaybackStatus:
      event_name = "PlaybackStatus";
      details.Add("last_played_sequence_number",
                  frame.playback_status().last_played_sequence_number());
      break;
    case optimization_guide::proto::TtcClientFrame::kCloseRequest:
      event_name = "CloseSessionRequest";
      details.Add("reason", frame.close_request().reason());
      break;
    case optimization_guide::proto::TtcClientFrame::FRAME_NOT_SET:
      break;
  }

  Log(event_name, std::move(details).Build(),
      SessionJournal::Track::kMesClientSend);
}

void TtcMesClient::LogServerFrame(
    const optimization_guide::proto::TtcServerFrame& frame) {
  actor::JournalDetailsBuilder details;
  details.Add("server_timestamp_ms", frame.server_timestamp_ms());

  std::string_view event_name = "TtcServerFrame";
  switch (frame.frame_case()) {
    case optimization_guide::proto::TtcServerFrame::kSessionStatus: {
      event_name = "SessionStatusResponse";
      const auto& status = frame.session_status();
      details
          .Add("state",
               EnumToString(status.state(),
                            &optimization_guide::proto::
                                SessionStatusResponse_SessionState_Name))
          .Add("server_session_id", status.server_session_id())
          .Add("diagnostic_error_detail", status.diagnostic_error_detail());
      break;
    }
    case optimization_guide::proto::TtcServerFrame::kServerContent: {
      const auto& content = frame.server_content();
      if (content.input_transcription().empty() &&
          content.output_transcription().empty()) {
        return;
      }
      event_name = "ServerContent";
      details.Add("input_transcription", content.input_transcription())
          .Add("output_transcription", content.output_transcription());
      break;
    }
    case optimization_guide::proto::TtcServerFrame::kToolCall: {
      event_name = "ToolCall";
      const auto& call = frame.tool_call();
      details.Add("name", call.name())
          .Add("call_id", call.call_id())
          .Add("arguments_json", call.arguments_json());
      break;
    }
    case optimization_guide::proto::TtcServerFrame::kGoAway:
      event_name = "GoAway";
      details.Add("time_left_ms", frame.go_away().time_left_ms());
      break;
    case optimization_guide::proto::TtcServerFrame::kServerError: {
      event_name = "ServerErrorNotification";
      const auto& error = frame.server_error();
      details
          .Add("error_code",
               EnumToString(error.error_code(),
                            &optimization_guide::proto::
                                ServerErrorNotification_ErrorCode_Name))
          .Add("error_message", error.error_message());
      break;
    }
    case optimization_guide::proto::TtcServerFrame::kInterrupted:
      event_name = "InterruptionNotice";
      break;
    case optimization_guide::proto::TtcServerFrame::kTurnComplete: {
      event_name = "TurnComplete";
      if (frame.turn_complete().has_turn_metrics()) {
        const auto& metrics = frame.turn_complete().turn_metrics();
        details.Add("input_tokens_used", metrics.input_tokens_used())
            .Add("output_tokens_used", metrics.output_tokens_used());
      }
      break;
    }
    case optimization_guide::proto::TtcServerFrame::kJournalEvent:
      // Don't log as it's redundant
      return;
    case optimization_guide::proto::TtcServerFrame::FRAME_NOT_SET:
      break;
  }

  Log(event_name, std::move(details).Build(),
      SessionJournal::Track::kMesClientRecv);
}

void TtcMesClient::Log(std::string_view event_name,
                       std::vector<actor::mojom::JournalDetailsPtr> details,
                       SessionJournal::Track track) {
  if (journal_) {
    journal_->Log(event_name, std::move(details), track);
  }
}

void TtcMesClient::Close() {
  setup_timer_.Stop();
  if (session_) {
    session_->RemoveObserver(this);
    session_.reset();
  }
  observer_ = nullptr;
  is_transport_connected_ = false;
  session_id_.clear();
}

bool TtcMesClient::is_transport_connected() const {
  return is_transport_connected_;
}

}  // namespace ttc
