// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/common/phishing_classifier/phishing_classifier.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/safe_browsing/core/common/fbs/client_model_generated.h"
#include "components/safe_browsing/core/common/phishing_classifier/scorer.h"
#include "components/safe_browsing/core/common/phishing_classifier/test_utils.h"
#include "components/safe_browsing/core/common/proto/client_model.pb.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace safe_browsing {

namespace {

using ::testing::_;

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

    scorer_ = Scorer::Create(mapped_region.region.Duplicate(), base::File());
    classifier_->set_scorer(scorer_.get());
  }

  void TearDown() override { classifier_.reset(); }

  void ClearScorer() { classifier_->set_scorer(nullptr); }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<PhishingClassifier> classifier_;
  std::unique_ptr<Scorer> scorer_;
};

#if !BUILDFLAG(IS_IOS)
// In renderer processes, `PhishingClassifier` typically relies on the
// process-wide `ScorerStorage` to obtain the model. In the browser process (or
// on iOS), the scorer is injected explicitly for profile isolation. This test
// ensures that once a scorer is explicitly set via `set_scorer()`,
// `PhishingClassifier` locks to that state and stops consulting the global
// `ScorerStorage`.
TEST_F(PhishingClassifierTest, UsesScorerStorageUntilScorerIsInjected) {
  base::ScopedClosureRunner cleanup_storage(
      base::BindOnce([] { ScorerStorage::GetInstance()->ClearScorer(); }));
  // Other tests in this process may share the `ScorerStorage` singleton.
  ScorerStorage::GetInstance()->SetScorer(nullptr);
  PhishingClassifier classifier;
  EXPECT_FALSE(classifier.is_ready());

  ScorerStorage::GetInstance()->SetScorer(std::make_unique<Scorer>());
  EXPECT_TRUE(classifier.is_ready());

  classifier.set_scorer(nullptr);
  EXPECT_FALSE(classifier.is_ready());

  classifier.set_scorer(scorer_.get());
  EXPECT_TRUE(classifier.is_ready());

  ScorerStorage::GetInstance()->SetScorer(nullptr);
  EXPECT_TRUE(classifier.is_ready());
}
#endif  // !BUILDFLAG(IS_IOS)

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
  classifier_->set_scorer(raw_mock_scorer);
  scorer_ = std::move(mock_scorer);

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
