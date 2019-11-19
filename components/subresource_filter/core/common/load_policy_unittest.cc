// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/load_policy.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

TEST(SubresourceFilterLoadPolicyTest, MoreRestrictiveLoadPolicy) {
  EXPECT_EQ(MoreRestrictiveLoadPolicy(LoadPolicy::DISALLOW,
                                      LoadPolicy::WOULD_DISALLOW),
            LoadPolicy::DISALLOW);
  EXPECT_EQ(MoreRestrictiveLoadPolicy(LoadPolicy::DISALLOW, LoadPolicy::ALLOW),
            LoadPolicy::DISALLOW);
  EXPECT_EQ(
      MoreRestrictiveLoadPolicy(LoadPolicy::WOULD_DISALLOW, LoadPolicy::ALLOW),
      LoadPolicy::WOULD_DISALLOW);
}

}  // namespace subresource_filter
