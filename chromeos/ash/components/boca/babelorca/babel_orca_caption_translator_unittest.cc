// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/babel_orca_caption_translator.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/metrics_hashes.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
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
constexpr char kInvalidLanguageName[] = "_";
constexpr char kTranscriptionFull[] =
    "The red fox jumped over the lazy brown dog. Hello there!";
constexpr char kTranscriptionPartial[] =
    "The red fox jumped over the lazy brown dog.";
constexpr char kTranscriptionFullFinal[] =
    "The red fox jumped over the lazy brown dog. Hello there! ";
constexpr char kTranscriptionPartialWithMore[] =
    "the red fox jumped over the lazy brown dog. Hello";
constexpr char kOtherTranscription[] = "This is a second transcript!";
constexpr char kOtherTranscriptionPartial[] = "This is a";
constexpr char kOtherTranscriptionPartialMore[] = "this is a second";
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
    // In tests below we are emulating calls to translate in
    // mid session, so here we inject some default values so
    // that we record the switching metrics correctly.
    caption_translator_->SetDefaultLanguagesForTesting(kDefaultSourceName,
                                                       kDefaultSourceName);
  }

  void GetFullTranslationCallback(
      base::OnceCallback<void()> quit_closure,
      const media::SpeechRecognitionResult& result) {
    EXPECT_EQ(result.transcription, kTranscriptionFull);
    std::move(quit_closure).Run();
  }

  void GetFullFinalTranslationCallback(
      base::OnceCallback<void()> quit_closure,
      const media::SpeechRecognitionResult& result) {
    EXPECT_EQ(result.transcription, kTranscriptionFullFinal);
    std::move(quit_closure).Run();
  }

  // Like above, but will only quit the run loop iff result.is_final.
  void GetPartialTranslationCallback(
      base::OnceCallback<void()> quit_closure,
      const media::SpeechRecognitionResult& result) {
    if (result.is_final) {
      EXPECT_EQ(result.transcription, kTranscriptionPartial);
      std::move(quit_closure).Run();
    }
  }

  void GetBothPartsOnCallback(base::OnceCallback<void()> quit_closure,
                              const media::SpeechRecognitionResult& result) {
    ++get_both_parts_callback_call_number_;
    ASSERT_LE(get_both_parts_callback_call_number_, 2);
    if (get_both_parts_callback_call_number_ == 1) {
      EXPECT_EQ(result.transcription, kTranscriptionFull);
    } else if (get_both_parts_callback_call_number_ == 2) {
      EXPECT_EQ(result.transcription, kTranscriptionPartial);
      std::move(quit_closure).Run();
    }
  }

  int get_both_parts_callback_call_number_ = 0;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<BabelOrcaCaptionTranslator> caption_translator_;
  base::WeakPtr<FakeBabelOrcaTranslationDispatcher> translation_dispatcher_;

  // purposefully uninitted callback for testing bad callbacks passed to the
  // translator
  base::WeakPtrFactory<BabelOrcaCaptionTranslatorTest> weak_ptr_factory_{this};
};

TEST_F(BabelOrcaCaptionTranslatorTest, NoCallbackNoOp) {
  BabelOrcaCaptionTranslator::OnTranslationCallback null_callback;
  caption_translator_->Translate(
      media::SpeechRecognitionResult(kTranscriptionFull, false),
      std::move(null_callback), kDefaultSourceName, kDefaultTargetName);
  EXPECT_EQ(translation_dispatcher_->GetNumGetTranslationCalls(), 0);
}

TEST_F(BabelOrcaCaptionTranslatorTest,
       FailsIfSourceOrTargetAreNotValidLanguages) {
  // Expect these calls to do nothing because the language strings should not
  // be valid.
  caption_translator_->Translate(
      media::SpeechRecognitionResult(kTranscriptionFull, false),
      base::DoNothing(), kInvalidLanguageName, kDefaultTargetName);
  caption_translator_->Translate(
      media::SpeechRecognitionResult(kTranscriptionFull, false),
      base::DoNothing(), kDefaultSourceName, kInvalidLanguageName);
  EXPECT_EQ(translation_dispatcher_->GetNumGetTranslationCalls(), 0);
}

TEST_F(BabelOrcaCaptionTranslatorTest,
       ReturnsTranscriptionIfSourceAndTargetAreTheSame) {
  base::UserActionTester actions;
  base::HistogramTester histograms;
  base::RunLoop run_loop;
  caption_translator_->Translate(
      media::SpeechRecognitionResult(kTranscriptionFull, false),
      base::BindOnce(
          &BabelOrcaCaptionTranslatorTest::GetFullTranslationCallback,
          weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure()),
      kDefaultTargetName, kDefaultTargetName);
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
  caption_translator_->Translate(
      media::SpeechRecognitionResult(kTranscriptionFull, false),
      base::BindOnce(
          &BabelOrcaCaptionTranslatorTest::GetFullTranslationCallback,
          weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure()),
      kIdeographicLanguage, kDefaultTargetName);
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
  caption_translator_->Translate(
      media::SpeechRecognitionResult(kTranscriptionFull, true),
      base::BindOnce(
          &BabelOrcaCaptionTranslatorTest::GetFullFinalTranslationCallback,
          weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure()),
      kIdeographicLanguage, kDefaultTargetName);
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

  // we only expect the mock method to be called once here as the cache will
  // contain our translation.
  caption_translator_->Translate(
      media::SpeechRecognitionResult(kTranscriptionFull, false),
      base::BindOnce(
          &BabelOrcaCaptionTranslatorTest::GetPartialTranslationCallback,
          weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure()),
      kDefaultSourceName, kIdeographicLanguage);
  caption_translator_->Translate(
      media::SpeechRecognitionResult(kTranscriptionPartial, true),
      base::BindOnce(
          &BabelOrcaCaptionTranslatorTest::GetPartialTranslationCallback,
          weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure()),
      kDefaultSourceName, kIdeographicLanguage);
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

  // we expect the mock method to be called twice here as the cache will not
  // contain our translation.
  caption_translator_->Translate(
      media::SpeechRecognitionResult(kTranscriptionFull, true),
      base::BindOnce(&BabelOrcaCaptionTranslatorTest::GetBothPartsOnCallback,
                     weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure()),
      kDefaultSourceName, kIdeographicLanguage);
  caption_translator_->Translate(
      media::SpeechRecognitionResult(kTranscriptionPartial, true),
      base::BindOnce(&BabelOrcaCaptionTranslatorTest::GetBothPartsOnCallback,
                     weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure()),
      kDefaultSourceName, kIdeographicLanguage);
  run_loop.Run();

  histograms.ExpectTotalCount(boca::kBocaBabelorcaTargetLanguage, 1);
  histograms.ExpectBucketCount(boca::kBocaBabelorcaTargetLanguage,
                               base::HashMetricName(kIdeographicLanguage), 1);
  EXPECT_EQ(
      actions.GetActionCount(boca::kBocaBabelorcaActionOfStudentSwitchLanguage),
      1);
  EXPECT_EQ(translation_dispatcher_->GetNumGetTranslationCalls(), 2);
}

// In set up we set the current languages to emulate
// an existing session, for this test we want to emulate
// a new session's first call to translate if the target
// language is distinct from the source.  Since there is
// no switch here we do not expect the switch metric
// to be recorded.
TEST_F(BabelOrcaCaptionTranslatorTest,
       DoesNotRecordMetricIfTranslationWasAlreadyActive) {
  base::UserActionTester actions;
  base::HistogramTester histograms;
  base::RunLoop run_loop;
  caption_translator_->UnsetCurrentLanguagesForTesting();
  caption_translator_->Translate(
      media::SpeechRecognitionResult(kTranscriptionFull, true),
      base::BindOnce(
          &BabelOrcaCaptionTranslatorTest::GetFullFinalTranslationCallback,
          weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure()),
      kIdeographicLanguage, kDefaultTargetName);
  run_loop.Run();

  histograms.ExpectTotalCount(boca::kBocaBabelorcaTargetLanguage, 0);
  histograms.ExpectBucketCount(boca::kBocaBabelorcaTargetLanguage,
                               base::HashMetricName(kDefaultTargetName), 0);
  EXPECT_EQ(
      actions.GetActionCount(boca::kBocaBabelorcaActionOfStudentSwitchLanguage),
      0);
}

// Tests that dispatches are done in order.
TEST_F(BabelOrcaCaptionTranslatorTest, DispatchTranslationsInOrder) {
  bool first_dispatch_handled = false;
  bool second_dispatch_handled = false;
  bool third_dispatch_handled = false;
  bool fourth_dispatch_handled = false;
  bool fifth_dispatch_handled = false;
  bool sixth_dispatch_handled = false;
  bool seventh_dispatch_handled = false;
  base::OnceCallback<void()> first_bound_dispatch;
  base::OnceCallback<void()> second_bound_dispatch;
  base::OnceCallback<void()> third_bound_dispatch;
  base::RunLoop first_run_loop;
  base::RunLoop second_run_loop;
  base::RunLoop third_run_loop;

  // These first two Translate calls ensure that if a segment we're
  // currently working on is updated the Translator will immediately
  // dispatch the updated segment without enqueueing.
  translation_dispatcher_->SetDispatchHandler(base::BindLambdaForTesting(
      [&first_bound_dispatch](base::OnceCallback<void()> callback) {
        first_bound_dispatch = std::move(callback);
      }));
  caption_translator_->Translate(
      media::SpeechRecognitionResult(kOtherTranscriptionPartial,
                                     /*is_final=*/false),
      base::BindLambdaForTesting(
          [&first_dispatch_handled](const media::SpeechRecognitionResult&) {
            first_dispatch_handled = true;
          }),
      kDefaultSourceName, kIdeographicLanguage);

  translation_dispatcher_->SetDispatchHandler(base::BindLambdaForTesting(
      [&second_bound_dispatch](base::OnceCallback<void()> callback) {
        second_bound_dispatch = std::move(callback);
      }));
  caption_translator_->Translate(
      media::SpeechRecognitionResult(kOtherTranscriptionPartialMore,
                                     /*is_final=*/false),
      base::BindLambdaForTesting(
          [&second_dispatch_handled](const media::SpeechRecognitionResult&) {
            second_dispatch_handled = true;
          }),
      kDefaultSourceName, kIdeographicLanguage);

  // We expect that these dispatches are bound, as they should both
  // have skipped the queue since they're int he current segment.
  // These will never be invoked however as the actual dispatcher
  // cancels pending requests if a new one is passed to it.
  ASSERT_FALSE(first_bound_dispatch.is_null());
  ASSERT_FALSE(second_bound_dispatch.is_null());

  translation_dispatcher_->SetDispatchHandler(base::BindLambdaForTesting(
      [&third_bound_dispatch](base::OnceCallback<void()> callback) {
        third_bound_dispatch = std::move(callback);
      }));
  caption_translator_->Translate(
      media::SpeechRecognitionResult(kOtherTranscription, /*is_final=*/true),
      base::BindLambdaForTesting([&third_dispatch_handled, &first_run_loop](
                                     const media::SpeechRecognitionResult&) {
        third_dispatch_handled = true;
        first_run_loop.Quit();
      }),
      kDefaultSourceName, kIdeographicLanguage);

  // we expect the third dispatch to be bound immediately.
  ASSERT_FALSE(third_bound_dispatch.is_null());

  // We should not affect the behavior of the subsequent calls to the dispatcher
  // as we will pretend that the the latency between the server and the client
  // has improved since the delayed dispatch of the first translate.
  translation_dispatcher_->UnsetDispatchHandler();

  // The subsequent calls to translate start work on a new segment.  The first
  // segment will "complete" its request when we invoke the second bound
  // dispatch below. Since the first two calls to translate contains a partial
  // transcription of the final call we expect that they will be skipped and
  // only the last call to translate will actually be dispatched.
  caption_translator_->Translate(
      media::SpeechRecognitionResult(kTranscriptionPartial, /*is_final=*/false),
      base::BindLambdaForTesting(
          [&fourth_dispatch_handled](const media::SpeechRecognitionResult&) {
            fourth_dispatch_handled = true;
          }),
      kDefaultSourceName, kIdeographicLanguage);

  caption_translator_->Translate(
      media::SpeechRecognitionResult(kTranscriptionPartialWithMore,
                                     /*is_final=*/false),
      base::BindLambdaForTesting(
          [&fifth_dispatch_handled](const media::SpeechRecognitionResult&) {
            // The first dispatch should have completed before this one was
            // handled.
            fifth_dispatch_handled = true;
          }),
      kDefaultSourceName, kIdeographicLanguage);

  // Note here that is_final is false, even though it is we still expect that
  // this one will be handled as it is the most up to date segment that the
  // translator has by the time it flushes the queue.
  caption_translator_->Translate(
      media::SpeechRecognitionResult(kTranscriptionFull, /*is_final=*/false),
      base::BindLambdaForTesting([&sixth_dispatch_handled, &second_run_loop](
                                     const media::SpeechRecognitionResult&) {
        sixth_dispatch_handled = true;
        second_run_loop.Quit();
      }),
      kDefaultSourceName, kIdeographicLanguage);

  // At this point the first request is still pending, so we expect that none of
  // the translations have been handled. When we invoke the third_bound_dispatch
  // we then should expect that only the third and fifth callbacks were invoked.
  // The rest were skipped because a more final transcription has come through.
  std::move(third_bound_dispatch).Run();
  first_run_loop.Run();
  second_run_loop.Run();

  // We only expect that the final dispatch in the first segment and the last
  // segment in the queue to be dispatched.
  EXPECT_FALSE(first_dispatch_handled);
  EXPECT_FALSE(second_dispatch_handled);
  EXPECT_TRUE(third_dispatch_handled);
  EXPECT_FALSE(fourth_dispatch_handled);
  EXPECT_FALSE(fifth_dispatch_handled);
  EXPECT_TRUE(sixth_dispatch_handled);

  // Ensure that the translator still dispatches if the last completed request
  // was not final and the queue was empty.
  caption_translator_->Translate(
      media::SpeechRecognitionResult(kOtherTranscription, /*is_final=*/true),
      base::BindLambdaForTesting([&seventh_dispatch_handled, &third_run_loop](
                                     const media::SpeechRecognitionResult&) {
        seventh_dispatch_handled = true;
        third_run_loop.Quit();
      }),
      kDefaultSourceName, kIdeographicLanguage);
  third_run_loop.Run();
  EXPECT_TRUE(seventh_dispatch_handled);
}

}  // namespace ash::babelorca
