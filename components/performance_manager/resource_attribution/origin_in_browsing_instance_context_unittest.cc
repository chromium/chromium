// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/resource_attribution/origin_in_browsing_instance_context.h"

#include <set>

#include "content/public/browser/browsing_instance_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace resource_attribution {

namespace {

using ::testing::UnorderedElementsAre;

constexpr content::BrowsingInstanceId kBrowsingInstance1 =
    content::BrowsingInstanceId::FromUnsafeValue(1);
constexpr content::BrowsingInstanceId kBrowsingInstance2 =
    content::BrowsingInstanceId::FromUnsafeValue(2);

}  // namespace

TEST(ResourceAttrOriginInBrowsingInstanceContextTest,
     SameOriginDifferentBrowsingInstance) {
  const auto kOrigin = url::Origin::Create(GURL("http://a.com:8080"));
  const auto kEquivalentOrigin =
      url::Origin::CreateFromNormalizedTuple("http", "a.com", 8080);
  ASSERT_EQ(kOrigin, kEquivalentOrigin);

  // Create OriginInBrowsingInstanceContexts from different browsing instances.
  auto origin_context1 =
      OriginInBrowsingInstanceContext(kOrigin, kBrowsingInstance1);
  auto origin_context2 =
      OriginInBrowsingInstanceContext(kEquivalentOrigin, kBrowsingInstance2);
  auto other_origin_context1 =
      OriginInBrowsingInstanceContext(kOrigin, kBrowsingInstance1);
  auto other_origin_context2 =
      OriginInBrowsingInstanceContext(kEquivalentOrigin, kBrowsingInstance2);

  // OriginInBrowsingInstanceContexts with equivalent origin and browsing
  // instance should compare equal, even when they were created from different
  // (but equal) origins.
  const std::set<OriginInBrowsingInstanceContext> origin_context_set{
      origin_context1, origin_context2, other_origin_context1,
      other_origin_context2};
  EXPECT_THAT(origin_context_set,
              UnorderedElementsAre(origin_context1, origin_context2));

  // Test accessors.
  EXPECT_EQ(origin_context1.GetOrigin(), kOrigin);
  EXPECT_EQ(origin_context2.GetOrigin(), kEquivalentOrigin);
  EXPECT_EQ(origin_context1.GetBrowsingInstance(), kBrowsingInstance1);
  EXPECT_EQ(origin_context2.GetBrowsingInstance(), kBrowsingInstance2);
}

TEST(ResourceAttrOriginInBrowsingInstanceContextTest,
     SameBrowsingInstanceDifferentOrigin) {
  const auto kUrl1 = GURL("http://a.com");
  const auto kUrl2 = GURL("http://b.com");

  // These should compare different from all origins that aren't derived from
  // them.
  const url::Origin kOpaqueOrigin1;
  const url::Origin kOpaqueOrigin2;

  // These should compare different from each other.
  const auto kOrigin1 = url::Origin::Create(kUrl1);
  const auto kOrigin2 = url::Origin::Create(kUrl2);

  // These should compare equal to kOrigin1 despite the different bases.
  const auto kOrigin1WithBase = url::Origin::Resolve(kUrl1, kOrigin2);
  const auto kOrigin1WithOpaqueBase =
      url::Origin::Resolve(kUrl1, kOpaqueOrigin1);

  // This should compare equal to kOpaqueOrigin1 because it uses its base
  // directly.
  const auto kOpaqueOrigin1WithBase =
      url::Origin::Resolve(GURL("about:blank"), kOpaqueOrigin1);

  const std::set<url::Origin> origin_set{kOpaqueOrigin1,
                                         kOpaqueOrigin2,
                                         kOrigin1,
                                         kOrigin2,
                                         kOrigin1WithBase,
                                         kOrigin1WithOpaqueBase,
                                         kOpaqueOrigin1WithBase};
  ASSERT_THAT(origin_set, UnorderedElementsAre(kOpaqueOrigin1, kOpaqueOrigin2,
                                               kOrigin1, kOrigin2));

  // Create OriginInBrowsingInstanceContexts from each origin.
  auto opaque_origin1_context =
      OriginInBrowsingInstanceContext(kOpaqueOrigin1, kBrowsingInstance1);
  auto opaque_origin2_context =
      OriginInBrowsingInstanceContext(kOpaqueOrigin2, kBrowsingInstance1);
  auto origin1_context =
      OriginInBrowsingInstanceContext(kOrigin1, kBrowsingInstance1);
  auto origin2_context =
      OriginInBrowsingInstanceContext(kOrigin2, kBrowsingInstance1);
  auto origin1_with_base_context =
      OriginInBrowsingInstanceContext(kOrigin1WithBase, kBrowsingInstance1);
  auto origin1_with_opaque_base_context = OriginInBrowsingInstanceContext(
      kOrigin1WithOpaqueBase, kBrowsingInstance1);
  auto opaque_origin1_with_base_context = OriginInBrowsingInstanceContext(
      kOpaqueOrigin1WithBase, kBrowsingInstance1);

  // OriginInBrowsingInstanceContexts with equivalent origins should compare
  // equal.
  std::set<OriginInBrowsingInstanceContext> origin_context_set{
      opaque_origin1_context,
      opaque_origin2_context,
      origin1_context,
      origin2_context,
      origin1_with_base_context,
      origin1_with_opaque_base_context,
      opaque_origin1_with_base_context};
  EXPECT_THAT(
      origin_context_set,
      UnorderedElementsAre(opaque_origin1_context, opaque_origin2_context,
                           origin1_context, origin2_context));

  // Test accessors.
  EXPECT_EQ(opaque_origin1_context.GetOrigin(), kOpaqueOrigin1);
  EXPECT_EQ(opaque_origin2_context.GetOrigin(), kOpaqueOrigin2);
  EXPECT_EQ(origin1_context.GetOrigin(), kOrigin1);
  EXPECT_EQ(origin2_context.GetOrigin(), kOrigin2);
  EXPECT_EQ(origin1_with_base_context.GetOrigin(), kOrigin1WithBase);
  EXPECT_EQ(origin1_with_opaque_base_context.GetOrigin(),
            kOrigin1WithOpaqueBase);
  EXPECT_EQ(opaque_origin1_with_base_context.GetOrigin(),
            kOpaqueOrigin1WithBase);
}

}  // namespace resource_attribution
