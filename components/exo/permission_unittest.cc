// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/permission.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace exo {
namespace {

TEST(PermissionsTest, ActiveCapability) {
  Permission p{Permission::Capability::kActivate, base::Days(1)};
  ASSERT_TRUE(p.Check(Permission::Capability::kActivate));
  ASSERT_FALSE(p.Check(static_cast<Permission::Capability>(
      (int)(Permission::Capability::kActivate) + 1)));
}

TEST(PermissionsTest, Revoke) {
  Permission p{Permission::Capability::kActivate, base::Days(1)};
  ASSERT_TRUE(p.Check(Permission::Capability::kActivate));
  p.Revoke();
  ASSERT_FALSE(p.Check(Permission::Capability::kActivate));
}

TEST(PermissionsTest, Expire) {
  Permission p{Permission::Capability::kActivate, base::Milliseconds(0)};
  ASSERT_FALSE(p.Check(Permission::Capability::kActivate));
}

}  // namespace
}  // namespace exo
