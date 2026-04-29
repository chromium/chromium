// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/actor/core/origin_checker.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/actor/core/actor_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace actor {
namespace {

constexpr std::string_view kExample = "https://example.com";
constexpr std::string_view kExampleSub = "https://sub.example.com";
constexpr std::string_view kAnother = "https://another.com";

class OriginCheckerTest : public ::testing::TestWithParam<bool> {
 public:
  OriginCheckerTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{kGlicCrossOriginNavigationGating,
          {{kGlicNavigationGatingUseSiteNotOrigin.name,
            is_site_scoped() ? "true" : "false"}}}},
        {});
  }
  ~OriginCheckerTest() override = default;

  bool is_site_scoped() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(OriginCheckerTest, InitialState) {
  const url::Origin example = url::Origin::Create(GURL(kExample));
  OriginChecker origin_checker;
  EXPECT_FALSE(origin_checker.IsNavigationAllowed(
      example, url::Origin::Create(GURL(kAnother))));
  EXPECT_FALSE(origin_checker.IsNavigationConfirmedByUser(example));
}

TEST_P(OriginCheckerTest, AllowNavigationToSingleOrigin) {
  const url::Origin example = url::Origin::Create(GURL(kExample));
  OriginChecker origin_checker;
  origin_checker.AllowNavigationTo(example,
                                   /*is_user_confirmed=*/false);

  const url::Origin another_origin = url::Origin::Create(GURL(kAnother));
  EXPECT_TRUE(origin_checker.IsNavigationAllowed(another_origin, example));
  EXPECT_EQ(origin_checker.IsNavigationAllowed(
                another_origin, url::Origin::Create(GURL(kExampleSub))),
            is_site_scoped());
  EXPECT_FALSE(origin_checker.IsNavigationAllowed(
      another_origin, url::Origin::Create(GURL("http://example.com"))));
  EXPECT_FALSE(origin_checker.IsNavigationAllowed(
      another_origin, url::Origin::Create(GURL("https://other.com"))));
}

TEST_P(OriginCheckerTest, AllowNavigationTo_Opaque) {
  const url::Origin example = url::Origin::Create(GURL(kExample));
  const url::Origin opaque;
  OriginChecker origin_checker;
  origin_checker.AllowNavigationTo(opaque,
                                   /*is_user_confirmed=*/false);

  EXPECT_TRUE(origin_checker.IsNavigationAllowed(example, opaque));
  EXPECT_FALSE(origin_checker.IsNavigationAllowed(example, url::Origin()));
  EXPECT_FALSE(origin_checker.IsNavigationAllowed(opaque, example));
}

TEST_P(OriginCheckerTest, AllowNavigationToMultipleOrigins) {
  const url::Origin example = url::Origin::Create(GURL(kExample));
  const url::Origin example_sub = url::Origin::Create(GURL(kExampleSub));
  const url::Origin foo = url::Origin::Create(GURL("https://foo.com"));
  const url::Origin another_origin = url::Origin::Create(GURL(kAnother));
  OriginChecker origin_checker;
  origin_checker.AllowNavigationTo({example, foo});

  EXPECT_TRUE(origin_checker.IsNavigationAllowed(another_origin, example));
  EXPECT_EQ(origin_checker.IsNavigationAllowed(another_origin, example_sub),
            is_site_scoped());
  EXPECT_TRUE(origin_checker.IsNavigationAllowed(another_origin, foo));
  EXPECT_FALSE(origin_checker.IsNavigationAllowed(
      another_origin, url::Origin::Create(GURL("https://other.com"))));
}

TEST_P(OriginCheckerTest, IsNavigationAllowed_SameOrigin) {
  const url::Origin example = url::Origin::Create(GURL(kExample));
  OriginChecker origin_checker;

  EXPECT_TRUE(origin_checker.IsNavigationAllowed(example, example));
}

TEST_P(OriginCheckerTest, IsNavigationAllowed_SameSite) {
  OriginChecker origin_checker;
  origin_checker.AllowNavigationTo(url::Origin::Create(GURL(kExampleSub)),
                                   /*is_user_confirmed=*/false);

  EXPECT_EQ(
      origin_checker.IsNavigationAllowed(url::Origin::Create(GURL(kAnother)),
                                         url::Origin::Create(GURL(kExample))),
      is_site_scoped());
}

TEST_P(OriginCheckerTest, IsNavigationAllowed_OpaqueInitiator) {
  const url::Origin example = url::Origin::Create(GURL(kExample));
  const url::Origin example_sub = url::Origin::Create(GURL(kExampleSub));
  OriginChecker origin_checker;
  origin_checker.AllowNavigationTo(example,
                                   /*is_user_confirmed=*/false);

  url::Origin opaque;
  EXPECT_TRUE(origin_checker.IsNavigationAllowed(opaque, example));
  EXPECT_EQ(origin_checker.IsNavigationAllowed(opaque, example_sub),
            is_site_scoped());
  EXPECT_FALSE(origin_checker.IsNavigationAllowed(
      opaque, url::Origin::Create(GURL(kAnother))));
}

TEST_P(OriginCheckerTest, ConfirmOrigin_Query) {
  const url::Origin example = url::Origin::Create(GURL(kExample));
  const url::Origin example_sub = url::Origin::Create(GURL(kExampleSub));

  OriginChecker origin_checker;
  origin_checker.AllowNavigationTo(example,
                                   /*is_user_confirmed=*/true);

  EXPECT_TRUE(origin_checker.IsNavigationConfirmedByUser(example));
  EXPECT_EQ(origin_checker.IsNavigationConfirmedByUser(example_sub),
            is_site_scoped());
  EXPECT_FALSE(origin_checker.IsNavigationConfirmedByUser(
      url::Origin::Create(GURL(kAnother))));
}

TEST_P(OriginCheckerTest, ConfirmOrigin_AllowsNavigation) {
  const url::Origin example = url::Origin::Create(GURL(kExample));
  const url::Origin example_sub = url::Origin::Create(GURL(kExampleSub));
  const url::Origin another_origin = url::Origin::Create(GURL(kAnother));

  OriginChecker origin_checker;
  origin_checker.AllowNavigationTo(example,
                                   /*is_user_confirmed=*/true);

  EXPECT_TRUE(origin_checker.IsNavigationAllowed(another_origin, example));
  EXPECT_EQ(origin_checker.IsNavigationAllowed(another_origin, example_sub),
            is_site_scoped());
  EXPECT_FALSE(origin_checker.IsNavigationAllowed(
      another_origin, url::Origin::Create(GURL("http://other.com"))));
}

TEST_P(OriginCheckerTest, ConfirmOrigin_Opaque) {
  const url::Origin opaque;

  OriginChecker origin_checker;
  origin_checker.AllowNavigationTo(opaque,
                                   /*is_user_confirmed=*/true);

  EXPECT_TRUE(origin_checker.IsNavigationConfirmedByUser(opaque));
  EXPECT_FALSE(origin_checker.IsNavigationConfirmedByUser(url::Origin()));
}

TEST_P(OriginCheckerTest,
       ConfirmOrigin_AllowsNavigation_RemembersConfirmation) {
  const url::Origin example = url::Origin::Create(GURL(kExample));
  const url::Origin example_sub = url::Origin::Create(GURL(kExampleSub));

  OriginChecker origin_checker;
  origin_checker.AllowNavigationTo(example,
                                   /*is_user_confirmed=*/false);
  origin_checker.AllowNavigationTo(example,
                                   /*is_user_confirmed=*/true);
  origin_checker.AllowNavigationTo(example,
                                   /*is_user_confirmed=*/false);

  EXPECT_TRUE(origin_checker.IsNavigationConfirmedByUser(example));
  EXPECT_EQ(origin_checker.IsNavigationConfirmedByUser(example_sub),
            is_site_scoped());
}

TEST_P(OriginCheckerTest, RecordsHistograms) {
  base::HistogramTester histograms;

  const url::Origin example = url::Origin::Create(GURL(kExample));
  const url::Origin example_sub = url::Origin::Create(GURL(kExampleSub));
  const url::Origin another = url::Origin::Create(GURL(kAnother));
  OriginChecker origin_checker;
  origin_checker.AllowNavigationTo(example, /*is_user_confirmed=*/true);
  origin_checker.AllowNavigationTo(example_sub, /*is_user_confirmed=*/true);
  origin_checker.AllowNavigationTo(another, /*is_user_confirmed=*/false);
  origin_checker.RecordSizeMetrics();

  histograms.ExpectUniqueSample("Actor.NavigationGating.AllowListSize",
                                /*sample=*/is_site_scoped() ? 2 : 3,
                                /*expected_bucket_count=*/1);
  histograms.ExpectUniqueSample("Actor.NavigationGating.ConfirmedListSize2",
                                /*sample=*/is_site_scoped() ? 1 : 2,
                                /*expected_bucket_count=*/1);
}

INSTANTIATE_TEST_SUITE_P(, OriginCheckerTest, ::testing::Bool());

}  // namespace
}  // namespace actor
