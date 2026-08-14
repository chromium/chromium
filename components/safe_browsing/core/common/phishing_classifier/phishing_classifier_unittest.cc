// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/common/phishing_classifier/phishing_classifier.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/safe_browsing/core/common/fbs/client_model_generated.h"
#include "components/safe_browsing/core/common/phishing_classifier/scorer.h"
#include "components/safe_browsing/core/common/proto/client_model.pb.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace safe_browsing {

namespace {

using ::testing::_;

// Mock implementation of Scorer used to intercept and verify calls to the
// visual TfLite models without executing the real model logic.
class MockScorer : public Scorer {
 public:
  MockScorer() = default;
  ~MockScorer() override = default;

  MOCK_METHOD(void,
              ApplyVisualTfLiteModel,
              (const gfx::Image& image,
               base::OnceCallback<void(std::vector<double>)> callback),
              (const, override));

  MOCK_METHOD(void,
              ApplyVisualTfLiteModelImageEmbedding,
              (const gfx::Image& image,
               base::OnceCallback<void(ImageFeatureEmbedding)> callback),
              (const, override));
};

std::string GetFlatBufferString() {
  flatbuffers::FlatBufferBuilder builder(1024);

  std::vector<flatbuffers::Offset<flat::TfLiteModelMetadata_::Threshold>>
      thresholds_vector;
  flatbuffers::Offset<flat::TfLiteModelMetadata> tflite_metadata_flat =
      flat::CreateTfLiteModelMetadataDirect(builder, 0, &thresholds_vector, 48,
                                            48);
  flat::ClientSideModelBuilder csd_model_builder(builder);
  csd_model_builder.add_tflite_metadata(tflite_metadata_flat);
  csd_model_builder.add_dom_model_version(123);
  builder.Finish(csd_model_builder.Finish());

  return std::string(reinterpret_cast<char*>(builder.GetBufferPointer()),
                     builder.GetSize());
}

}  // namespace

class PhishingClassifierTest : public testing::Test {
 public:
  PhishingClassifierTest() {
    std::string model_str = GetFlatBufferString();
    base::MappedReadOnlyRegion mapped_region =
        base::ReadOnlySharedMemoryRegion::Create(model_str.length());
    mapped_region.mapping.GetMemoryAsSpan<char>().copy_from(model_str);
    classifier_ = std::make_unique<PhishingClassifier>();

    auto scorer =
        Scorer::Create(mapped_region.region.Duplicate(), base::File());
#if BUILDFLAG(IS_IOS)
    classifier_->set_scorer(scorer.get());
    scorer_ = std::move(scorer);
#else
    ScorerStorage::GetInstance()->SetScorer(std::move(scorer));
#endif
  }

  void TearDown() override {
    classifier_.reset();
#if !BUILDFLAG(IS_IOS)
    ScorerStorage::GetInstance()->SetScorer(nullptr);
#endif
  }

  void ClearScorer() {
#if BUILDFLAG(IS_IOS)
    classifier_->set_scorer(nullptr);
#else
    ScorerStorage::GetInstance()->SetScorer(nullptr);
#endif
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<PhishingClassifier> classifier_;
#if BUILDFLAG(IS_IOS)
  std::unique_ptr<Scorer> scorer_;
#endif
};

TEST_F(PhishingClassifierTest, Classification) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(48, 48);
  bitmap.eraseColor(SK_ColorWHITE);

  GURL url("http://test.com");

  base::test::TestFuture<const ClientPhishingRequest&,
                         PhishingClassifier::Result>
      test_future;
  classifier_->BeginClassification(url, gfx::Image::CreateFrom1xBitmap(bitmap),
                                   test_future.GetCallback());

  const ClientPhishingRequest& verdict = test_future.Get<0>();
  PhishingClassifier::Result result = test_future.Get<1>();

  ASSERT_EQ(result, PhishingClassifier::Result::kSuccess);
  EXPECT_EQ(verdict.url(), "http://test.com/");
  EXPECT_EQ(verdict.client_score(), 0.0f);
}

TEST_F(PhishingClassifierTest, ClassificationWithImageEmbedding) {
  // Inject a MockScorer to intercept model application calls.
  auto mock_scorer = std::make_unique<MockScorer>();
  MockScorer* raw_mock_scorer = mock_scorer.get();
#if BUILDFLAG(IS_IOS)
  classifier_->set_scorer(raw_mock_scorer);
  scorer_ = std::move(mock_scorer);
#else
  ScorerStorage::GetInstance()->SetScorer(std::move(mock_scorer));
#endif

  SkBitmap bitmap;
  bitmap.allocN32Pixels(48, 48);
  bitmap.eraseColor(SK_ColorWHITE);

  GURL url("http://test.com");

  // Configure the classifier to perform image embedding matching.
  classifier_->SetClientSideDetectionType(
      safe_browsing::ClientSideDetectionType::IMAGE_EMBEDDING_MATCH);

  // The standard classification flow always applies the visual model first.
  // We simulate a successful but empty result.
  EXPECT_CALL(*raw_mock_scorer, ApplyVisualTfLiteModel(_, _))
      .WillOnce([](const gfx::Image& image,
                   base::OnceCallback<void(std::vector<double>)> callback) {
        std::move(callback).Run({});
      });

  // Prepare a fake embedding to be returned by the mock.
  ImageFeatureEmbedding expected_embedding;
  expected_embedding.add_embedding_value(0.5);
  expected_embedding.add_embedding_value(0.6);

  // Because the type is IMAGE_EMBEDDING_MATCH, the classifier should
  // subsequently apply the image embedding model. We simulate returning our
  // fake embedding.
  EXPECT_CALL(*raw_mock_scorer, ApplyVisualTfLiteModelImageEmbedding(_, _))
      .WillOnce([expected_embedding](
                    const gfx::Image& image,
                    base::OnceCallback<void(ImageFeatureEmbedding)> callback) {
        std::move(callback).Run(expected_embedding);
      });

  base::test::TestFuture<const ClientPhishingRequest&,
                         PhishingClassifier::Result>
      test_future;
  classifier_->BeginClassification(url, gfx::Image::CreateFrom1xBitmap(bitmap),
                                   test_future.GetCallback());

  const ClientPhishingRequest& verdict = test_future.Get<0>();
  PhishingClassifier::Result result = test_future.Get<1>();

  ASSERT_EQ(result, PhishingClassifier::Result::kSuccess);
  EXPECT_EQ(verdict.url(), "http://test.com/");
  EXPECT_TRUE(verdict.has_image_feature_embedding());
  ASSERT_EQ(verdict.image_feature_embedding().embedding_value_size(), 2);
  EXPECT_EQ(verdict.image_feature_embedding().embedding_value(0), 0.5f);
  EXPECT_EQ(verdict.image_feature_embedding().embedding_value(1), 0.6f);
}

// Verifies that cancellation does not affect classifier readiness, and that
// `is_ready()` correctly tracks the presence of a scorer.
TEST_F(PhishingClassifierTest, IsReadyAndCancel) {
  EXPECT_TRUE(classifier_->is_ready());
  classifier_->CancelPendingClassification();
  EXPECT_TRUE(classifier_->is_ready());

  ClearScorer();
  EXPECT_FALSE(classifier_->is_ready());
}

TEST_F(PhishingClassifierTest, CancelInFlight) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(48, 48);
  bitmap.eraseColor(SK_ColorWHITE);

  GURL url("http://test.com");

  base::test::TestFuture<const ClientPhishingRequest&,
                         PhishingClassifier::Result>
      test_future;
  classifier_->BeginClassification(url, gfx::Image::CreateFrom1xBitmap(bitmap),
                                   test_future.GetCallback());

  // Cancel immediately. This should invalidate the weak pointer for
  // OnVisualFeaturesExtracted.
  classifier_->CancelPendingClassification();

  // Wait for all background tasks to complete.
  base::ThreadPoolInstance::Get()->FlushForTesting();

  // Post a sentinel task to the end of the main thread queue, then wait for it
  // to run to ensure any pending replies on the main thread are processed.
  bool sentinel_run = false;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce([](bool* flag) { *flag = true; }, &sentinel_run));
  EXPECT_TRUE(base::test::RunUntil([&sentinel_run]() { return sentinel_run; }));

  // The callback should NOT have been run.
  EXPECT_FALSE(test_future.IsReady());
}

}  // namespace safe_browsing
