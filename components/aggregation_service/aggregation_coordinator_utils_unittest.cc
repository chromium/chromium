// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/aggregation_service/aggregation_coordinator_utils.h"

#include "base/test/scoped_feature_list.h"
#include "components/aggregation_service/aggregation_service.mojom.h"
#include "components/aggregation_service/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace aggregation_service {
namespace {

TEST(AggregationCoordinatorUtilsTest,
     GetAggregationCoordinatorFromFeatureParam) {
  const struct {
    const char* desc;
    const char* feature_param_name;
    const char* feature_param;
    mojom::AggregationCoordinator coordinator;
    url::Origin expected;
  } kTestCases[] = {
      {
          "aws-valid",
          "aws_cloud",
          "https://a.test",
          mojom::AggregationCoordinator::kAwsCloud,
          url::Origin::Create(GURL("https://a.test")),
      },
      {
          "aws-valid-non-empty-path",
          "aws_cloud",
          "https://a.test/foo",
          mojom::AggregationCoordinator::kAwsCloud,
          url::Origin::Create(GURL("https://a.test")),
      },
      {
          "aws-valid-subdomain",
          "aws_cloud",
          "https://a.b.test",
          mojom::AggregationCoordinator::kAwsCloud,
          url::Origin::Create(GURL("https://a.b.test")),
      },
      {
          "aws-non-trustworthy",
          "aws_cloud",
          "http://a.test",
          mojom::AggregationCoordinator::kAwsCloud,
          url::Origin::Create(GURL(kDefaultAggregationCoordinatorAwsCloud)),
      },
  };

  for (const auto& test_case : kTestCases) {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        ::aggregation_service::kAggregationServiceMultipleCloudProviders,
        {{test_case.feature_param_name, test_case.feature_param}});

    EXPECT_EQ(GetAggregationCoordinatorOrigin(test_case.coordinator),
              test_case.expected)
        << test_case.desc;
  }
}

}  // namespace
}  // namespace aggregation_service
