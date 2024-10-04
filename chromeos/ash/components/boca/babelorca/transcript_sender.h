// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TRANSCRIPT_SENDER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TRANSCRIPT_SENDER_H_

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
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace media {
struct SpeechRecognitionResult;
}  // namespace media

namespace net {
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace ash::babelorca {

class BabelOrcaMessage;
class TachyonAuthedClient;
class TachyonRequestDataProvider;
class TachyonResponse;

// Class to send transcriptions.
class TranscriptSender {
 public:
  struct Options {
    size_t max_allowed_char = 200;
    size_t max_errors_num = 2;
  };

  TranscriptSender(
      TachyonAuthedClient* authed_client,
      TachyonRequestDataProvider* request_data_provider,
      base::Time init_timestamp,
      const net::NetworkTrafficAnnotationTag& network_traffic_annotation,
      Options options,
      base::OnceClosure failure_cb);

  TranscriptSender(const TranscriptSender&) = delete;
  TranscriptSender& operator=(const TranscriptSender&) = delete;

  ~TranscriptSender();

  // Returns `true` if will accept sending request, `false` otherwise.
  // Currently, it only rejects sending if max number of errors is reached.
  bool SendTranscriptionUpdate(const media::SpeechRecognitionResult& transcript,
                               const std::string& language);

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
  const net::NetworkTrafficAnnotationTag network_traffic_annotation_;
  const Options options_;
  base::OnceClosure failure_cb_;
  size_t errors_num_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  base::WeakPtrFactory<TranscriptSender> weak_ptr_factory{this};
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TRANSCRIPT_SENDER_H_
