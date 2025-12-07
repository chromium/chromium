// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/transcript_sender_rate_limiter.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/boca/babelorca/transcript_sender.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::babelorca {
namespace {

const std::string kLanguage1 = "fr";
const std::string kLanguage2 = "en";

class FakeTranscriptSender : public TranscriptSender {
 public:
  FakeTranscriptSender() = default;

  FakeTranscriptSender(const FakeTranscriptSender&) = delete;
  FakeTranscriptSender& operator=(const FakeTranscriptSender&) = delete;

  ~FakeTranscriptSender() override = default;

  void SendTranscriptionUpdate(const media::SpeechRecognitionResult& transcript,
                               const std::string& language) override {
    transcript_ = transcript;
    language_ = language;
  }

  void Clear() {
    transcript_.reset();
    language_.reset();
  }

  std::optional<media::SpeechRecognitionResult> transcript() {
    return transcript_;
  }

  std::optional<std::string> language() { return language_; }

 private:
  std::optional<media::SpeechRecognitionResult> transcript_;
  std::optional<std::string> language_;
};

TEST(TranscriptSenderRateLimiterTest, SendFirstTranscriptImmediately) {
  auto sender = std::make_unique<FakeTranscriptSender>();
  auto* sender_ptr = sender.get();
  TranscriptSenderRateLimiter sender_rate_limiter(std::move(sender));

  media::SpeechRecognitionResult transcript("transcript", /*is_final=*/false);
  sender_rate_limiter.Send(transcript, kLanguage1);
  std::optional<media::SpeechRecognitionResult> latest_transcript =
      sender_ptr->transcript();
  std::optional<std::string> latest_language = sender_ptr->language();

  ASSERT_TRUE(latest_transcript.has_value());
  EXPECT_EQ(latest_transcript.value(), transcript);
  ASSERT_TRUE(latest_language.has_value());
  EXPECT_EQ(latest_language.value(), kLanguage1);
}

TEST(TranscriptSenderRateLimiterTest, SendImmediatelyIfAfterMinDelay) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  auto sender = std::make_unique<FakeTranscriptSender>();
  auto* sender_ptr = sender.get();
  TranscriptSenderRateLimiter sender_rate_limiter(std::move(sender));

  media::SpeechRecognitionResult transcript1("transcript1", /*is_final=*/false);
  sender_rate_limiter.Send(transcript1, kLanguage1);
  // Advance time by the delay, then send a new transcript.
  task_environment.FastForwardBy(
      TranscriptSenderRateLimiter::Options().min_send_delay);
  media::SpeechRecognitionResult transcript2("transcript2", /*is_final=*/true);
  sender_rate_limiter.Send(transcript2, kLanguage1);
  std::optional<media::SpeechRecognitionResult> latest_transcript =
      sender_ptr->transcript();
  std::optional<std::string> latest_language = sender_ptr->language();

  ASSERT_TRUE(latest_transcript.has_value());
  EXPECT_EQ(latest_transcript.value(), transcript2);
  ASSERT_TRUE(latest_language.has_value());
  EXPECT_EQ(latest_language.value(), kLanguage1);
}

TEST(TranscriptSenderRateLimiterTest, OverwritePendingTranscriptIfNotFinal) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  auto sender = std::make_unique<FakeTranscriptSender>();
  auto* sender_ptr = sender.get();
  TranscriptSenderRateLimiter sender_rate_limiter(std::move(sender));

  media::SpeechRecognitionResult transcript1("transcript1", /*is_final=*/false);
  sender_rate_limiter.Send(transcript1, kLanguage1);
  sender_ptr->Clear();
  task_environment.FastForwardBy(base::Milliseconds(100));

  media::SpeechRecognitionResult transcript2("transcript2", /*is_final=*/false);
  sender_rate_limiter.Send(transcript2, kLanguage1);
  task_environment.FastForwardBy(base::Milliseconds(100));
  media::SpeechRecognitionResult transcript3("transcript3", /*is_final=*/false);
  sender_rate_limiter.Send(transcript3, kLanguage2);
  EXPECT_FALSE(sender_ptr->transcript().has_value());
  EXPECT_FALSE(sender_ptr->language().has_value());

  task_environment.FastForwardBy(
      TranscriptSenderRateLimiter::Options().min_send_delay -
      base::Milliseconds(200));
  std::optional<media::SpeechRecognitionResult> latest_transcript =
      sender_ptr->transcript();
  std::optional<std::string> latest_language = sender_ptr->language();

  ASSERT_TRUE(latest_transcript.has_value());
  EXPECT_EQ(latest_transcript.value(), transcript3);
  ASSERT_TRUE(latest_language.has_value());
  EXPECT_EQ(latest_language.value(), kLanguage2);
}

TEST(TranscriptSenderRateLimiterTest, QueueNewTranscripts) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  auto sender = std::make_unique<FakeTranscriptSender>();
  auto* sender_ptr = sender.get();
  TranscriptSenderRateLimiter sender_rate_limiter(std::move(sender));

  media::SpeechRecognitionResult transcript1("transcript1", /*is_final=*/false);
  sender_rate_limiter.Send(transcript1, kLanguage1);
  sender_ptr->Clear();

  media::SpeechRecognitionResult transcript2("transcript2", /*is_final=*/true);
  sender_rate_limiter.Send(transcript2, kLanguage1);
  media::SpeechRecognitionResult transcript3("transcript3", /*is_final=*/true);
  sender_rate_limiter.Send(transcript3, kLanguage2);

  task_environment.FastForwardBy(
      TranscriptSenderRateLimiter::Options().min_send_delay);
  std::optional<media::SpeechRecognitionResult> latest_transcript =
      sender_ptr->transcript();
  std::optional<std::string> latest_language = sender_ptr->language();

  ASSERT_TRUE(latest_transcript.has_value());
  EXPECT_EQ(latest_transcript.value(), transcript2);
  ASSERT_TRUE(latest_language.has_value());
  EXPECT_EQ(latest_language.value(), kLanguage1);

  task_environment.FastForwardBy(
      TranscriptSenderRateLimiter::Options().min_send_delay);
  latest_transcript = sender_ptr->transcript();
  latest_language = sender_ptr->language();

  ASSERT_TRUE(latest_transcript.has_value());
  EXPECT_EQ(latest_transcript.value(), transcript3);
  ASSERT_TRUE(latest_language.has_value());
  EXPECT_EQ(latest_language.value(), kLanguage2);
}

TEST(TranscriptSenderRateLimiterTest, DropOldTranscriptsIfReachedCapacity) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  auto sender = std::make_unique<FakeTranscriptSender>();
  auto* sender_ptr = sender.get();
  TranscriptSenderRateLimiter sender_rate_limiter(std::move(sender),
                                                  {.max_queue_size = 2});

  media::SpeechRecognitionResult transcript1("transcript1", /*is_final=*/false);
  sender_rate_limiter.Send(transcript1, kLanguage1);
  sender_ptr->Clear();

  media::SpeechRecognitionResult transcript2("transcript2", /*is_final=*/true);
  sender_rate_limiter.Send(transcript2, kLanguage1);
  media::SpeechRecognitionResult transcript3("transcript3", /*is_final=*/true);
  sender_rate_limiter.Send(transcript3, kLanguage1);
  media::SpeechRecognitionResult transcript4("transcript4", /*is_final=*/true);
  sender_rate_limiter.Send(transcript4, kLanguage1);

  task_environment.FastForwardBy(
      TranscriptSenderRateLimiter::Options().min_send_delay);
  std::optional<media::SpeechRecognitionResult> latest_transcript =
      sender_ptr->transcript();

  ASSERT_TRUE(latest_transcript.has_value());
  EXPECT_EQ(latest_transcript.value(), transcript3);

  task_environment.FastForwardBy(
      TranscriptSenderRateLimiter::Options().min_send_delay);
  latest_transcript = sender_ptr->transcript();

  ASSERT_TRUE(latest_transcript.has_value());
  EXPECT_EQ(latest_transcript.value(), transcript4);

  sender_ptr->Clear();
  task_environment.FastForwardBy(
      TranscriptSenderRateLimiter::Options().min_send_delay);

  EXPECT_FALSE(sender_ptr->transcript().has_value());
}

}  // namespace
}  // namespace ash::babelorca
