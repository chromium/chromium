// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TRANSCRIPT_SENDER_RATE_LIMITER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TRANSCRIPT_SENDER_RATE_LIMITER_H_

#include <memory>
#include <string>

#include "base/containers/queue.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "media/mojo/mojom/speech_recognition_result.h"

namespace ash::babelorca {

class TranscriptSender;

class TranscriptSenderRateLimiter {
 public:
  struct Options {
    base::TimeDelta min_send_delay = base::Seconds(1);
    size_t max_queue_size = 10;
  };
  explicit TranscriptSenderRateLimiter(
      std::unique_ptr<TranscriptSender> sender);

  TranscriptSenderRateLimiter(std::unique_ptr<TranscriptSender> sender,
                              Options options);

  TranscriptSenderRateLimiter(const TranscriptSenderRateLimiter&) = delete;
  TranscriptSenderRateLimiter& operator=(const TranscriptSenderRateLimiter&) =
      delete;

  ~TranscriptSenderRateLimiter();

  void Send(const media::SpeechRecognitionResult& transcript,
            const std::string& language);

 private:
  struct TranscriptData {
    media::SpeechRecognitionResult transcript;
    std::string language;
  };

  void DoSend();

  const std::unique_ptr<TranscriptSender> sender_;
  const Options options_;

  base::queue<TranscriptData> transcript_data_queue_;
  base::Time last_sent_time_;

  base::OneShotTimer send_timer_;
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TRANSCRIPT_SENDER_RATE_LIMITER_H_
