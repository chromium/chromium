// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/phishing_classifier/scorer.h"

#include <stdint.h>

#include <memory>
#include <unordered_set>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "base/threading/thread.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/features.h"
#include "components/safe_browsing/core/common/fbs/client_model_generated.h"
#include "components/safe_browsing/core/common/proto/client_model.pb.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColorSpace.h"

namespace safe_browsing {

namespace {

std::string GetFlatBufferString() {
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
  csd_model_builder.add_tflite_metadata(tflite_metadata_flat);
  csd_model_builder.add_dom_model_version(123);

  builder.Finish(csd_model_builder.Finish());
  return std::string(reinterpret_cast<char*>(builder.GetBufferPointer()),
                     builder.GetSize());
}

base::MappedReadOnlyRegion GetMappedReadOnlyRegionWithData(std::string data) {
  base::MappedReadOnlyRegion mapped_region =
      base::ReadOnlySharedMemoryRegion::Create(data.length());
  EXPECT_TRUE(mapped_region.IsValid());
  memcpy(mapped_region.mapping.memory(), data.data(), data.length());
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

  ClientSideModel model_;
};

TEST_F(PhishingScorerTest, HasValidFlatBufferModel) {
  std::unique_ptr<Scorer> scorer;
  std::string flatbuffer = GetFlatBufferString();
  base::MappedReadOnlyRegion mapped_region =
      GetMappedReadOnlyRegionWithData(flatbuffer);
  scorer = Scorer::Create(mapped_region.region.Duplicate(), base::File());
  EXPECT_TRUE(scorer.get() != nullptr);

  // Invalid region.
  scorer = Scorer::Create(base::ReadOnlySharedMemoryRegion(), base::File());
  EXPECT_FALSE(scorer.get());

  // Invalid buffer in region.
  mapped_region = GetMappedReadOnlyRegionWithData("bogus string");
  scorer = Scorer::Create(mapped_region.region.Duplicate(), base::File());
  EXPECT_FALSE(scorer.get());
}

TEST_F(PhishingScorerTest, PageTerms) {
  std::unique_ptr<Scorer> scorer;
  std::string flatbuffer = GetFlatBufferString();
  base::MappedReadOnlyRegion mapped_region =
      GetMappedReadOnlyRegionWithData(flatbuffer);
  scorer = Scorer::Create(mapped_region.region.Duplicate(), base::File());
  ASSERT_TRUE(scorer.get());
  base::RepeatingCallback<bool(const std::string&)> page_terms_callback(
      scorer->find_page_term_callback());
  EXPECT_FALSE(page_terms_callback.Run("a"));
  EXPECT_FALSE(page_terms_callback.Run(""));
  EXPECT_TRUE(page_terms_callback.Run("token one"));
  EXPECT_FALSE(page_terms_callback.Run("token onetwo"));
  EXPECT_TRUE(page_terms_callback.Run("token two"));
  EXPECT_FALSE(page_terms_callback.Run("token ZZ"));
}

TEST_F(PhishingScorerTest, PageWordsFlat) {
  std::unique_ptr<Scorer> scorer;
  std::string flatbuffer = GetFlatBufferString();
  base::MappedReadOnlyRegion mapped_region =
      GetMappedReadOnlyRegionWithData(flatbuffer);
  scorer = Scorer::Create(mapped_region.region.Duplicate(), base::File());
  ASSERT_TRUE(scorer.get());
  base::RepeatingCallback<bool(uint32_t)> page_words_callback(
      scorer->find_page_word_callback());
  EXPECT_FALSE(page_words_callback.Run(0U));
  EXPECT_TRUE(page_words_callback.Run(1000U));
  EXPECT_FALSE(page_words_callback.Run(1500U));
  EXPECT_TRUE(page_words_callback.Run(2000U));
  EXPECT_TRUE(page_words_callback.Run(3000U));
  EXPECT_FALSE(page_words_callback.Run(4000U));
  EXPECT_EQ(2U, scorer->max_words_per_term());
  EXPECT_EQ(12345U, scorer->murmurhash3_seed());
  EXPECT_EQ(10U, scorer->max_shingles_per_page());
  EXPECT_EQ(3U, scorer->shingle_size());
}

TEST_F(PhishingScorerTest, ComputeScoreFlat) {
  std::unique_ptr<Scorer> scorer;
  std::string flatbuffer = GetFlatBufferString();
  base::MappedReadOnlyRegion mapped_region =
      GetMappedReadOnlyRegionWithData(flatbuffer);
  scorer = Scorer::Create(mapped_region.region.Duplicate(), base::File());
  EXPECT_TRUE(scorer.get() != nullptr);

  // An empty feature map should match the empty rule.
  FeatureMap features;
  // The expected logodds is 0.5 (empty rule) => p = exp(0.5) / (exp(0.5) + 1)
  // => 0.62245933120185459
  EXPECT_DOUBLE_EQ(0.62245933120185459, scorer->ComputeScore(features));
  // Same if the feature does not match any rule.
  EXPECT_TRUE(features.AddBooleanFeature("not existing feature"));
  EXPECT_DOUBLE_EQ(0.62245933120185459, scorer->ComputeScore(features));

  // Feature 1 matches which means that the logodds will be:
  //   0.5 (empty rule) + 2.0 (rule weight) * 0.15 (feature weight) = 0.8
  //   => p = 0.6899744811276125
  EXPECT_TRUE(features.AddRealFeature("feature1", 0.15));
  EXPECT_DOUBLE_EQ(0.6899744811276125, scorer->ComputeScore(features));

  // Now, both feature 1 and feature 2 match.  Expected logodds:
  //   0.5 (empty rule) + 2.0 (rule weight) * 0.15 (feature weight) +
  //   3.0 (rule weight) * 0.15 (feature1 weight) * 1.0 (feature2) weight = 9.8
  //   => p = 0.99999627336071584
  EXPECT_TRUE(features.AddBooleanFeature("feature2"));
  EXPECT_DOUBLE_EQ(0.77729986117469119, scorer->ComputeScore(features));
}

TEST_F(PhishingScorerTest, DomModelVersionFlatbuffer) {
  std::unique_ptr<Scorer> scorer;
  std::string flatbuffer = GetFlatBufferString();
  base::MappedReadOnlyRegion mapped_region =
      GetMappedReadOnlyRegionWithData(flatbuffer);
  scorer = Scorer::Create(mapped_region.region.Duplicate(), base::File());
  ASSERT_TRUE(scorer.get() != nullptr);
  EXPECT_EQ(scorer->dom_model_version(), 123);
}

}  // namespace safe_browsing
