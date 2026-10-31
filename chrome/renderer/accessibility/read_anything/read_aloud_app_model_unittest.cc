// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything/read_aloud_app_model.h"

#include <memory>
#include <string>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

class ReadAnythingReadAloudAppModelTest : public testing::Test {
 public:
  ReadAnythingReadAloudAppModelTest() = default;
  ~ReadAnythingReadAloudAppModelTest() override = default;
  ReadAnythingReadAloudAppModelTest(const ReadAnythingReadAloudAppModelTest&) =
      delete;
  ReadAnythingReadAloudAppModelTest& operator=(
      const ReadAnythingReadAloudAppModelTest&) = delete;

  void SetUp() override {
    testing::Test::SetUp();
    model_ = std::make_unique<ReadAloudAppModel>();
  }

  void TearDown() override {
    model_->GetDependencyParserModel().Reset();
    task_environment_.RunUntilIdle();
    model_.reset();
    testing::Test::TearDown();
  }

  bool SpeechPlaying() { return model_->speech_playing(); }

  void SetSpeechPlaying(bool speech_playing) {
    model_->SetSpeechPlaying(speech_playing);
  }

  double SpeechRate() { return model_->speech_rate(); }

  void SetSpeechRate(double speech_rate) {
    model_->set_speech_rate(speech_rate);
  }

  const base::ListValue& EnabledLanguages() {
    return model_->languages_enabled_in_pref();
  }

  void SetLanguageEnabled(const std::string& lang, bool enabled) {
    model_->SetLanguageEnabled(lang, enabled);
  }

  const base::DictValue& Voices() { return model_->voices(); }

  void SetVoice(const std::string& voice, const std::string& lang) {
    model_->SetVoice(voice, lang);
  }

  int HighlightGranularity() { return model_->highlight_granularity(); }

  void SetHighlightGranularity(int granularity) {
    model_->set_highlight_granularity(granularity);
  }

  bool IsHighlightOn() { return model_->IsHighlightOn(); }

  std::string DefaultLanguage() { return model_->default_language_code(); }

  void SetDefaultLanguage(std::string lang) {
    model_->set_default_language_code(lang);
  }

  void LogSpeechStop(ReadAloudAppModel::ReadAloudStopSource source) {
    model_->LogSpeechStop(source);
  }

  ReadAloudAppModel& model() { return *model_; }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<ReadAloudAppModel> model_;
};

TEST_F(ReadAnythingReadAloudAppModelTest,
       LogSpeechStop_AudioDidNotStart_LogsDelay) {
  SetSpeechPlaying(true);
  const auto delay = base::Milliseconds(25);
  task_environment_.FastForwardBy(delay);
  auto source = ReadAloudAppModel::ReadAloudStopSource::kCloseReadingMode;
  base::HistogramTester histogram_tester;

  LogSpeechStop(source);

  histogram_tester.ExpectUniqueSample(
      ReadAloudAppModel::kSpeechStopSourceHistogramName, source, 1);
  histogram_tester.ExpectUniqueTimeSample(
      ReadAloudAppModel::kAudioStartTimeFailureHistogramName, delay, 1);
  histogram_tester.ExpectTotalCount(
      ReadAloudAppModel::kAudioStartTimeSuccessHistogramName, 0);
}

TEST_F(ReadAnythingReadAloudAppModelTest,
       LogSpeechStop_AudioDidStart_DoesNotLogDelay) {
  SetSpeechPlaying(true);
  const auto delay = base::Milliseconds(12);
  task_environment_.FastForwardBy(delay);
  model().SetAudioCurrentlyPlaying(true);
  auto source = ReadAloudAppModel::ReadAloudStopSource::kCloseReadingMode;
  base::HistogramTester histogram_tester;

  LogSpeechStop(source);

  histogram_tester.ExpectUniqueSample(
      ReadAloudAppModel::kSpeechStopSourceHistogramName, source, 1);
  histogram_tester.ExpectTotalCount(
      ReadAloudAppModel::kAudioStartTimeSuccessHistogramName, 0);
  histogram_tester.ExpectTotalCount(
      ReadAloudAppModel::kAudioStartTimeFailureHistogramName, 0);
}

TEST_F(ReadAnythingReadAloudAppModelTest,
       LogSpeechStop_LogsStopSourceWithSpeechNotPlaying) {
  auto source = ReadAloudAppModel::ReadAloudStopSource::kFinishContent;
  base::HistogramTester histogram_tester;

  LogSpeechStop(source);

  histogram_tester.ExpectUniqueSample(
      ReadAloudAppModel::kSpeechStopSourceHistogramName, source, 1);
}

TEST_F(ReadAnythingReadAloudAppModelTest,
       LogSpeechStop_LogsStopSourceWithSpeechPlaying) {
  SetSpeechPlaying(true);
  auto source = ReadAloudAppModel::ReadAloudStopSource::kFinishContent;
  base::HistogramTester histogram_tester;

  LogSpeechStop(source);

  histogram_tester.ExpectUniqueSample(
      ReadAloudAppModel::kSpeechStopSourceHistogramName, source, 1);
}

TEST_F(ReadAnythingReadAloudAppModelTest, SetAudioCurrentlyPlaying_LogsDelay) {
  SetSpeechPlaying(true);
  const auto delay = base::Milliseconds(27);
  task_environment_.FastForwardBy(delay);
  base::HistogramTester histogram_tester;

  model().SetAudioCurrentlyPlaying(true);

  histogram_tester.ExpectTotalCount(
      ReadAloudAppModel::kSpeechStopSourceHistogramName, 0);
  histogram_tester.ExpectUniqueTimeSample(
      ReadAloudAppModel::kAudioStartTimeSuccessHistogramName, delay, 1);
  histogram_tester.ExpectTotalCount(
      ReadAloudAppModel::kAudioStartTimeFailureHistogramName, 0);
}

TEST_F(ReadAnythingReadAloudAppModelTest, SpeechPlaying) {
  EXPECT_FALSE(SpeechPlaying());

  SetSpeechPlaying(true);
  EXPECT_TRUE(SpeechPlaying());

  SetSpeechPlaying(false);
  EXPECT_FALSE(SpeechPlaying());
}

TEST_F(ReadAnythingReadAloudAppModelTest, SpeechRate) {
  EXPECT_EQ(SpeechRate(), 1);

  const double speech_rate1 = 0.5;
  SetSpeechRate(speech_rate1);
  EXPECT_EQ(SpeechRate(), speech_rate1);

  const double speech_rate2 = 1.2;
  SetSpeechRate(speech_rate2);
  EXPECT_EQ(SpeechRate(), speech_rate2);
}

TEST_F(ReadAnythingReadAloudAppModelTest, EnabledLanguages) {
  EXPECT_TRUE(EnabledLanguages().empty());

  const std::string enabled_lang = "fr";
  SetLanguageEnabled(enabled_lang, true);
  EXPECT_TRUE(EnabledLanguages().contains(enabled_lang));

  SetLanguageEnabled(enabled_lang, false);
  EXPECT_FALSE(EnabledLanguages().contains(enabled_lang));
}

TEST_F(ReadAnythingReadAloudAppModelTest, Voices) {
  EXPECT_TRUE(Voices().empty());

  const char* lang1 = "pt-br";
  const char* voice1 = "Mulan";
  const char* lang2 = "yue";
  const char* voice2 = "Shang";
  SetVoice(voice1, lang1);
  SetVoice(voice2, lang2);
  EXPECT_TRUE(Voices().contains(lang1));
  EXPECT_TRUE(Voices().contains(lang2));
  EXPECT_STREQ(Voices().FindString(lang1)->c_str(), voice1);
  EXPECT_STREQ(Voices().FindString(lang2)->c_str(), voice2);

  const char* voice3 = "Mushu";
  SetVoice(voice3, lang2);
  EXPECT_TRUE(Voices().contains(lang1));
  EXPECT_TRUE(Voices().contains(lang2));
  EXPECT_STREQ(Voices().FindString(lang2)->c_str(), voice3);
}

TEST_F(ReadAnythingReadAloudAppModelTest, Highlight) {
  EXPECT_EQ(HighlightGranularity(), 0);

  const int off = 1;
  SetHighlightGranularity(off);
  EXPECT_EQ(HighlightGranularity(), off);
  EXPECT_FALSE(IsHighlightOn());

  const int on = 0;
  SetHighlightGranularity(on);
  EXPECT_EQ(HighlightGranularity(), on);
  EXPECT_TRUE(IsHighlightOn());
}

TEST_F(ReadAnythingReadAloudAppModelTest, DefaultLanguageCode) {
  EXPECT_EQ(DefaultLanguage(), "en");

  const char* lang1 = "tr";
  SetDefaultLanguage(lang1);
  EXPECT_EQ(DefaultLanguage(), lang1);

  const char* lang2 = "hi";
  SetDefaultLanguage(lang2);
  EXPECT_EQ(DefaultLanguage(), lang2);
}

TEST_F(ReadAnythingReadAloudAppModelTest, ResetAndLogSingleSampleMetrics) {
  const std::vector<std::string> metrics = {
      "Accessibility.ReadAnything.ReadAloudPlaySessionCount",
      "Accessibility.ReadAnything.ReadAloudPauseSessionCount",
      "Accessibility.ReadAnything.ReadAloudNextButtonSessionCount",
      "Accessibility.ReadAnything.ReadAloudPreviousButtonSessionCount",
  };

  const std::string& playCountName = metrics[0];
  const std::string& pauseCountName = metrics[1];
  const std::string& nextButtonCountName = metrics[2];
  const std::string& previousButtonCountName = metrics[3];

  // Play - pause - play
  model().IncrementMetric(playCountName);
  model().IncrementMetric(pauseCountName);
  model().IncrementMetric(playCountName);

  base::HistogramTester histogram_tester;
  model().ResetAndLogSingleSampleMetrics();

  // Each metric has been logged once.
  for (const auto& metric : metrics) {
    histogram_tester.ExpectTotalCount(metric, /*expected_count=*/1);
  }

  // ReadAloudPlaySessionCount has a single sample of 2.
  // ReadAloudPauseSessionCount has a single sample of 1.
  // ReadAloudNextButtonSessionCount and ReadAloudPreviousButtonSessionCount
  // each have a single sample of 0.
  histogram_tester.ExpectUniqueSample(playCountName, /*sample=*/2,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(pauseCountName, /*sample=*/1,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(nextButtonCountName, /*sample=*/0,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(previousButtonCountName, /*sample=*/0,
                                      /*expected_bucket_count=*/1);

  // Increment in the new session
  model().IncrementMetric(playCountName);
  model().ResetAndLogSingleSampleMetrics();

  // Each metric has two total samples.
  for (const auto& metric : metrics) {
    histogram_tester.ExpectTotalCount(metric, /*expected_count=*/2);
  }

  // ReadAloudPlaySessionCount should now have one sample of 1 and one sample
  // of 2.
  histogram_tester.ExpectBucketCount(playCountName, /*sample=*/1,
                                     /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(playCountName, /*sample=*/2,
                                     /*expected_count=*/1);

  // ReadAloudPauseSessionCount should now have one sample of 1 and one sample
  // of 0.
  histogram_tester.ExpectBucketCount(pauseCountName, /*sample=*/0,
                                     /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(pauseCountName, /*sample=*/1,
                                     /*expected_count=*/1);

  // ReadAloudNextButtonSessionCount and ReadAloudPreviousButtonSessionCount
  // each have two samples of 0.
  histogram_tester.ExpectBucketCount(previousButtonCountName, /*sample=*/0,
                                     /*expected_count=*/2);
  histogram_tester.ExpectBucketCount(nextButtonCountName, /*sample=*/0,
                                     /*expected_count=*/2);
}

TEST_F(ReadAnythingReadAloudAppModelTest,
       ResetAndLogSingleSampleMetrics_PlayFromLineFocus) {
  const std::string lineFocusCountName =
      "Accessibility.ReadAnything.ReadAloudPlayFromLineFocusSessionCount";

  model().IncrementMetric(lineFocusCountName);

  base::HistogramTester histogram_tester;
  model().ResetAndLogSingleSampleMetrics();

  histogram_tester.ExpectUniqueSample(lineFocusCountName, /*sample=*/1,
                                      /*expected_bucket_count=*/1);

  // In the new session with no increment, it should log a sample of 0.
  model().ResetAndLogSingleSampleMetrics();
  histogram_tester.ExpectTotalCount(lineFocusCountName, /*expected_count=*/2);
  histogram_tester.ExpectBucketCount(lineFocusCountName, /*sample=*/0,
                                     /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(lineFocusCountName, /*sample=*/1,
                                     /*expected_count=*/1);
}

TEST_F(ReadAnythingReadAloudAppModelTest, LogPlaybackContext) {
  base::HistogramTester histograms;
  const char* histogram_name =
      "Accessibility.ReadAnything.ReadAloud.PlaybackContext";

  // Test Side Panel
  model_->LogPlaybackContext(
      ReadAloudAppModel::ReadAnythingPlaybackContext::kSidePanel);
  EXPECT_EQ(model_->current_session_context_for_testing(),
            ReadAloudAppModel::ReadAnythingPlaybackContext::kSidePanel);
  histograms.ExpectUniqueSample(
      histogram_name,
      ReadAloudAppModel::ReadAnythingPlaybackContext::kSidePanel, 1);

  // Test Immersive
  model_->LogPlaybackContext(
      ReadAloudAppModel::ReadAnythingPlaybackContext::kImmersive);
  EXPECT_EQ(model_->current_session_context_for_testing(),
            ReadAloudAppModel::ReadAnythingPlaybackContext::kImmersive);
  histograms.ExpectBucketCount(
      histogram_name,
      ReadAloudAppModel::ReadAnythingPlaybackContext::kImmersive, 1);
  histograms.ExpectTotalCount(histogram_name, 2);
}
