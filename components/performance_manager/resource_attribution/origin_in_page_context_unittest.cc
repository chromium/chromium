// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/resource_attribution/origin_in_page_context.h"

#include <memory>
#include <optional>
#include <set>

#include "base/memory/weak_ptr.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/resource_attribution/page_context.h"
#include "components/performance_manager/resource_attribution/performance_manager_aliases.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "components/performance_manager/test_support/run_in_graph.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace resource_attribution {

namespace {

using ::testing::UnorderedElementsAre;

using ResourceAttrOriginInPageContextTest =
    performance_manager::PerformanceManagerTestHarness;

TEST_F(ResourceAttrOriginInPageContextTest, SameOriginDifferentPage) {
  const auto kOrigin1 = url::Origin::Create(GURL("http://a.com:8080"));
  const auto kOrigin2 =
      url::Origin::CreateFromNormalizedTuple("http", "a.com", 8080);
  ASSERT_EQ(kOrigin1, kOrigin2);

  std::unique_ptr<content::WebContents> web_contents1 = CreateTestWebContents();
  std::unique_ptr<content::WebContents> web_contents2 = CreateTestWebContents();
  base::WeakPtr<PageNode> page_node1 =
      PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents1.get());
  base::WeakPtr<PageNode> page_node2 =
      PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents2.get());

  // Create 2 OriginInPageContexts on the main thread from separate
  // PageContexts.
  std::optional<PageContext> page_context1 =
      PageContext::FromWebContents(web_contents1.get());
  ASSERT_TRUE(page_context1.has_value());
  auto origin_context1 = OriginInPageContext(kOrigin1, page_context1.value());

  std::optional<PageContext> page_context2 =
      PageContext::FromWebContents(web_contents2.get());
  ASSERT_TRUE(page_context2.has_value());
  auto origin_context2 = OriginInPageContext(kOrigin2, page_context2.value());

  // Create 2 OriginInPageContexts on the PM sequence from separate
  // PageContexts.
  std::optional<OriginInPageContext> pm_origin_context1;
  std::optional<OriginInPageContext> pm_origin_context2;
  performance_manager::RunInGraph([&] {
    std::optional<PageContext> pm_page_context1 =
        PageContext::FromWeakPageNode(page_node1);
    ASSERT_TRUE(pm_page_context1.has_value());
    pm_origin_context1 =
        OriginInPageContext(kOrigin2, pm_page_context1.value());

    std::optional<PageContext> pm_page_context2 =
        PageContext::FromWeakPageNode(page_node2);
    ASSERT_TRUE(pm_page_context2.has_value());
    pm_origin_context2 =
        OriginInPageContext(kOrigin1, pm_page_context2.value());
  });
  ASSERT_TRUE(pm_origin_context1.has_value());
  ASSERT_TRUE(pm_origin_context2.has_value());

  // OriginInPageContexts with equivalent pages and origins should compare
  // equal, even though they were created from different Origin and PageContext
  // objects.
  const std::set<OriginInPageContext> origin_context_set{
      origin_context1, origin_context2, pm_origin_context1.value(),
      pm_origin_context2.value()};
  EXPECT_THAT(origin_context_set,
              UnorderedElementsAre(origin_context1, origin_context2));

  // Test accessors.
  EXPECT_EQ(origin_context1.GetOrigin(), kOrigin1);
  EXPECT_EQ(origin_context2.GetOrigin(), kOrigin2);
  EXPECT_EQ(origin_context1.GetPageContext(), page_context1.value());
  EXPECT_EQ(origin_context2.GetPageContext(), page_context2.value());

  // OriginInPageContexts should compare equal once their underlying page is
  // deleted, showing that like PageContexts they're compared by a permanent id,
  // not live data.
  web_contents1.reset();
  performance_manager::RunInGraph([&] {
    // Wait for the PageNode to be cleared.
    EXPECT_FALSE(page_node1);
  });
  EXPECT_EQ(origin_context1, pm_origin_context1.value());
  EXPECT_NE(origin_context1, origin_context2);
}

TEST_F(ResourceAttrOriginInPageContextTest, SamePageDifferentOrigin) {
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

  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  std::optional<PageContext> page_context =
      PageContext::FromWebContents(web_contents.get());
  ASSERT_TRUE(page_context.has_value());

  // Create OriginInPageContexts on the main thread from each origin.
  auto opaque_origin1_context =
      OriginInPageContext(kOpaqueOrigin1, page_context.value());
  auto opaque_origin2_context =
      OriginInPageContext(kOpaqueOrigin2, page_context.value());
  auto origin1_context = OriginInPageContext(kOrigin1, page_context.value());
  auto origin2_context = OriginInPageContext(kOrigin2, page_context.value());
  auto origin1_with_base_context =
      OriginInPageContext(kOrigin1WithBase, page_context.value());
  auto origin1_with_opaque_base_context =
      OriginInPageContext(kOrigin1WithOpaqueBase, page_context.value());
  auto opaque_origin1_with_base_context =
      OriginInPageContext(kOpaqueOrigin1WithBase, page_context.value());

  // OriginInPageContexts with equivalent origins should compare equal.
  std::set<OriginInPageContext> origin_context_set{
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

}  // namespace

}  // namespace resource_attribution
