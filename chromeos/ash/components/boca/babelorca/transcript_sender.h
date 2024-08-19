// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TRANSCRIPT_SENDER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TRANSCRIPT_SENDER_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/types/expected.h"
#include "chromeos/ash/components/boca/babelorca/response_callback_wrapper.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace media {
struct SpeechRecognitionResult;
}  // namespace media

namespace net {
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace ash::babelorca {

class BabelOrcaMessage;
class InboxSendResponse;
class TachyonAuthedClient;
class TachyonRequestDataProvider;

// Class to send transcriptions.
class TranscriptSender {
 public:
  TranscriptSender(
      TachyonAuthedClient* authed_client,
      TachyonRequestDataProvider* request_data_provider,
      std::string_view sender_email,
      const net::NetworkTrafficAnnotationTag& network_traffic_annotation,
      size_t max_allowed_char);

  TranscriptSender(const TranscriptSender&) = delete;
  TranscriptSender& operator=(const TranscriptSender&) = delete;

  ~TranscriptSender();

  void SendTranscriptionUpdate(const media::SpeechRecognitionResult& transcript,
                               const std::string& language);

 private:
  BabelOrcaMessage GenerateMessage(
      const media::SpeechRecognitionResult& transcript,
      int part_index,
      const std::string& language);

  void UpdateTranscripts(const media::SpeechRecognitionResult& transcript,
                         const std::string& language);

  void Send(int max_retries, std::string message);

  void OnSendResponse(
      base::expected<InboxSendResponse,
                     ResponseCallbackWrapper::TachyonRequestError> response);

  SEQUENCE_CHECKER(sequence_checker_);

  int message_order_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  int current_transcript_index_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  std::string current_transcript_text_ GUARDED_BY_CONTEXT(sequence_checker_) =
      "";

  std::string previous_language_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::string previous_transcript_text_ GUARDED_BY_CONTEXT(sequence_checker_);

  const raw_ptr<TachyonAuthedClient> authed_client_;
  const raw_ptr<TachyonRequestDataProvider> request_data_provider_;
  const std::string sender_email_;
  const net::NetworkTrafficAnnotationTag network_traffic_annotation_;
  const size_t max_allowed_char_;
  const std::string sender_uuid_;

  base::WeakPtrFactory<TranscriptSender> weak_ptr_factory{this};
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TRANSCRIPT_SENDER_H_
