// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/common/phishing_classifier/phishing_image_embedder.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "components/safe_browsing/core/common/fbs/client_model_generated.h"
#include "components/safe_browsing/core/common/phishing_classifier/scorer.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"

namespace safe_browsing {

namespace {

using ::testing::_;

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
constexpr int kExpectedVisualWidth = 18;
constexpr int kExpectedVisualHeight = 32;
#else
constexpr int kExpectedVisualWidth = 48;
constexpr int kExpectedVisualHeight = 48;
#endif
constexpr size_t kExpectedVisualDataSize =
    3u * kExpectedVisualWidth * kExpectedVisualHeight;

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

class PhishingImageEmbedderTest : public testing::Test {
 public:
  PhishingImageEmbedderTest() {
    std::string model_str = GetFlatBufferString();
    base::MappedReadOnlyRegion mapped_region =
        base::ReadOnlySharedMemoryRegion::Create(model_str.length());
    mapped_region.mapping.GetMemoryAsSpan<char>().copy_from(model_str);
    image_embedder_ = std::make_unique<PhishingImageEmbedder>();

    auto scorer =
        Scorer::Create(mapped_region.region.Duplicate(), base::File());
#if BUILDFLAG(IS_IOS)
    image_embedder_->set_scorer(scorer.get());
    scorer_ = std::move(scorer);
#else
    ScorerStorage::GetInstance()->SetScorer(std::move(scorer));
#endif
  }

  void TearDown() override {
    image_embedder_.reset();
#if !BUILDFLAG(IS_IOS)
    ScorerStorage::GetInstance()->SetScorer(nullptr);
#endif
  }

  void ClearScorer() {
#if BUILDFLAG(IS_IOS)
    image_embedder_->set_scorer(nullptr);
#else
    ScorerStorage::GetInstance()->SetScorer(nullptr);
#endif
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<PhishingImageEmbedder> image_embedder_;
#if BUILDFLAG(IS_IOS)
  std::unique_ptr<Scorer> scorer_;
#endif
};

TEST_F(PhishingImageEmbedderTest, NoImageEmbeddingWithVisualFeatures) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(48, 48);
  bitmap.eraseColor(SK_ColorWHITE);

  base::test::TestFuture<PhishingImageEmbedder::Result,
                         const ImageFeatureEmbedding&, const VisualFeatures&>
      future;

  image_embedder_->BeginImageEmbedding(gfx::Image::CreateFrom1xBitmap(bitmap),
                                       /*can_extract_visual_features=*/true,
                                       future.GetCallback());

  // Since we didn't attach a valid image embedding model to the scorer,
  // ApplyVisualTfLiteModelImageEmbedding will return an empty embedding.
  // However, visual feature extraction will still succeed on the bitmap,
  // and the callback should be invoked.

  auto result = future.Get<0>();
  ASSERT_EQ(result, PhishingImageEmbedder::Result::kSuccess);

  const auto& embedding = future.Get<1>();
  // Expect empty embedding because no model was loaded.
  EXPECT_EQ(embedding.embedding_value_size(), 0);

  const auto& visual_features = future.Get<2>();
  // Expect visual features to be extracted.
  ASSERT_TRUE(visual_features.has_image());
  EXPECT_EQ(visual_features.image().width(), kExpectedVisualWidth);
  EXPECT_EQ(visual_features.image().height(), kExpectedVisualHeight);
  EXPECT_EQ(visual_features.image().data().size(), kExpectedVisualDataSize);
}

TEST_F(PhishingImageEmbedderTest, NoImageEmbeddingOrVisualFeatures) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(48, 48);
  bitmap.eraseColor(SK_ColorWHITE);

  base::test::TestFuture<PhishingImageEmbedder::Result,
                         const ImageFeatureEmbedding&, const VisualFeatures&>
      future;

  image_embedder_->BeginImageEmbedding(gfx::Image::CreateFrom1xBitmap(bitmap),
                                       /*can_extract_visual_features=*/false,
                                       future.GetCallback());

  auto result = future.Get<0>();
  ASSERT_EQ(result, PhishingImageEmbedder::Result::kSuccess);

  // Since we didn't attach a valid image embedding model to the scorer,
  // ApplyVisualTfLiteModelImageEmbedding will return an empty embedding.
  const auto& embedding = future.Get<1>();
  EXPECT_EQ(embedding.embedding_value_size(), 0);

  // Since we passed false for `can_extract_visual_features`, visual features
  // should not be extracted.
  const auto& visual_features = future.Get<2>();
  EXPECT_FALSE(visual_features.has_image());
}

TEST_F(PhishingImageEmbedderTest, ImageEmbeddingWithMockScorer) {
  auto mock_scorer = std::make_unique<MockScorer>();
  MockScorer* raw_mock_scorer = mock_scorer.get();
#if BUILDFLAG(IS_IOS)
  image_embedder_->set_scorer(raw_mock_scorer);
  scorer_ = std::move(mock_scorer);
#else
  ScorerStorage::GetInstance()->SetScorer(std::move(mock_scorer));
#endif

  SkBitmap bitmap;
  bitmap.allocN32Pixels(48, 48);
  bitmap.eraseColor(SK_ColorWHITE);

  EXPECT_CALL(*raw_mock_scorer, ApplyVisualTfLiteModelImageEmbedding(_, _))
      .WillOnce([](const gfx::Image& image,
                   base::OnceCallback<void(ImageFeatureEmbedding)> callback) {
        ImageFeatureEmbedding embedding;
        embedding.add_embedding_value(0.5f);
        embedding.add_embedding_value(0.6f);
        std::move(callback).Run(embedding);
      });

  base::test::TestFuture<PhishingImageEmbedder::Result,
                         const ImageFeatureEmbedding&, const VisualFeatures&>
      future;

  image_embedder_->BeginImageEmbedding(gfx::Image::CreateFrom1xBitmap(bitmap),
                                       /*can_extract_visual_features=*/true,
                                       future.GetCallback());

  auto result = future.Get<0>();
  ASSERT_EQ(result, PhishingImageEmbedder::Result::kSuccess);

  const auto& embedding = future.Get<1>();
  ASSERT_EQ(embedding.embedding_value_size(), 2);
  EXPECT_EQ(embedding.embedding_value(0), 0.5f);
  EXPECT_EQ(embedding.embedding_value(1), 0.6f);

  const auto& visual_features = future.Get<2>();
  ASSERT_TRUE(visual_features.has_image());
  EXPECT_EQ(visual_features.image().width(), kExpectedVisualWidth);
  EXPECT_EQ(visual_features.image().height(), kExpectedVisualHeight);
  EXPECT_EQ(visual_features.image().data().size(), kExpectedVisualDataSize);
}

// Verifies that cancellation does not affect embedder readiness, and that
// `is_ready()` correctly tracks the presence of a scorer.
TEST_F(PhishingImageEmbedderTest, IsReadyAndCancel) {
  EXPECT_TRUE(image_embedder_->is_ready());
  image_embedder_->CancelPendingImageEmbedding();
  EXPECT_TRUE(image_embedder_->is_ready());

  ClearScorer();
  EXPECT_FALSE(image_embedder_->is_ready());
}

TEST_F(PhishingImageEmbedderTest, CancelInFlight) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(48, 48);
  bitmap.eraseColor(SK_ColorWHITE);

  base::test::TestFuture<PhishingImageEmbedder::Result,
                         const ImageFeatureEmbedding&, const VisualFeatures&>
      future;

  image_embedder_->BeginImageEmbedding(gfx::Image::CreateFrom1xBitmap(bitmap),
                                       /*can_extract_visual_features=*/true,
                                       future.GetCallback());

  // Cancel immediately. This should invalidate the weak pointer for
  // callback execution.
  image_embedder_->CancelPendingImageEmbedding();

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
  EXPECT_FALSE(future.IsReady());
}

}  // namespace safe_browsing
