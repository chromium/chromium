// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/privacy_budget/order_preserving_set.h"
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

TEST(OrderPreservingSetTest, CanInstantiate) {
  OrderPreservingSet<blink::IdentifiableSurface> default_initialized;
  EXPECT_TRUE(default_initialized.empty());

  std::vector<blink::IdentifiableSurface> surfaces_list;
  OrderPreservingSet<blink::IdentifiableSurface> initialized_with_list(
      std::move(surfaces_list));
  EXPECT_TRUE(initialized_with_list.empty());

  OrderPreservingSet<blink::IdentifiableSurface> initialized_with_initializers{
      blink::IdentifiableSurface::FromMetricHash(1)};
  EXPECT_FALSE(initialized_with_initializers.empty());
  EXPECT_EQ(1u, initialized_with_initializers[0].ToUkmMetricHash());
}

TEST(OrderPreservingSetTest, CanMoveAssign) {
  std::vector<blink::IdentifiableSurface> surfaces_list = {
      blink::IdentifiableSurface::FromMetricHash(3),  // not in order.
      blink::IdentifiableSurface::FromMetricHash(1),
      blink::IdentifiableSurface::FromMetricHash(2),
  };

  OrderPreservingSet<blink::IdentifiableSurface> surface_set;
  surface_set = std::move(surfaces_list);

  EXPECT_EQ(3u, surface_set[0].ToUkmMetricHash());
  EXPECT_EQ(1u, surface_set[1].ToUkmMetricHash());
  EXPECT_EQ(2u, surface_set[2].ToUkmMetricHash());
}

TEST(OrderPreservingSetTest, IteratorsPreserveOrder) {
  OrderPreservingSet<blink::IdentifiableSurface> surface_set{
      blink::IdentifiableSurface::FromMetricHash(3),  // not in order.
      blink::IdentifiableSurface::FromMetricHash(1),
      blink::IdentifiableSurface::FromMetricHash(2),
  };

  std::vector<blink::IdentifiableSurface> seen;
  for (const auto v : surface_set) {
    seen.push_back(v);
  }

  EXPECT_EQ(3u, surface_set[0].ToUkmMetricHash());
  EXPECT_EQ(1u, surface_set[1].ToUkmMetricHash());
  EXPECT_EQ(2u, surface_set[2].ToUkmMetricHash());
}

TEST(OrderPreservingSetTest, LookupsWork) {
  OrderPreservingSet<blink::IdentifiableSurface> surface_set{
      blink::IdentifiableSurface::FromMetricHash(3),  // not in order.
      blink::IdentifiableSurface::FromMetricHash(1),
      blink::IdentifiableSurface::FromMetricHash(2),
  };

  EXPECT_TRUE(
      surface_set.contains(blink::IdentifiableSurface::FromMetricHash(3)));
  EXPECT_FALSE(
      surface_set.contains(blink::IdentifiableSurface::FromMetricHash(4)));
}
