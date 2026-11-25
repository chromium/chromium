// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/on_device_translation/translator.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/on_device_translation/input_budget.h"
#include "components/on_device_translation/public/mojom/translator.mojom.h"
#include "components/on_device_translation/test/fake_translator.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

namespace on_device_translation {
namespace {

using blink::mojom::ModelStreamingResponseStatus;

// Records the result that a `Translator` reports.
class TestResponder : public blink::mojom::ModelStreamingResponder {
 public:
  mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  // Waits until the translator reports completion or an error.
  void WaitForResult() { run_loop_.Run(); }

  const std::string& output() const { return output_; }
  const std::optional<ModelStreamingResponseStatus>& error() const {
    return error_;
  }
  const blink::mojom::QuotaErrorInfoPtr& quota_error_info() const {
    return quota_error_info_;
  }

  // blink::mojom::ModelStreamingResponder:
  void OnStreaming(const std::string& text) override { output_ += text; }
  void OnStreamingThought(const std::string& text) override {}
  void OnCompletion(
      blink::mojom::ModelExecutionContextInfoPtr context_info) override {
    run_loop_.Quit();
  }
  void OnError(ModelStreamingResponseStatus status,
               blink::mojom::QuotaErrorInfoPtr quota_error_info) override {
    error_ = status;
    quota_error_info_ = std::move(quota_error_info);
    run_loop_.Quit();
  }
  void OnToolCalls(std::vector<blink::mojom::ToolCallPtr> tool_calls) override {
  }
  void OnContextOverflow() override {}

 private:
  mojo::Receiver<blink::mojom::ModelStreamingResponder> receiver_{this};
  base::RunLoop run_loop_;
  std::string output_;
  std::optional<ModelStreamingResponseStatus> error_;
  blink::mojom::QuotaErrorInfoPtr quota_error_info_;
};

class TranslatorTest : public testing::Test {
 protected:
  void SetMaxInputSize(int max_input_size) {
    feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kTranslationAPI,
        {{"TranslationAPIMaxInputSizeBytes",
          base::NumberToString(max_input_size)}});
  }

  void CreateTranslator() {
    mojo::PendingRemote<mojom::OnDeviceTranslator> remote;
    fake_translator_.emplace(remote.InitWithNewPipeAndPassReceiver());
    translator_.emplace(base::BindRepeating([] { return true; }), "en", "es",
                        std::move(remote), /*input_budget=*/nullptr);
  }

  base::test::ScopedFeatureList feature_list_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::optional<FakeTranslator> fake_translator_;
  std::optional<Translator> translator_;
};

TEST_F(TranslatorTest, TranslateAcceptsInputAtTheLimit) {
  SetMaxInputSize(5);
  CreateTranslator();
  TestResponder responder;
  translator_->Translate("abcde", responder.BindNewPipeAndPassRemote());
  responder.WaitForResult();
  EXPECT_FALSE(responder.error());
  EXPECT_EQ(responder.output(), "Translated: abcde");
}

TEST_F(TranslatorTest, TranslateRejectsInputOverTheLimit) {
  base::HistogramTester histogram_tester;
  SetMaxInputSize(5);
  CreateTranslator();
  TestResponder responder;
  translator_->Translate("abcdef", responder.BindNewPipeAndPassRemote());
  responder.WaitForResult();
  EXPECT_EQ(responder.error(),
            ModelStreamingResponseStatus::kErrorInputTooLarge);
  ASSERT_TRUE(responder.quota_error_info());
  EXPECT_EQ(responder.quota_error_info()->requested, 6u);
  EXPECT_EQ(responder.quota_error_info()->quota, 5u);
  EXPECT_TRUE(responder.output().empty());
  histogram_tester.ExpectUniqueSample("Translate.TranslatorApi.TranslationLength",
                                      6, 1);
}

TEST_F(TranslatorTest, TranslateStreamingRejectsInputOverTheLimit) {
  SetMaxInputSize(5);
  CreateTranslator();
  TestResponder responder;
  translator_->TranslateStreaming("abc. def.",
                                  responder.BindNewPipeAndPassRemote());
  responder.WaitForResult();
  EXPECT_EQ(responder.error(),
            ModelStreamingResponseStatus::kErrorInputTooLarge);
}

TEST_F(TranslatorTest, NoLimitWhenTurnedOff) {
  SetMaxInputSize(0);
  CreateTranslator();
  TestResponder responder;
  translator_->Translate(std::string(1000, 'a'),
                         responder.BindNewPipeAndPassRemote());
  responder.WaitForResult();
  EXPECT_FALSE(responder.error());
}

// A translation service that holds each call until the test answers it.
class HoldingTranslator : public mojom::OnDeviceTranslator {
 public:
  explicit HoldingTranslator(
      mojo::PendingReceiver<mojom::OnDeviceTranslator> receiver)
      : receiver_(this, std::move(receiver)) {}
  // Closes the pipe before the held callbacks go away.
  ~HoldingTranslator() override { Disconnect(); }

  // mojom::OnDeviceTranslator:
  void Translate(const std::string& input,
                 TranslateCallback callback) override {
    translate_callbacks_.push_back(std::move(callback));
  }
  void SplitSentences(const std::string& input,
                      SplitSentencesCallback callback) override {
    if (!sentences_.empty()) {
      std::move(callback).Run(sentences_);
      return;
    }
    std::move(callback).Run({input});
  }

  void set_sentences(std::vector<std::string> sentences) {
    sentences_ = std::move(sentences);
  }

  size_t pending_count() const { return translate_callbacks_.size(); }

  // Answers the oldest call.
  void AnswerOne() {
    ASSERT_FALSE(translate_callbacks_.empty());
    TranslateCallback callback = std::move(translate_callbacks_.front());
    translate_callbacks_.erase(translate_callbacks_.begin());
    std::move(callback).Run("done");
  }

  void Disconnect() {
    receiver_.reset();
    translate_callbacks_.clear();
  }

 private:
  mojo::Receiver<mojom::OnDeviceTranslator> receiver_;
  std::vector<std::string> sentences_;
  std::vector<TranslateCallback> translate_callbacks_;
};

class TranslatorInputBudgetTest : public testing::Test {
 protected:
  void TearDown() override {
    translators_.clear();
    services_.clear();
    task_environment_.RunUntilIdle();
  }

  // Creates a translator that shares `budget_`, with its own service.
  void AddTranslator() {
    mojo::PendingRemote<mojom::OnDeviceTranslator> remote;
    services_.push_back(std::make_unique<HoldingTranslator>(
        remote.InitWithNewPipeAndPassReceiver()));
    translators_.push_back(
        std::make_unique<Translator>(base::BindRepeating([] { return true; }),
                                     "en", "es", std::move(remote), budget_));
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<InputBudget> budget_ =
      base::MakeRefCounted<InputBudget>(/*max_bytes=*/10,
                                        /*max_queued_bytes=*/12);
  std::vector<std::unique_ptr<HoldingTranslator>> services_;
  std::vector<std::unique_ptr<Translator>> translators_;
};

// Tests that the translators of an origin share the budget: a call that
// doesn't fit waits in the browser, and runs when an earlier result comes back.
TEST_F(TranslatorInputBudgetTest, CallWaitsForTheBudget) {
  AddTranslator();
  AddTranslator();
  TestResponder first;
  translators_[0]->Translate("abcdef", first.BindNewPipeAndPassRemote());
  TestResponder second;
  translators_[1]->Translate("ghijkl", second.BindNewPipeAndPassRemote());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(services_[0]->pending_count(), 1u);
  EXPECT_EQ(services_[1]->pending_count(), 0u);
  EXPECT_EQ(budget_->reserved_bytes(), 6u);
  EXPECT_EQ(budget_->queued_bytes(), 6u);

  services_[0]->AnswerOne();
  first.WaitForResult();
  EXPECT_FALSE(first.error());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(services_[1]->pending_count(), 1u);
  services_[1]->AnswerOne();
  second.WaitForResult();
  EXPECT_FALSE(second.error());
  EXPECT_EQ(budget_->reserved_bytes(), 0u);
}

// Tests that many calls sent at once all complete, in order, while the service
// never holds more than the budget.
TEST_F(TranslatorInputBudgetTest, ManyCallsCompleteInOrder) {
  budget_ = base::MakeRefCounted<InputBudget>(/*max_bytes=*/10,
                                              /*max_queued_bytes=*/100);
  AddTranslator();
  constexpr int kCalls = 8;
  std::vector<std::unique_ptr<TestResponder>> responders;
  for (int i = 0; i < kCalls; ++i) {
    responders.push_back(std::make_unique<TestResponder>());
    translators_[0]->Translate("abcdef",
                               responders.back()->BindNewPipeAndPassRemote());
  }
  for (int i = 0; i < kCalls; ++i) {
    task_environment_.RunUntilIdle();
    ASSERT_EQ(services_[0]->pending_count(), 1u);
    EXPECT_LE(budget_->reserved_bytes(), 10u);
    services_[0]->AnswerOne();
    responders[i]->WaitForResult();
    EXPECT_FALSE(responders[i]->error());
    EXPECT_EQ(responders[i]->output(), "done");
  }
  EXPECT_EQ(budget_->reserved_bytes(), 0u);
  EXPECT_EQ(budget_->queued_bytes(), 0u);
}

// Tests that a call fails when the input that waits in the browser would go
// over its limit.
TEST_F(TranslatorInputBudgetTest, FullQueueRejects) {
  AddTranslator();
  TestResponder first;
  translators_[0]->Translate("abcdef", first.BindNewPipeAndPassRemote());
  TestResponder second;
  translators_[0]->Translate("abcdef", second.BindNewPipeAndPassRemote());
  TestResponder third;
  translators_[0]->Translate("abcdef", third.BindNewPipeAndPassRemote());
  TestResponder rejected;
  translators_[0]->Translate("a", rejected.BindNewPipeAndPassRemote());
  rejected.WaitForResult();
  EXPECT_EQ(rejected.error(),
            ModelStreamingResponseStatus::kErrorGenericFailure);
  EXPECT_FALSE(rejected.quota_error_info());
  EXPECT_EQ(budget_->queued_bytes(), 12u);
}

// Tests that a streaming translation holds the budget until its last sentence
// returns.
TEST_F(TranslatorInputBudgetTest, StreamingHoldsTheBudget) {
  AddTranslator();
  TestResponder streaming;
  translators_[0]->TranslateStreaming("abcdefgh",
                                      streaming.BindNewPipeAndPassRemote());
  task_environment_.RunUntilIdle();
  ASSERT_EQ(services_[0]->pending_count(), 1u);
  EXPECT_EQ(budget_->reserved_bytes(), 8u);

  TestResponder waiting;
  translators_[0]->Translate("abc", waiting.BindNewPipeAndPassRemote());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(services_[0]->pending_count(), 1u);

  services_[0]->AnswerOne();
  streaming.WaitForResult();
  EXPECT_FALSE(streaming.error());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(services_[0]->pending_count(), 1u);
  EXPECT_EQ(budget_->reserved_bytes(), 3u);
}

// Tests that calls the service drops give the budget back.
TEST_F(TranslatorInputBudgetTest, DroppedCallsGiveTheBudgetBack) {
  AddTranslator();
  TestResponder first;
  translators_[0]->Translate("abcdef", first.BindNewPipeAndPassRemote());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(budget_->reserved_bytes(), 6u);
  services_[0]->Disconnect();
  first.WaitForResult();
  EXPECT_EQ(first.error(), ModelStreamingResponseStatus::kErrorGenericFailure);
  EXPECT_EQ(budget_->reserved_bytes(), 0u);
}

// Tests that a streaming translation dropped by the service rejects and
// releases its input budget reservation.
TEST_F(TranslatorInputBudgetTest, DroppedStreamingCallGivesTheBudgetBack) {
  AddTranslator();
  TestResponder streaming;
  translators_[0]->TranslateStreaming("abcdefgh",
                                      streaming.BindNewPipeAndPassRemote());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(budget_->reserved_bytes(), 8u);
  services_[0]->Disconnect();
  streaming.WaitForResult();
  EXPECT_EQ(streaming.error(),
            ModelStreamingResponseStatus::kErrorGenericFailure);
  EXPECT_EQ(budget_->reserved_bytes(), 0u);
}

// Tests that destroying a translator while the service still works on a call
// keeps the budget reserved until the service replies.
TEST_F(TranslatorInputBudgetTest,
       DestroyedTranslatorHoldsBudgetUntilServiceReplies) {
  AddTranslator();
  TestResponder first;
  translators_[0]->Translate("abcdef", first.BindNewPipeAndPassRemote());
  task_environment_.RunUntilIdle();
  ASSERT_EQ(services_[0]->pending_count(), 1u);
  EXPECT_EQ(budget_->reserved_bytes(), 6u);

  translators_[0].reset();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(budget_->reserved_bytes(), 6u);

  services_[0]->AnswerOne();
  first.WaitForResult();
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(first.error());
  EXPECT_EQ(budget_->reserved_bytes(), 0u);
}

// Tests that dropping the streaming responder after one sentence keeps the
// budget reserved until every sentence sent to the service replies.
TEST_F(TranslatorInputBudgetTest,
       AbortedStreamHoldsBudgetUntilRemainingSentencesReply) {
  AddTranslator();
  services_[0]->set_sentences({"abcd", "efgh"});
  auto streaming = std::make_unique<TestResponder>();
  translators_[0]->TranslateStreaming("abcdefgh",
                                      streaming->BindNewPipeAndPassRemote());
  task_environment_.RunUntilIdle();
  ASSERT_EQ(services_[0]->pending_count(), 2u);
  EXPECT_EQ(budget_->reserved_bytes(), 8u);

  services_[0]->AnswerOne();
  task_environment_.RunUntilIdle();
  ASSERT_EQ(services_[0]->pending_count(), 1u);
  EXPECT_EQ(budget_->reserved_bytes(), 8u);

  streaming.reset();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(budget_->reserved_bytes(), 8u);

  services_[0]->AnswerOne();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(budget_->reserved_bytes(), 0u);
}

// Tests that the waiting call of a translator that goes away gives up its
// place.
TEST_F(TranslatorInputBudgetTest, DestroyedTranslatorGivesUpItsPlace) {
  AddTranslator();
  AddTranslator();
  TestResponder first;
  translators_[0]->Translate("abcdef", first.BindNewPipeAndPassRemote());
  TestResponder second;
  translators_[1]->Translate("ghijkl", second.BindNewPipeAndPassRemote());
  translators_[1].reset();
  task_environment_.RunUntilIdle();
  services_[0]->AnswerOne();
  first.WaitForResult();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(budget_->reserved_bytes(), 0u);
  EXPECT_EQ(budget_->queued_bytes(), 0u);
  EXPECT_EQ(services_[1]->pending_count(), 0u);
}

// Tests that input with nothing to translate doesn't use the budget.
TEST_F(TranslatorInputBudgetTest, BlankInputDoesNotUseTheBudget) {
  AddTranslator();
  TestResponder responder;
  translators_[0]->Translate(std::string(20, ' '),
                             responder.BindNewPipeAndPassRemote());
  responder.WaitForResult();
  EXPECT_FALSE(responder.error());
  EXPECT_EQ(budget_->reserved_bytes(), 0u);
}

}  // namespace
}  // namespace on_device_translation
