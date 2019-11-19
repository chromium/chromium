// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/client_filterable_state.h"

#include "base/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {

TEST(VariationsClientFilterableStateTest, IsEnterprise) {
  // Test, for non enterprise clients, is_enterprise_function_ is called once.
  ClientFilterableState client_non_enterprise(
      base::BindOnce([] { return false; }));
  EXPECT_FALSE(client_non_enterprise.IsEnterprise());
  EXPECT_FALSE(client_non_enterprise.IsEnterprise());

  // Test, for enterprise clients, is_enterprise_function_ is called once.
  ClientFilterableState client_enterprise(base::BindOnce([] { return true; }));
  EXPECT_TRUE(client_enterprise.IsEnterprise());
  EXPECT_TRUE(client_enterprise.IsEnterprise());
}

}  // namespace variations
