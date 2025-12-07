// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/plus_addresses/plus_address_menu_model.h"

#include "base/functional/callback.h"
#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plus_addresses {

namespace {

TEST(PlusAddressMenuModelTest, HasTwoItems) {
  PlusAddressMenuModel model(u"foo@gmail.com", base::DoNothing(),
                             base::DoNothing());
  EXPECT_EQ(model.GetItemCount(), 2u);
}

TEST(PlusAddressMenuModelTest, RunsCallbacks) {
  base::test::TestFuture<void> undo;
  base::test::TestFuture<void> open_management;
  PlusAddressMenuModel model(u"foo@gmail.com", undo.GetCallback(),
                             open_management.GetRepeatingCallback());

  EXPECT_FALSE(undo.IsReady());
  model.ExecuteCommand(PlusAddressMenuModel::kUndoReplacement,
                       /*event_flags=*/0);
  EXPECT_TRUE(undo.IsReady());

  EXPECT_FALSE(open_management.IsReady());
  model.ExecuteCommand(PlusAddressMenuModel::kManage, /*event_flags=*/0);
  EXPECT_TRUE(open_management.IsReady());
}

}  // namespace

}  // namespace plus_addresses
