// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_cache_impl.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_cache.h"
#include "components/autofill/core/browser/proto/autofill_ai_model_cache.pb.h"
#include "components/autofill/core/common/signatures.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

class AutofillAiModelCacheImplTest : public ::testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_provider_ = std::make_unique<leveldb_proto::ProtoDatabaseProvider>(
        temp_dir_.GetPath());
    RecreateCache();
  }

  void TearDown() override {
    cache_.reset();
    db_provider_.reset();
    // Allow for destruction on a different sequence.
    task_environment_.RunUntilIdle();
  }

  void AdvanceClock(base::TimeDelta delta) {
    task_environment_.AdvanceClock(delta);
  }

  // Simulates restart of the browser by recreating the cache.
  void RecreateCache(size_t max_cache_size = 50,
                     base::TimeDelta max_cache_age = base::Days(7)) {
    // Process remaining operations.
    task_environment_.RunUntilIdle();
    cache_ = std::make_unique<AutofillAiModelCacheImpl>(
        /*history_service=*/nullptr, db_provider_.get(), temp_dir_.GetPath(),
        max_cache_size, max_cache_age);
    // Wait until database has loaded.
    task_environment_.RunUntilIdle();
  }

  AutofillAiModelCache& cache() { return *cache_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<leveldb_proto::ProtoDatabaseProvider> db_provider_;
  std::unique_ptr<AutofillAiModelCache> cache_;
};

TEST_F(AutofillAiModelCacheImplTest, AddNewEntry) {
  constexpr auto signature1 = FormSignature(123);
  constexpr auto signature2 = FormSignature(234);

  EXPECT_FALSE(cache().Contains(signature1));
  EXPECT_FALSE(cache().Contains(signature2));
  cache().Update(signature1, AutofillAiModelCache::ModelResponse(), {});
  EXPECT_TRUE(cache().Contains(signature1));
  EXPECT_FALSE(cache().Contains(signature2));
}

// Tests that recreating the cache repopulates it with the data persisted on
// disk.
TEST_F(AutofillAiModelCacheImplTest, CacheSurvivesRestart) {
  constexpr auto signature = FormSignature(123);

  EXPECT_FALSE(cache().Contains(signature));
  cache().Update(signature, AutofillAiModelCache::ModelResponse(), {});
  EXPECT_TRUE(cache().Contains(signature));

  // Simulate restart.
  RecreateCache();
  EXPECT_TRUE(cache().Contains(signature));
}

// Tests that the maximum cache size is enforced by removing the oldest entries
// that exceed the cache size.
TEST_F(AutofillAiModelCacheImplTest, MaxCacheSize) {
  constexpr auto signature1 = FormSignature(123);
  constexpr auto signature2 = FormSignature(1234);
  constexpr auto signature3 = FormSignature(12345);
  constexpr auto signature4 = FormSignature(123456);

  RecreateCache(/*max_cache_size=*/3);
  cache().Update(signature1, AutofillAiModelCache::ModelResponse(), {});
  AdvanceClock(base::Days(1));
  cache().Update(signature2, AutofillAiModelCache::ModelResponse(), {});
  AdvanceClock(base::Days(1));
  cache().Update(signature3, AutofillAiModelCache::ModelResponse(), {});
  AdvanceClock(base::Days(1));
  EXPECT_TRUE(cache().Contains(signature1));
  EXPECT_TRUE(cache().Contains(signature2));
  EXPECT_TRUE(cache().Contains(signature3));
  EXPECT_FALSE(cache().Contains(signature4));

  // Adding a fourth entry removes the first one.
  cache().Update(signature4, AutofillAiModelCache::ModelResponse(), {});
  EXPECT_FALSE(cache().Contains(signature1));
  EXPECT_TRUE(cache().Contains(signature2));
  EXPECT_TRUE(cache().Contains(signature3));
  EXPECT_TRUE(cache().Contains(signature4));

  // This remains true after a restart.
  RecreateCache();
  EXPECT_FALSE(cache().Contains(signature1));
  EXPECT_TRUE(cache().Contains(signature2));
  EXPECT_TRUE(cache().Contains(signature3));
  EXPECT_TRUE(cache().Contains(signature4));
}

// Tests that the maximum cache age is enforced.
TEST_F(AutofillAiModelCacheImplTest, MaxCacheAge) {
  constexpr auto signature1 = FormSignature(123);
  constexpr auto signature2 = FormSignature(1234);
  constexpr auto signature3 = FormSignature(12345);

  RecreateCache(/*max_cache_size=*/10, /*max_cache_age=*/base::Days(3));
  cache().Update(signature1, AutofillAiModelCache::ModelResponse(), {});
  AdvanceClock(base::Days(1));
  cache().Update(signature2, AutofillAiModelCache::ModelResponse(), {});
  AdvanceClock(base::Days(1));
  cache().Update(signature3, AutofillAiModelCache::ModelResponse(), {});
  AdvanceClock(base::Days(1));
  EXPECT_TRUE(cache().Contains(signature1));
  EXPECT_TRUE(cache().Contains(signature2));
  EXPECT_TRUE(cache().Contains(signature3));

  // If we advance the clock further, the first entry expires.
  AdvanceClock(base::Hours(1));
  EXPECT_FALSE(cache().Contains(signature1));
  EXPECT_TRUE(cache().Contains(signature2));
  EXPECT_TRUE(cache().Contains(signature3));

  // A day later, the second entry expires as well.
  AdvanceClock(base::Days(1));
  EXPECT_FALSE(cache().Contains(signature1));
  EXPECT_FALSE(cache().Contains(signature2));
  EXPECT_TRUE(cache().Contains(signature3));

  // This is still true after a restart.
  RecreateCache(/*max_cache_size=*/10, /*max_cache_age=*/base::Days(3));
  EXPECT_FALSE(cache().Contains(signature1));
  EXPECT_FALSE(cache().Contains(signature2));
  EXPECT_TRUE(cache().Contains(signature3));
}

TEST_F(AutofillAiModelCacheImplTest, Erase) {
  constexpr auto signature1 = FormSignature(123);
  constexpr auto signature2 = FormSignature(1234);

  cache().Update(signature1, AutofillAiModelCache::ModelResponse(), {});
  cache().Update(signature2, AutofillAiModelCache::ModelResponse(), {});
  EXPECT_TRUE(cache().Contains(signature1));
  EXPECT_TRUE(cache().Contains(signature2));

  cache().Erase(signature2);
  EXPECT_TRUE(cache().Contains(signature1));
  EXPECT_FALSE(cache().Contains(signature2));

  cache().Erase(signature1);
  EXPECT_FALSE(cache().Contains(signature1));
  EXPECT_FALSE(cache().Contains(signature2));
}

// Tests that `Autofill.AutofillAi.ModelCache.InitSuccess` is emitted on
// startup.
TEST_F(AutofillAiModelCacheImplTest, InitSuccessMetric) {
  base::HistogramTester histogram_tester;
  RecreateCache();
  histogram_tester.ExpectUniqueSample(
      "Autofill.AutofillAi.ModelCache.InitSuccess", true, 1);
}

// Tests that the cache returns the properly parsed field predictions for a
// given cache entry.
TEST_F(AutofillAiModelCacheImplTest, GetFieldPredictions) {
  constexpr auto form_signature = FormSignature(123);
  constexpr auto field_signature1 = FieldSignature(345);
  constexpr auto field_signature2 = FieldSignature(765);

  AutofillAiModelCache::ModelResponse model_response;
  {
    auto* field_response = model_response.add_field_responses();
    field_response->set_field_type(PASSPORT_NUMBER);
  }
  {
    auto* field_response = model_response.add_field_responses();
    field_response->set_field_type(PASSPORT_ISSUING_COUNTRY);
  }
  {
    auto* field_response = model_response.add_field_responses();
    field_response->set_field_type(PASSPORT_EXPIRATION_DATE);
    field_response->set_formatting_meta("DD.MM.YYYY");
  }

  using FieldIdentifier = AutofillAiModelCache::FieldIdentifier;
  using FieldPrediction = AutofillAiModelCache::FieldPrediction;
  const auto identifier1 = FieldIdentifier{.signature = field_signature1};
  const auto identifier2 = FieldIdentifier{.signature = field_signature1,
                                           .rank_in_signature_group = 1};
  const auto identifier3 = FieldIdentifier{.signature = field_signature2};
  cache().Update(form_signature, model_response,
                 {identifier1, identifier2, identifier3});

  EXPECT_THAT(
      cache().GetFieldPredictions(form_signature),
      UnorderedElementsAre(
          Pair(identifier1, FieldPrediction{.field_type = PASSPORT_NUMBER}),
          Pair(identifier2,
               FieldPrediction{.field_type = PASSPORT_ISSUING_COUNTRY}),
          Pair(identifier3,
               FieldPrediction{.field_type = PASSPORT_EXPIRATION_DATE,
                               .format_string = u"DD.MM.YYYY"})));
}

// Tests that the cache handles invalid field types gracefully.
TEST_F(AutofillAiModelCacheImplTest, GetFieldPredictionsInvalidType) {
  constexpr auto form_signature = FormSignature(123);
  constexpr auto field_signature = FieldSignature(345);

  AutofillAiModelCache::ModelResponse model_response;
  {
    auto* field_response = model_response.add_field_responses();
    constexpr int invalid_field_type = 789;
    static_assert(ToSafeFieldType(invalid_field_type, NO_SERVER_DATA) ==
                  NO_SERVER_DATA);
    field_response->set_field_type(invalid_field_type);
  }

  using FieldIdentifier = AutofillAiModelCache::FieldIdentifier;
  using FieldPrediction = AutofillAiModelCache::FieldPrediction;
  const auto identifier = FieldIdentifier{.signature = field_signature};
  cache().Update(form_signature, model_response, {identifier});

  EXPECT_THAT(cache().GetFieldPredictions(form_signature),
              UnorderedElementsAre(Pair(
                  identifier, FieldPrediction{.field_type = NO_SERVER_DATA})));
}

// Tests that the cache returns an empty map if there is no cache entry for
// the form signature.
TEST_F(AutofillAiModelCacheImplTest, GetNonExistantFieldPrediction) {
  EXPECT_THAT(cache().GetFieldPredictions(FormSignature(4565)), IsEmpty());
}

}  // namespace

}  // namespace autofill
