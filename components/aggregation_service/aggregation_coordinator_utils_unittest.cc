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
          "valid",
          "https://a.test",
          url::Origin::Create(GURL("https://a.test")),
      },
      {
          "valid-non-empty-path",
          "https://a.test/foo",
          url::Origin::Create(GURL("https://a.test")),
      },
      {
          "valid-subdomain",
          "https://a.b.test",
          url::Origin::Create(GURL("https://a.b.test")),
      },
      {
          "non-trustworthy",
          "http://a.test",
          url::Origin::Create(GURL(kDefaultAggregationCoordinatorAwsCloud)),
      },
      {
          "invalid-origin",
          "a.test",
          url::Origin::Create(GURL(kDefaultAggregationCoordinatorAwsCloud)),
      },
      {
          "multiple",
          "https://b.test,https://a.test",
          url::Origin::Create(GURL("https://b.test")),
      },
      {
          "any-invalid",
          "https://b.test,http://a.test",
          url::Origin::Create(GURL(kDefaultAggregationCoordinatorAwsCloud)),
      },
      {
          "empty",
          "",
          url::Origin::Create(GURL(kDefaultAggregationCoordinatorAwsCloud)),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);

    ScopedAggregationCoordinatorAllowlistForTesting
        scoped_coordinator_allowlist;

    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        ::aggregation_service::kAggregationServiceMultipleCloudProviders,
        {{"allowlist", test_case.feature_param}});

    EXPECT_EQ(GetDefaultAggregationCoordinatorOrigin(), test_case.expected);
  }
}

TEST(AggregationCoordinatorUtilsTest, IsAggregationCoordinatorOriginAllowed) {
  const struct {
    url::Origin origin;
    bool expected;
  } kTestCases[] = {
      {
          .origin = url::Origin::Create(GURL("https://a.test")),
          .expected = true,
      },
      {
          .origin = url::Origin::Create(GURL("https://b.test")),
          .expected = true,
      },
      {
          .origin = url::Origin::Create(GURL("https://c.test")),
          .expected = false,
      },
  };

  ScopedAggregationCoordinatorAllowlistForTesting scoped_coordinator_allowlist(
      {url::Origin::Create(GURL("https://a.test")),
       url::Origin::Create(GURL("https://b.test"))});

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.origin);
    EXPECT_EQ(IsAggregationCoordinatorOriginAllowed(test_case.origin),
              test_case.expected);
  }
}

}  // namespace
}  // namespace aggregation_service
