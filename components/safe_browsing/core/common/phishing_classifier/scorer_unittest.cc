// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/common/phishing_classifier/scorer.h"

#include <stdint.h>

#include <memory>
#include <unordered_set>

#include "base/compiler_specific.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "base/test/test_future.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "components/safe_browsing/core/common/fbs/client_model_generated.h"
#include "components/safe_browsing/core/common/phishing_classifier/features.h"
#include "components/safe_browsing/core/common/proto/client_model.pb.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "ui/gfx/image/image.h"

namespace safe_browsing {

namespace {

// Input tensor dimensions for the test visual classification (`visual_model_*`)
// and image embedding (`image_embedding.tflite`) TFLite models.
constexpr int kVisualClassificationInputWidth = 48;
constexpr int kVisualClassificationInputHeight = 48;
constexpr int kImageEmbeddingInputWidth = 289;
constexpr int kImageEmbeddingInputHeight = 289;

std::string GetFlatBufferString(bool include_tflite_metadata = true) {
  flatbuffers::FlatBufferBuilder builder(1024);
  std::vector<flatbuffers::Offset<flat::Hash>> hashes;
  // Make sure this is sorted.
  std::vector<std::string> hashes_vector = {"feature1", "feature2", "feature3",
                                            "token one", "token two"};
  for (std::string& feature : hashes_vector) {
    std::vector<uint8_t> hash_data(feature.begin(), feature.end());
    hashes.push_back(flat::CreateHashDirect(builder, &hash_data));
  }
  flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<flat::Hash>>>
      hashes_flat = builder.CreateVector(hashes);

  std::vector<flatbuffers::Offset<flat::ClientSideModel_::Rule>> rules;
  std::vector<int32_t> rule_feature1 = {};
  std::vector<int32_t> rule_feature2 = {0};
  std::vector<int32_t> rule_feature3 = {0, 1};
  rules.push_back(
      flat::ClientSideModel_::CreateRuleDirect(builder, &rule_feature1, 0.5));
  rules.push_back(
      flat::ClientSideModel_::CreateRuleDirect(builder, &rule_feature2, 2));
  rules.push_back(
      flat::ClientSideModel_::CreateRuleDirect(builder, &rule_feature3, 3));
  flatbuffers::Offset<
      flatbuffers::Vector<flatbuffers::Offset<flat::ClientSideModel_::Rule>>>
      rules_flat = builder.CreateVector(rules);

  std::vector<int32_t> page_terms_vector = {3, 4};
  flatbuffers::Offset<flatbuffers::Vector<int32_t>> page_term_flat =
      builder.CreateVector(page_terms_vector);

  std::vector<uint32_t> page_words_vector = {1000U, 2000U, 3000U};
  flatbuffers::Offset<flatbuffers::Vector<uint32_t>> page_word_flat =
      builder.CreateVector(page_words_vector);

  std::vector<flatbuffers::Offset<flat::TfLiteModelMetadata_::Threshold>>
      thresholds_vector = {};
  flatbuffers::Offset<flat::TfLiteModelMetadata> tflite_metadata_flat =
      flat::CreateTfLiteModelMetadataDirect(builder, 0, &thresholds_vector, 0,
                                            0);

  flat::ClientSideModelBuilder csd_model_builder(builder);
  csd_model_builder.add_hashes(hashes_flat);
  csd_model_builder.add_rule(rules_flat);
  csd_model_builder.add_page_term(page_term_flat);
  csd_model_builder.add_page_word(page_word_flat);
  csd_model_builder.add_max_words_per_term(2);
  csd_model_builder.add_murmur_hash_seed(12345U);
  csd_model_builder.add_max_shingles_per_page(10);
  csd_model_builder.add_shingle_size(3);
  if (include_tflite_metadata) {
    csd_model_builder.add_tflite_metadata(tflite_metadata_flat);
  }
  csd_model_builder.add_dom_model_version(123);

  builder.Finish(csd_model_builder.Finish());
  return std::string(reinterpret_cast<char*>(builder.GetBufferPointer()),
                     builder.GetSize());
}

base::MappedReadOnlyRegion GetMappedReadOnlyRegionWithData(std::string data) {
  base::MappedReadOnlyRegion mapped_region =
      base::ReadOnlySharedMemoryRegion::Create(data.length());
  EXPECT_TRUE(mapped_region.IsValid());
  mapped_region.mapping.GetMemoryAsSpan<char>().copy_from(data);
  return mapped_region;
}

}  // namespace

class PhishingScorerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Setup a simple model.  Note that the scorer does not care about
    // how features are encoded so we use readable strings here to make
    // the test simpler to follow.
    model_.Clear();
    model_.add_hashes("feature1");
    model_.add_hashes("feature2");
    model_.add_hashes("feature3");
    model_.add_hashes("token one");
    model_.add_hashes("token two");

    ClientSideModel::Rule* rule;
    rule = model_.add_rule();
    rule->set_weight(0.5);

    rule = model_.add_rule();
    rule->add_feature(0);  // feature1
    rule->set_weight(2.0);

    rule = model_.add_rule();
    rule->add_feature(0);  // feature1
    rule->add_feature(1);  // feature2
    rule->set_weight(3.0);

    model_.add_page_term(3);  // token one
    model_.add_page_term(4);  // token two

    // These will be murmur3 hashes, but for this test it's not necessary
    // that the hashes correspond to actual words.
    model_.add_page_word(1000U);
    model_.add_page_word(2000U);
    model_.add_page_word(3000U);

    model_.set_max_words_per_term(2);
    model_.set_murmur_hash_seed(12345U);
    model_.set_max_shingles_per_page(10);
    model_.set_shingle_size(3);
    model_.set_dom_model_version(123);
  }

  void TearDown() override {
    base::DiscardableMemoryAllocator::SetInstance(nullptr);
  }

  base::test::TaskEnvironment task_environment_;
  ClientSideModel model_;
};

TEST_F(PhishingScorerTest, HasValidFlatBufferModel) {
  std::unique_ptr<Scorer> scorer;
  std::string flatbuffer = GetFlatBufferString();
  base::MappedReadOnlyRegion mapped_region =
      GetMappedReadOnlyRegionWithData(flatbuffer);
  scorer = Scorer::Create(mapped_region.region.Duplicate(), base::File());
  EXPECT_TRUE(scorer.get() != nullptr);

  // Missing TfLite metadata.
  flatbuffer = GetFlatBufferString(/*include_tflite_metadata=*/false);
  mapped_region = GetMappedReadOnlyRegionWithData(flatbuffer);
  scorer = Scorer::Create(mapped_region.region.Duplicate(), base::File());
  EXPECT_FALSE(scorer.get());

  // Invalid region.
  scorer = Scorer::Create(base::ReadOnlySharedMemoryRegion(), base::File());
  EXPECT_FALSE(scorer.get());

  // Invalid buffer in region.
  mapped_region = GetMappedReadOnlyRegionWithData("bogus string");
  scorer = Scorer::Create(mapped_region.region.Duplicate(), base::File());
  EXPECT_FALSE(scorer.get());
}

TEST_F(PhishingScorerTest, ImageEmbeddingModelDefensiveChecks) {
  // Verify CreateScorerWithImageEmbeddingModel handles invalid region/null
  // scorer gracefully.
  std::unique_ptr<Scorer> scorer = Scorer::CreateScorerWithImageEmbeddingModel(
      base::ReadOnlySharedMemoryRegion(), base::File(), base::File());
  EXPECT_FALSE(scorer);

  // Verify AttachImageEmbeddingModel handles null flatbuffer_model_
  // gracefully.
  scorer = Scorer::Create(/*classification_input_width=*/0,
                          /*classification_input_height=*/0, base::File());
  ASSERT_TRUE(scorer);
  EXPECT_FALSE(scorer->HasVisualTfLiteModel());
  EXPECT_EQ(0, scorer->image_embedding_tflite_model_version());

  // This should not crash despite flatbuffer_model_ being null.
  scorer->AttachImageEmbeddingModel(base::File());

  // Verify AttachImageEmbeddingModel and image_embedding_tflite_model_version
  // handle a valid flatbuffer_model_ that lacks img_embedding_metadata
  // gracefully.
  std::string flatbuffer = GetFlatBufferString();
  base::MappedReadOnlyRegion mapped_region =
      GetMappedReadOnlyRegionWithData(flatbuffer);
  scorer = Scorer::Create(mapped_region.region.Duplicate(), base::File());
  ASSERT_TRUE(scorer);
  EXPECT_EQ(0, scorer->image_embedding_tflite_model_version());

  // This should not crash despite img_embedding_metadata being null in the
  // flatbuffer model.
  scorer->AttachImageEmbeddingModel(base::File());
}

// Tests that `ApplyVisualTfLiteModel()` resizes the input image to the model's
// configured classification dimensions and produces classification scores.
TEST_F(PhishingScorerTest, ApplyVisualTfLiteModel) {
  base::FilePath tflite_path;
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &tflite_path));
  tflite_path = tflite_path.AppendASCII("components")
                    .AppendASCII("test")
                    .AppendASCII("data")
                    .AppendASCII("safe_browsing")
#if BUILDFLAG(IS_IOS)
                    .AppendASCII("visual_model_ios.tflite");
#elif BUILDFLAG(IS_ANDROID)
                    .AppendASCII("visual_model_android.tflite");
#else
                    .AppendASCII("visual_model_desktop.tflite");
#endif

  base::File tflite_model(tflite_path,
                          base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(tflite_model.IsValid());

  std::unique_ptr<Scorer> scorer =
      Scorer::Create(kVisualClassificationInputWidth,
                     kVisualClassificationInputHeight, std::move(tflite_model));
  ASSERT_TRUE(scorer);

  // Use a source image larger than the model input dimensions so that
  // `ResizeImageForModel()` actively resizes the image.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(kVisualClassificationInputWidth * 3,
                        kVisualClassificationInputHeight * 3);
  bitmap.eraseColor(SK_ColorWHITE);

  base::test::TestFuture<std::vector<double>> future;
  scorer->ApplyVisualTfLiteModel(gfx::Image::CreateFrom1xBitmap(bitmap),
                                 future.GetCallback());
  EXPECT_FALSE(future.Get().empty());
}

// Tests that `ApplyVisualTfLiteModelImageEmbedding()` resizes the input image
// to the model's configured embedding dimensions and produces an image
// embedding.
TEST_F(PhishingScorerTest, ApplyVisualTfLiteModelImageEmbedding) {
  base::FilePath img_embedding_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT,
                                     &img_embedding_path));
  img_embedding_path = img_embedding_path.AppendASCII("components")
                           .AppendASCII("test")
                           .AppendASCII("data")
                           .AppendASCII("safe_browsing")
                           .AppendASCII("image_embedding.tflite");

  base::File img_embedding_model(img_embedding_path,
                                 base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(img_embedding_model.IsValid());

  std::unique_ptr<Scorer> scorer = Scorer::CreateScorerWithImageEmbeddingModel(
      kVisualClassificationInputWidth, kVisualClassificationInputHeight,
      base::File(), kImageEmbeddingInputWidth, kImageEmbeddingInputHeight,
      std::move(img_embedding_model));
  ASSERT_TRUE(scorer);

  // Use a source image larger than the model input dimensions so that
  // `ResizeImageForModel()` actively resizes the image.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(kImageEmbeddingInputWidth * 3,
                        kImageEmbeddingInputHeight * 3);
  bitmap.eraseColor(SK_ColorWHITE);

  base::test::TestFuture<ImageFeatureEmbedding> future;
  scorer->ApplyVisualTfLiteModelImageEmbedding(
      gfx::Image::CreateFrom1xBitmap(bitmap), future.GetCallback());
  EXPECT_GT(future.Get().embedding_value_size(), 0);
}

}  // namespace safe_browsing
