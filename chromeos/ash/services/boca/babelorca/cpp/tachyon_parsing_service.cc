// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/boca/babelorca/cpp/tachyon_parsing_service.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/types/expected.h"
#include "chromeos/ash/components/boca/babelorca/proto/babel_orca_message.pb.h"
#include "chromeos/ash/components/boca/babelorca/proto/stream_body.pb.h"
#include "chromeos/ash/components/boca/babelorca/proto/tachyon.pb.h"
#include "chromeos/ash/components/boca/babelorca/proto/tachyon_enums.pb.h"
#include "chromeos/ash/services/boca/babelorca/cpp/proto_http_stream_parser.h"
#include "chromeos/ash/services/boca/babelorca/mojom/tachyon_parsing_service.mojom-shared.h"
#include "chromeos/ash/services/boca/babelorca/mojom/tachyon_parsing_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::babelorca {
namespace {

enum class ConversionStatus { kNotBabelOrca, kError };

mojom::StreamStatusPtr ConvertStatusDataToMojom(const std::string& data) {
  Status proto_status;
  if (!proto_status.ParseFromString(data)) {
    return nullptr;
  }
  mojom::StreamStatusPtr stream_status = mojom::StreamStatus::New();
  stream_status->code = proto_status.code();
  stream_status->message = std::move(proto_status.message());
  return stream_status;
}

mojom::TranscriptPartPtr ConvertTranscriptPartToMojom(
    const TranscriptPart& transcript_proto) {
  mojom::TranscriptPartPtr transcript_mojom = mojom::TranscriptPart::New();
  transcript_mojom->transcript_id = transcript_proto.transcript_id();
  transcript_mojom->is_final = transcript_proto.is_final();
  transcript_mojom->language = transcript_proto.language();
  transcript_mojom->text = transcript_proto.text();
  transcript_mojom->text_index = transcript_proto.text_index();
  return transcript_mojom;
}

base::expected<mojom::BabelOrcaMessagePtr, ConversionStatus> ConvertDataToMojom(
    const std::string& data) {
  ReceiveMessagesResponse received_message;
  if (!received_message.ParseFromString(data)) {
    return base::unexpected(ConversionStatus::kError);
  }
  if (!received_message.has_inbox_message()) {
    return base::unexpected(ConversionStatus::kNotBabelOrca);
  }
  const InboxMessage& inbox_message = received_message.inbox_message();
  BabelOrcaMessage babel_orca_message_proto;
  if (!babel_orca_message_proto.ParseFromString(inbox_message.message())) {
    return base::unexpected(ConversionStatus::kNotBabelOrca);
  }
  mojom::BabelOrcaMessagePtr babel_orca_message_mojom =
      mojom::BabelOrcaMessage::New();
  if (inbox_message.sender_id().type() == IdType::EMAIL) {
    babel_orca_message_mojom->sender_email = inbox_message.sender_id().id();
  }
  babel_orca_message_mojom->session_id = babel_orca_message_proto.session_id();
  babel_orca_message_mojom->init_timestamp_ms =
      babel_orca_message_proto.init_timestamp_ms();
  babel_orca_message_mojom->order = babel_orca_message_proto.order();
  babel_orca_message_mojom->current_transcript = ConvertTranscriptPartToMojom(
      babel_orca_message_proto.current_transcript());
  if (babel_orca_message_proto.has_previous_transcript()) {
    babel_orca_message_mojom->previous_transcript =
        ConvertTranscriptPartToMojom(
            babel_orca_message_proto.previous_transcript());
  }
  return babel_orca_message_mojom;
}

}  // namespace

TachyonParsingService::TachyonParsingService(
    mojo::PendingReceiver<mojom::TachyonParsingService> receiver)
    : receiver_(this, std::move(receiver)),
      stream_parser_(std::make_unique<ProtoHttpStreamParser>()) {}

TachyonParsingService::~TachyonParsingService() = default;

void TachyonParsingService::Parse(const std::string& stream_data,
                                  ParseCallback callback) {
  if ((parsing_state_ != mojom::ParsingState::kOk)) {
    std::move(callback).Run(parsing_state_, {}, nullptr);
    return;
  }
  parsing_state_ = stream_parser_->Append(stream_data);
  std::vector<std::string> parsed_data = stream_parser_->TakeParseResult();

  std::vector<mojom::BabelOrcaMessagePtr> messages;
  mojom::StreamStatusPtr stream_status;
  for (size_t i = 0; i < parsed_data.size(); ++i) {
    if (parsing_state_ == mojom::ParsingState::kClosed &&
        i == parsed_data.size() - 1) {
      stream_status = ConvertStatusDataToMojom(parsed_data[i]);
      if (!stream_status) {
        parsing_state_ = parsing_state_ = mojom::ParsingState::kError;
      }
      break;
    }
    base::expected<mojom::BabelOrcaMessagePtr, ConversionStatus> result =
        ConvertDataToMojom(parsed_data[i]);
    if (result.has_value()) {
      messages.emplace_back(std::move(result.value()));
    } else if (result.error() == ConversionStatus::kError) {
      parsing_state_ = mojom::ParsingState::kError;
      break;
    }
  }
  if ((parsing_state_ != mojom::ParsingState::kOk)) {
    stream_parser_.reset();
  }
  std::move(callback).Run(parsing_state_, std::move(messages),
                          std::move(stream_status));
}

}  // namespace ash::babelorca
