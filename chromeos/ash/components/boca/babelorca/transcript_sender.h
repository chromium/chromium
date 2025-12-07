// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TRANSCRIPT_SENDER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TRANSCRIPT_SENDER_H_

#include <string>

namespace media {
struct SpeechRecognitionResult;
}  // namespace media

namespace ash::babelorca {

class TranscriptSender {
 public:
  TranscriptSender(const TranscriptSender&) = delete;
  TranscriptSender& operator=(const TranscriptSender&) = delete;

  virtual ~TranscriptSender() = default;

  virtual void SendTranscriptionUpdate(
      const media::SpeechRecognitionResult& transcript,
      const std::string& language) = 0;

 protected:
  TranscriptSender() = default;
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TRANSCRIPT_SENDER_H_
