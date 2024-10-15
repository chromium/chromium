// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/transcript_sender_rate_limiter.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/location.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/boca/babelorca/transcript_sender.h"
#include "media/mojo/mojom/speech_recognition_result.h"

namespace ash::babelorca {

TranscriptSenderRateLimiter::TranscriptSenderRateLimiter(
    std::unique_ptr<TranscriptSender> sender)
    : TranscriptSenderRateLimiter(std::move(sender), Options()) {}

TranscriptSenderRateLimiter::TranscriptSenderRateLimiter(
    std::unique_ptr<TranscriptSender> sender,
    Options options)
    : sender_(std::move(sender)), options_(options) {}

TranscriptSenderRateLimiter::~TranscriptSenderRateLimiter() = default;

void TranscriptSenderRateLimiter::Send(
    const media::SpeechRecognitionResult& transcript,
    const std::string& language) {
  TranscriptData transcript_data{transcript, language};
  if (transcript_data_queue_.empty() ||
      transcript_data_queue_.back().transcript.is_final) {
    transcript_data_queue_.push(std::move(transcript_data));
    if (options_.max_queue_size > 0 &&
        transcript_data_queue_.size() > options_.max_queue_size) {
      transcript_data_queue_.pop();
    }
  } else {
    transcript_data_queue_.back() = std::move(transcript_data);
  }
  if (send_timer_.IsRunning()) {
    return;
  }
  base::TimeDelta time_elapsed = base::Time::Now() - last_sent_time_;
  if (time_elapsed >= options_.min_send_delay) {
    DoSend();
    return;
  }

  send_timer_.Start(FROM_HERE, options_.min_send_delay - time_elapsed, this,
                    &TranscriptSenderRateLimiter::DoSend);
}

void TranscriptSenderRateLimiter::DoSend() {
  last_sent_time_ = base::Time::Now();
  TranscriptData transcript_data = std::move(transcript_data_queue_.front());
  transcript_data_queue_.pop();
  sender_->SendTranscriptionUpdate(transcript_data.transcript,
                                   transcript_data.language);
  if (!transcript_data_queue_.empty()) {
    send_timer_.Start(FROM_HERE, options_.min_send_delay, this,
                      &TranscriptSenderRateLimiter::DoSend);
  }
}

}  // namespace ash::babelorca
