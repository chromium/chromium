// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/on_device_translation/translator.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
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
                        std::move(remote));
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

}  // namespace
}  // namespace on_device_translation
