// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TRANSCRIPT_SENDER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TRANSCRIPT_SENDER_IMPL_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "chromeos/ash/components/boca/babelorca/transcript_sender.h"

namespace media {
struct SpeechRecognitionResult;
}  // namespace media

namespace ash::babelorca {

class BabelOrcaMessage;
class TachyonAuthedClient;
class TachyonRequestDataProvider;
class TachyonResponse;

// Class to send transcriptions.
class TranscriptSenderImpl : public TranscriptSender {
 public:
  struct Options {
    size_t max_allowed_char = 200;
    size_t max_errors_num = 2;
  };

  TranscriptSenderImpl(TachyonAuthedClient* authed_client,
                       TachyonRequestDataProvider* request_data_provider,
                       base::Time init_timestamp,
                       Options options,
                       base::OnceClosure failure_cb);

  TranscriptSenderImpl(const TranscriptSenderImpl&) = delete;
  TranscriptSenderImpl& operator=(const TranscriptSenderImpl&) = delete;

  ~TranscriptSenderImpl() override;

  // Sends the transcript to the group specified by `request_data_provider`.
  // Only rejects sending if max number of errors is reached.
  void SendTranscriptionUpdate(const media::SpeechRecognitionResult& transcript,
                               const std::string& language) override;

 private:
  BabelOrcaMessage GenerateMessage(
      const media::SpeechRecognitionResult& transcript,
      int part_index,
      const std::string& language);

  void UpdateTranscripts(const media::SpeechRecognitionResult& transcript,
                         const std::string& language);

  void Send(int max_retries, std::string message);

  void OnSendResponse(TachyonResponse response);

  SEQUENCE_CHECKER(sequence_checker_);

  int message_order_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  int current_transcript_index_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  std::string current_transcript_text_ GUARDED_BY_CONTEXT(sequence_checker_) =
      "";

  std::string previous_language_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::string previous_transcript_text_ GUARDED_BY_CONTEXT(sequence_checker_);

  const raw_ptr<TachyonAuthedClient> authed_client_;
  const raw_ptr<TachyonRequestDataProvider> request_data_provider_;
  const int64_t init_timestamp_ms_;
  const Options options_;
  base::OnceClosure failure_cb_;
  size_t errors_num_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  bool failed_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  base::WeakPtrFactory<TranscriptSenderImpl> weak_ptr_factory{this};
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TRANSCRIPT_SENDER_IMPL_H_
