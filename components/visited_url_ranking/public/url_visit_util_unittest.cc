// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/public/url_visit_util.h"

#include <optional>

#include "base/memory/scoped_refptr.h"
#include "components/segmentation_platform/public/types/processed_value.h"
#include "components/visited_url_ranking/public/test_support.h"
#include "components/visited_url_ranking/public/url_visit_schema.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using segmentation_platform::InputContext;
using segmentation_platform::processing::ProcessedValue;

namespace visited_url_ranking {

constexpr int kNumAuxiliaryMetadataInputs = 3;

class URLVisitUtilTest : public testing::Test,
                         public ::testing::WithParamInterface<Fetcher> {};

TEST_F(URLVisitUtilTest, CreateInputContextFromURLVisitAggregate) {
  auto aggregate = CreateSampleURLVisitAggregate(GURL(kSampleSearchUrl));
  scoped_refptr<InputContext> input_context =
      AsInputContext(kURLVisitAggregateSchema, aggregate);
  ASSERT_EQ(input_context->metadata_args.size(),
            kNumInputs + kNumAuxiliaryMetadataInputs);

  for (const auto& field_schema : kURLVisitAggregateSchema) {
    EXPECT_TRUE(input_context->metadata_args.find(field_schema.name) !=
                input_context->metadata_args.end());
  }

  std::optional<ProcessedValue> tab_count = input_context->GetMetadataArgument(
      kURLVisitAggregateSchema
          .at(URLVisitAggregateRankingModelInputSignals::kLocalTabCount)
          .name);
  ASSERT_TRUE(tab_count);
  EXPECT_EQ(tab_count.value(), ProcessedValue::FromFloat(1.0f));

  std::optional<ProcessedValue> visit_count =
      input_context->GetMetadataArgument(
          kURLVisitAggregateSchema
              .at(URLVisitAggregateRankingModelInputSignals::kVisitCount)
              .name);
  ASSERT_TRUE(visit_count);
  EXPECT_EQ(visit_count.value(), ProcessedValue::FromFloat(1.0f));
}

TEST_P(URLVisitUtilTest, CreateInputContextFromURLVisitAggregateSingleFetcher) {
  const auto fetcher = GetParam();
  auto aggregate = CreateSampleURLVisitAggregate(GURL(kSampleSearchUrl), 1.0f,
                                                 base::Time::Now(), {fetcher});
  scoped_refptr<InputContext> input_context =
      AsInputContext(kURLVisitAggregateSchema, aggregate);
  ASSERT_EQ(input_context->metadata_args.size(),
            kNumInputs + kNumAuxiliaryMetadataInputs);

  for (const auto& field_schema : kURLVisitAggregateSchema) {
    EXPECT_TRUE(input_context->metadata_args.find(field_schema.name) !=
                input_context->metadata_args.end());
  }
}

TEST_P(URLVisitUtilTest, GettersReturnDataURLVisitAggregateSingleFetcher) {
  const auto fetcher = GetParam();
  auto aggregate = CreateSampleURLVisitAggregate(GURL(kSampleSearchUrl), 1.0f,
                                                 base::Time::Now(), {fetcher});
  if (fetcher == Fetcher::kHistory) {
    const history::AnnotatedVisit* visit =
        GetHistoryEntryVisitIfExists(aggregate);
    ASSERT_EQ(GURL(kSampleSearchUrl), visit->url_row.url());
    ASSERT_EQ(u"sample_title", visit->url_row.title());
  } else {
    const URLVisitAggregate::TabData* tab_data = GetTabDataIfExists(aggregate);
    EXPECT_EQ(1u, tab_data->tab_count);
    EXPECT_FALSE(tab_data->pinned);

    const URLVisitAggregate::Tab* tab = GetTabIfExists(aggregate);
    ASSERT_EQ(GURL(kSampleSearchUrl), tab->visit.url);
    ASSERT_EQ(u"sample_title", tab->visit.title);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         URLVisitUtilTest,
                         ::testing::Values(Fetcher::kHistory,
                                           Fetcher::kSession,
                                           Fetcher::kTabModel));

}  // namespace visited_url_ranking
