// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/transcript_sender_impl.h"

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

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("ash_babelorca_transcript_sender_impl",
                                        R"(
        semantics {
          sender: "School Tools"
          description: "Sends user speech captions to School Tool session "
                        "members."
          trigger: "User enables sending speech captions during a School Tools "
                    "session."
          data: "User email for sender verification, user speech captions and "
                "oauth token for using the instant messaging service."
          user_data {
            type: ACCESS_TOKEN
            type: EMAIL
            type: USER_CONTENT
          }
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "cros-edu-eng@google.com"
            }
          }
          last_reviewed: "2024-10-17"
        }
        policy {
          cookies_allowed: NO
          setting: "This request cannot be stopped in settings, but will not "
                    "be sent if the user does not enable sending captions in "
                    "School Tools session."
          policy_exception_justification: "Not implemented."
        })");

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

TranscriptSenderImpl::TranscriptSenderImpl(
    TachyonAuthedClient* authed_client,
    TachyonRequestDataProvider* request_data_provider,
    base::Time init_timestamp,
    Options options,
    base::OnceClosure failure_cb)
    : authed_client_(authed_client),
      request_data_provider_(request_data_provider),
      init_timestamp_ms_(init_timestamp.InMillisecondsSinceUnixEpoch()),
      options_(std::move(options)),
      failure_cb_(std::move(failure_cb)) {}

TranscriptSenderImpl::~TranscriptSenderImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void TranscriptSenderImpl::SendTranscriptionUpdate(
    const media::SpeechRecognitionResult& transcript,
    const std::string& language) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (failed_) {
    return;
  }

  if (!request_data_provider_->session_id().has_value() ||
      !request_data_provider_->sender_email().has_value() ||
      !request_data_provider_->tachyon_token().has_value() ||
      !request_data_provider_->group_id().has_value()) {
    LOG(ERROR) << "Session Id is set: "
               << request_data_provider_->session_id().has_value()
               << ", sender email is set: "
               << request_data_provider_->sender_email().has_value()
               << ", tachyon token is set: "
               << request_data_provider_->tachyon_token().has_value()
               << ", group id is set: "
               << request_data_provider_->group_id().has_value();
    failed_ = true;
    std::move(failure_cb_).Run();
    return;
  }
  const int part_index =
      GetTranscriptPartIndex(current_transcript_text_, transcript.transcription,
                             options_.max_allowed_char);
  BabelOrcaMessage message = GenerateMessage(transcript, part_index, language);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(CreateRequestString, std::move(message),
                     request_data_provider_->tachyon_token().value(),
                     request_data_provider_->group_id().value(),
                     request_data_provider_->sender_email().value()),
      base::BindOnce(&TranscriptSenderImpl::Send, weak_ptr_factory.GetWeakPtr(),
                     /*max_retries=*/transcript.is_final ? 1 : 0));
  // Should be called after `GenerateMessage`.
  UpdateTranscripts(transcript, language);
  return;
}

BabelOrcaMessage TranscriptSenderImpl::GenerateMessage(
    const media::SpeechRecognitionResult& transcript,
    int part_index,
    const std::string& language) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BabelOrcaMessage message;
  // Set main message metadata.
  message.set_session_id(request_data_provider_->session_id().value());
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

void TranscriptSenderImpl::UpdateTranscripts(
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

void TranscriptSenderImpl::Send(int max_retries, std::string request_string) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (request_string.empty()) {
    LOG(ERROR) << "Send request is empty.";
    return;
  }

  auto response_callback_wrapper = base::BindOnce(
      &TranscriptSenderImpl::OnSendResponse, weak_ptr_factory.GetWeakPtr());
  authed_client_->StartAuthedRequestString(
      std::make_unique<RequestDataWrapper>(
          kTrafficAnnotation, kSendMessageUrl, max_retries,
          std::move(response_callback_wrapper)),
      std::move(request_string));
}

void TranscriptSenderImpl::OnSendResponse(TachyonResponse response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (response.ok()) {
    errors_num_ = 0;
    return;
  }
  ++errors_num_;
  VLOG(1) << "Send request failed";
  if (errors_num_ >= options_.max_errors_num && failure_cb_) {
    failed_ = true;
    std::move(failure_cb_).Run();
  }
}

}  // namespace ash::babelorca
