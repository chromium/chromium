// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/transcript_sender.h"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "chromeos/ash/components/boca/babelorca/proto/babel_orca_message.pb.h"
#include "chromeos/ash/components/boca/babelorca/proto/tachyon.pb.h"
#include "chromeos/ash/components/boca/babelorca/proto/tachyon_common.pb.h"
#include "chromeos/ash/components/boca/babelorca/proto/tachyon_enums.pb.h"
#include "chromeos/ash/components/boca/babelorca/request_data_wrapper.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_authed_client.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_constants.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_request_data_provider.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_response.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_utils.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace ash::babelorca {
namespace {

int GetTranscriptPartIndex(const std::string& current_text,
                           const std::string& new_text,
                           size_t max_allowed_char) {
  const int len = new_text.length() < current_text.length()
                      ? new_text.length()
                      : current_text.length();
  int diff_index = 0;
  while (diff_index < len && new_text[diff_index] == current_text[diff_index]) {
    ++diff_index;
  }
  const size_t diff_len = new_text.length() - diff_index;
  if (diff_len < max_allowed_char) {
    const int index = diff_index - (max_allowed_char - diff_len);
    diff_index = index < 0 ? 0 : index;
  }
  return diff_index;
}

std::string CreateRequestString(BabelOrcaMessage message,
                                std::string tachyon_token,
                                std::string group_id,
                                std::string sender_email) {
  Id receiver_id;
  receiver_id.set_id(std::move(group_id));
  receiver_id.set_app(kTachyonAppName);
  receiver_id.set_type(IdType::GROUP_ID);
  InboxSendRequest send_request;
  *send_request.mutable_header() = GetRequestHeaderTemplate();
  send_request.mutable_header()->set_auth_token_payload(
      std::move(tachyon_token));
  *send_request.mutable_dest_id() = receiver_id;

  send_request.mutable_message()->set_message_id(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  send_request.mutable_message()->set_message(message.SerializeAsString());
  *send_request.mutable_message()->mutable_receiver_id() = receiver_id;
  send_request.mutable_message()->mutable_sender_id()->set_id(
      std::move(sender_email));
  send_request.mutable_message()->mutable_sender_id()->set_type(IdType::EMAIL);
  send_request.mutable_message()->mutable_sender_id()->set_app(kTachyonAppName);
  send_request.mutable_message()->set_message_type(InboxMessage::GROUP);
  send_request.mutable_message()->set_message_class(InboxMessage::EPHEMERAL);

  send_request.set_fanout_sender(MessageFanout::OTHER_SENDER_DEVICES);

  return send_request.SerializeAsString();
}

}  // namespace

TranscriptSender::TranscriptSender(
    TachyonAuthedClient* authed_client,
    TachyonRequestDataProvider* request_data_provider,
    base::Time init_timestamp,
    const net::NetworkTrafficAnnotationTag& network_traffic_annotation,
    Options options,
    base::OnceClosure failure_cb)
    : authed_client_(authed_client),
      request_data_provider_(request_data_provider),
      init_timestamp_ms_(init_timestamp.InMillisecondsSinceUnixEpoch()),
      network_traffic_annotation_(network_traffic_annotation),
      options_(std::move(options)),
      failure_cb_(std::move(failure_cb)) {}

TranscriptSender::~TranscriptSender() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool TranscriptSender::SendTranscriptionUpdate(
    const media::SpeechRecognitionResult& transcript,
    const std::string& language) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (errors_num_ >= options_.max_errors_num) {
    return false;
  }
  const int part_index =
      GetTranscriptPartIndex(current_transcript_text_, transcript.transcription,
                             options_.max_allowed_char);
  BabelOrcaMessage message = GenerateMessage(transcript, part_index, language);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(CreateRequestString, std::move(message),
                     request_data_provider_->tachyon_token(),
                     request_data_provider_->group_id(),
                     request_data_provider_->sender_email()),
      base::BindOnce(&TranscriptSender::Send, weak_ptr_factory.GetWeakPtr(),
                     /*max_retries=*/transcript.is_final ? 1 : 0));
  // Should be called after `GenerateMessage`.
  UpdateTranscripts(transcript, language);
  return true;
}

BabelOrcaMessage TranscriptSender::GenerateMessage(
    const media::SpeechRecognitionResult& transcript,
    int part_index,
    const std::string& language) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BabelOrcaMessage message;
  // Set main message metadata.
  message.set_session_id(request_data_provider_->session_id());
  message.set_init_timestamp_ms(init_timestamp_ms_);
  message.set_order(message_order_);
  ++message_order_;

  std::string current_text_part = transcript.transcription.substr(part_index);
  const size_t current_text_part_len = current_text_part.length();
  TranscriptPart* current_transcript_part =
      message.mutable_current_transcript();
  current_transcript_part->set_transcript_id(current_transcript_index_);
  current_transcript_part->set_text_index(part_index);
  current_transcript_part->set_text(std::move(current_text_part));
  current_transcript_part->set_is_final(transcript.is_final);
  current_transcript_part->set_language(language);

  // Set previous transcript if message did not reach
  // `options_.max_allowed_char`.
  if (current_text_part_len < options_.max_allowed_char &&
      !previous_transcript_text_.empty()) {
    const size_t max_prev_len =
        options_.max_allowed_char - current_text_part_len;
    const int prev_index =
        previous_transcript_text_.length() < max_prev_len
            ? 0
            : previous_transcript_text_.length() - max_prev_len;
    std::string prev_text = previous_transcript_text_.substr(prev_index);
    TranscriptPart* previous_transcript_part =
        message.mutable_previous_transcript();
    previous_transcript_part->set_transcript_id(current_transcript_index_ - 1);
    previous_transcript_part->set_text_index(prev_index);
    previous_transcript_part->set_text(std::move(prev_text));
    previous_transcript_part->set_is_final(true);
    previous_transcript_part->set_language(previous_language_);
  }
  return message;
}

void TranscriptSender::UpdateTranscripts(
    const media::SpeechRecognitionResult& transcript,
    const std::string& language) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!transcript.is_final) {
    current_transcript_text_ = transcript.transcription;
    return;
  }
  ++current_transcript_index_;
  previous_language_ = language;
  previous_transcript_text_ = transcript.transcription;
  current_transcript_text_ = "";
}

void TranscriptSender::Send(int max_retries, std::string request_string) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (request_string.empty()) {
    LOG(ERROR) << "Send request is empty.";
    return;
  }

  auto response_callback_wrapper = base::BindOnce(
      &TranscriptSender::OnSendResponse, weak_ptr_factory.GetWeakPtr());
  authed_client_->StartAuthedRequestString(
      std::make_unique<RequestDataWrapper>(
          network_traffic_annotation_, kSendMessageUrl, max_retries,
          std::move(response_callback_wrapper)),
      std::move(request_string));
}

void TranscriptSender::OnSendResponse(TachyonResponse response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (response.ok()) {
    errors_num_ = 0;
    return;
  }
  ++errors_num_;
  if (errors_num_ >= options_.max_errors_num && failure_cb_) {
    std::move(failure_cb_).Run();
  }
}

}  // namespace ash::babelorca
