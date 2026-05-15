// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/ui_controllers/toolbar/icon_handle.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace toolbar_ui_api {

namespace {

class TestIcon : public IconHandle::Provider {
 public:
  explicit TestIcon(uint64_t handle_id) : handle_id_(handle_id) {}

  IconHandleId HandleId() override { return handle_id_; }

 private:
  friend class base::RefCounted<TestIcon>;
  ~TestIcon() override = default;

  IconHandleId handle_id_;
};

}  // namespace

TEST(IconHandle, Equality) {
  IconHandle null_handle, null_handle2;
  IconHandle icon1(base::MakeRefCounted<TestIcon>(1u));
  IconHandle icon2(base::MakeRefCounted<TestIcon>(2u));

  EXPECT_EQ(null_handle, null_handle);
  EXPECT_EQ(null_handle, null_handle2);
  EXPECT_NE(null_handle, icon1);
  EXPECT_NE(null_handle, icon2);

  EXPECT_EQ(null_handle2, null_handle);
  EXPECT_EQ(null_handle2, null_handle2);
  EXPECT_NE(null_handle2, icon1);
  EXPECT_NE(null_handle2, icon2);

  EXPECT_NE(icon1, null_handle);
  EXPECT_NE(icon1, null_handle2);
  EXPECT_EQ(icon1, icon1);
  EXPECT_NE(icon1, icon2);

  EXPECT_NE(icon2, null_handle);
  EXPECT_NE(icon2, null_handle2);
  EXPECT_NE(icon2, icon1);
  EXPECT_EQ(icon2, icon2);
}

TEST(IconHandle, Getters) {
  IconHandle null_handle;
  IconHandle icon1(base::MakeRefCounted<TestIcon>(1u));
  IconHandle icon2(base::MakeRefCounted<TestIcon>(2u));

  EXPECT_TRUE(null_handle.is_null());
  EXPECT_EQ(0u, null_handle.HandleId().value());

  EXPECT_FALSE(icon1.is_null());
  EXPECT_EQ(1u, icon1.HandleId().value());

  EXPECT_FALSE(icon2.is_null());
  EXPECT_EQ(2u, icon2.HandleId().value());
}

}  // namespace toolbar_ui_api
