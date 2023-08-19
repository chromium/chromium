// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/aggregation_service/aggregation_coordinator_utils.h"

#include "base/test/scoped_feature_list.h"
#include "components/aggregation_service/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace aggregation_service {
namespace {

TEST(AggregationCoordinatorUtilsTest, GetDefaultAggregationCoordinatorOrigin) {
  const struct {
    const char* desc;
    const char* feature_param;
    url::Origin expected;
  } kTestCases[] = {
      {
          "aws-valid",
          "https://a.test",
          url::Origin::Create(GURL("https://a.test")),
      },
      {
          "aws-valid-non-empty-path",
          "https://a.test/foo",
          url::Origin::Create(GURL("https://a.test")),
      },
      {
          "aws-valid-subdomain",
          "https://a.b.test",
          url::Origin::Create(GURL("https://a.b.test")),
      },
      {
          "aws-non-trustworthy",
          "http://a.test",
          url::Origin::Create(GURL(kDefaultAggregationCoordinatorAwsCloud)),
      },
  };

  for (const auto& test_case : kTestCases) {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        ::aggregation_service::kAggregationServiceMultipleCloudProviders,
        {{"aws_cloud", test_case.feature_param}});

    EXPECT_EQ(GetDefaultAggregationCoordinatorOrigin(), test_case.expected)
        << test_case.desc;
  }
}

TEST(AggregationCoordinatorUtilsTest, IsAggregationCoordinatorOriginAllowed) {
  const struct {
    url::Origin origin;
    bool expected;
  } kTestCases[] = {
      {
          .origin = url::Origin::Create(GURL("https://aws.example.test")),
          .expected = true,
      },
      {
          .origin = url::Origin::Create(GURL("https://gcp.example.test")),
          .expected = true,
      },
      {
          .origin = url::Origin::Create(GURL("https://a.test")),
          .expected = false,
      },
  };

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      ::aggregation_service::kAggregationServiceMultipleCloudProviders,
      {{"aws_cloud", "https://aws.example.test"},
       {"gcp_cloud", "https://gcp.example.test"}});

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(IsAggregationCoordinatorOriginAllowed(test_case.origin),
              test_case.expected)
        << test_case.origin;
  }
}

}  // namespace
}  // namespace aggregation_service
