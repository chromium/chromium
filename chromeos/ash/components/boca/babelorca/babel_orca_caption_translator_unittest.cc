// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/babel_orca_caption_translator.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/metrics_hashes.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/boca/babelorca/fakes/fake_translation_dispatcher.h"
#include "chromeos/ash/components/boca/boca_metrics_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::babelorca {

namespace {

constexpr char kDefaultSourceName[] = "en-US";
constexpr char kDefaultTargetName[] = "de-DE";
constexpr char kIdeographicLanguage[] = "ja-JP";
constexpr char kTranscriptionFull[] =
    "The red fox jumped over the lazy brown dog. Hello there!";
constexpr char kTranscriptionPartial[] =
    "The red fox jumped over the lazy brown dog.";
constexpr char kTranscriptionFullFinal[] =
    "The red fox jumped over the lazy brown dog. Hello there! ";
}  // namespace

class BabelOrcaCaptionTranslatorTest : public testing::Test {
 public:
  BabelOrcaCaptionTranslatorTest() = default;
  ~BabelOrcaCaptionTranslatorTest() override = default;
  BabelOrcaCaptionTranslatorTest(BabelOrcaCaptionTranslatorTest&) = delete;
  BabelOrcaCaptionTranslatorTest operator=(BabelOrcaCaptionTranslatorTest&) =
      delete;

  void SetUp() override {
    auto fake_caption_dispatcher =
        std::make_unique<FakeBabelOrcaTranslationDispatcher>();
    translation_dispatcher_ = fake_caption_dispatcher->GetWeakPtr();
    caption_translator_ = std::make_unique<BabelOrcaCaptionTranslator>(
        std::move(fake_caption_dispatcher));
  }

  void NulloptCallback(
      base::OnceCallback<void()> quit_closure,
      const std::optional<media::SpeechRecognitionResult>& result) {
    EXPECT_FALSE(result);
    std::move(quit_closure).Run();
  }

  void GetFullTranslationCallback(
      base::OnceCallback<void()> quit_closure,
      const std::optional<media::SpeechRecognitionResult>& result) {
    EXPECT_EQ(result->transcription, kTranscriptionFull);
    std::move(quit_closure).Run();
  }

  void GetFullFinalTranslationCallback(
      base::OnceCallback<void()> quit_closure,
      const std::optional<media::SpeechRecognitionResult>& result) {
    EXPECT_EQ(result->transcription, kTranscriptionFullFinal);
    std::move(quit_closure).Run();
  }

  // Like above, but will only quit the run loop iff result.is_final.
  void GetPartialTranslationCallback(
      base::OnceCallback<void()> quit_closure,
      const std::optional<media::SpeechRecognitionResult>& result) {
    if (result->is_final) {
      EXPECT_EQ(result->transcription, kTranscriptionPartial);
      std::move(quit_closure).Run();
    }
  }

  void GetBothPartsOnCallback(
      base::OnceCallback<void()> quit_closure,
      const std::optional<media::SpeechRecognitionResult>& result) {
    ++get_both_parts_callback_call_number_;
    ASSERT_LE(get_both_parts_callback_call_number_, 2);
    if (get_both_parts_callback_call_number_ == 1) {
      EXPECT_EQ(result->transcription, kTranscriptionFull);
    } else if (get_both_parts_callback_call_number_ == 2) {
      EXPECT_EQ(result->transcription, kTranscriptionPartial);
      std::move(quit_closure).Run();
    }
  }

  int get_both_parts_callback_call_number_ = 0;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<BabelOrcaCaptionTranslator> caption_translator_;
  base::WeakPtr<FakeBabelOrcaTranslationDispatcher> translation_dispatcher_;
  base::WeakPtrFactory<BabelOrcaCaptionTranslatorTest> weak_ptr_factory_{this};
};

TEST_F(BabelOrcaCaptionTranslatorTest, NoCallbackNoOp) {
  caption_translator_->Translate(
      media::SpeechRecognitionResult(kTranscriptionFull, false));
  EXPECT_EQ(translation_dispatcher_->GetNumGetTranslationCalls(), 0);
}

TEST_F(BabelOrcaCaptionTranslatorTest, ReturnsNullOptImmediately) {
  base::UserActionTester actions;
  base::HistogramTester histograms;
  base::RunLoop run_loop;
  caption_translator_->InitTranslationAndSetCallback(
      base::BindRepeating(&BabelOrcaCaptionTranslatorTest::NulloptCallback,
                          weak_ptr_factory_.GetWeakPtr(),
                          run_loop.QuitClosure()),
      kDefaultSourceName, kDefaultTargetName);
  caption_translator_->Translate(std::nullopt);
  run_loop.Run();

  histograms.ExpectTotalCount(boca::kBocaBabelorcaTargetLanguage, 1);
  histograms.ExpectBucketCount(boca::kBocaBabelorcaTargetLanguage,
                               base::HashMetricName(kDefaultTargetName), 1);
  EXPECT_EQ(
      actions.GetActionCount(boca::kBocaBabelorcaActionOfStudentSwitchLanguage),
      1);
}

TEST_F(BabelOrcaCaptionTranslatorTest,
       ReturnsTranscriptionIfSourceAndTargetAreTheSame) {
  base::UserActionTester actions;
  base::HistogramTester histograms;
  base::RunLoop run_loop;
  caption_translator_->InitTranslationAndSetCallback(
      base::BindRepeating(
          &BabelOrcaCaptionTranslatorTest::GetFullTranslationCallback,
          weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure()),
      kDefaultTargetName, kDefaultTargetName);
  caption_translator_->Translate(
      media::SpeechRecognitionResult(kTranscriptionFull, false));
  run_loop.Run();

  histograms.ExpectTotalCount(boca::kBocaBabelorcaTargetLanguage, 0);
  EXPECT_EQ(
      actions.GetActionCount(boca::kBocaBabelorcaActionOfStudentSwitchLanguage),
      0);
  EXPECT_EQ(translation_dispatcher_->GetNumGetTranslationCalls(), 0);
}

TEST_F(BabelOrcaCaptionTranslatorTest, NoCacheTranslationNotFinal) {
  base::UserActionTester actions;
  base::HistogramTester histograms;
  base::RunLoop run_loop;
  caption_translator_->InitTranslationAndSetCallback(
      base::BindRepeating(
          &BabelOrcaCaptionTranslatorTest::GetFullTranslationCallback,
          weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure()),
      kIdeographicLanguage, kDefaultTargetName);
  caption_translator_->Translate(
      media::SpeechRecognitionResult(kTranscriptionFull, false));
  run_loop.Run();

  histograms.ExpectTotalCount(boca::kBocaBabelorcaTargetLanguage, 1);
  histograms.ExpectBucketCount(boca::kBocaBabelorcaTargetLanguage,
                               base::HashMetricName(kDefaultTargetName), 1);
  EXPECT_EQ(
      actions.GetActionCount(boca::kBocaBabelorcaActionOfStudentSwitchLanguage),
      1);
}

TEST_F(BabelOrcaCaptionTranslatorTest, NoCacheTranslationFinal) {
  base::UserActionTester actions;
  base::HistogramTester histograms;
  base::RunLoop run_loop;
  caption_translator_->InitTranslationAndSetCallback(
      base::BindRepeating(
          &BabelOrcaCaptionTranslatorTest::GetFullFinalTranslationCallback,
          weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure()),
      kIdeographicLanguage, kDefaultTargetName);
  caption_translator_->Translate(
      media::SpeechRecognitionResult(kTranscriptionFull, true));
  run_loop.Run();

  histograms.ExpectTotalCount(boca::kBocaBabelorcaTargetLanguage, 1);
  histograms.ExpectBucketCount(boca::kBocaBabelorcaTargetLanguage,
                               base::HashMetricName(kDefaultTargetName), 1);
  EXPECT_EQ(
      actions.GetActionCount(boca::kBocaBabelorcaActionOfStudentSwitchLanguage),
      1);
}

TEST_F(BabelOrcaCaptionTranslatorTest, CachingTranslationNoWorkNeeded) {
  base::UserActionTester actions;
  base::HistogramTester histograms;
  base::RunLoop run_loop;
  caption_translator_->InitTranslationAndSetCallback(
      base::BindRepeating(
          &BabelOrcaCaptionTranslatorTest::GetPartialTranslationCallback,
          weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure()),
      kDefaultSourceName, kIdeographicLanguage);

  // we only expect the mock method to be called once here as the cache will
  // contain our translation.
  caption_translator_->Translate(
      media::SpeechRecognitionResult(kTranscriptionFull, false));
  caption_translator_->Translate(
      media::SpeechRecognitionResult(kTranscriptionPartial, true));
  run_loop.Run();

  histograms.ExpectTotalCount(boca::kBocaBabelorcaTargetLanguage, 1);
  histograms.ExpectBucketCount(boca::kBocaBabelorcaTargetLanguage,
                               base::HashMetricName(kIdeographicLanguage), 1);
  EXPECT_EQ(
      actions.GetActionCount(boca::kBocaBabelorcaActionOfStudentSwitchLanguage),
      1);
  EXPECT_EQ(translation_dispatcher_->GetNumGetTranslationCalls(), 1);
}

TEST_F(BabelOrcaCaptionTranslatorTest, CachingTranslationClearsCache) {
  base::UserActionTester actions;
  base::HistogramTester histograms;
  base::RunLoop run_loop;
  caption_translator_->InitTranslationAndSetCallback(
      base::BindRepeating(
          &BabelOrcaCaptionTranslatorTest::GetBothPartsOnCallback,
          weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure()),
      kDefaultSourceName, kIdeographicLanguage);

  // we expect the mock method to be called twice here as the cache will not
  // contain our translation.
  caption_translator_->Translate(
      media::SpeechRecognitionResult(kTranscriptionFull, true));
  caption_translator_->Translate(
      media::SpeechRecognitionResult(kTranscriptionPartial, true));
  run_loop.Run();

  histograms.ExpectTotalCount(boca::kBocaBabelorcaTargetLanguage, 1);
  histograms.ExpectBucketCount(boca::kBocaBabelorcaTargetLanguage,
                               base::HashMetricName(kIdeographicLanguage), 1);
  EXPECT_EQ(
      actions.GetActionCount(boca::kBocaBabelorcaActionOfStudentSwitchLanguage),
      1);
  EXPECT_EQ(translation_dispatcher_->GetNumGetTranslationCalls(), 2);
}

}  // namespace ash::babelorca
