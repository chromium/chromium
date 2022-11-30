// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/add_supervision/add_supervision_handler_utils.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "testing/gtest/include/gtest/gtest.h"

using AddSupervisionHandlerUtilsTest = testing::Test;

// Tests that only the right apps are returned via the API.
TEST_F(AddSupervisionHandlerUtilsTest, TestShouldIncludeAppUpdate) {
  // Return ARC apps.
  auto arc_state =
      std::make_unique<apps::App>(apps::AppType::kArc, "arc_app_id");
  apps::AppUpdate arc_update(arc_state.get(), nullptr /* delta */,
                             EmptyAccountId());
  EXPECT_TRUE(ShouldIncludeAppUpdate(arc_update));

  // Don't return non-ARC apps.
  auto non_arc_state =
      std::make_unique<apps::App>(apps::AppType::kBuiltIn, "builtin_app_id");
  apps::AppUpdate non_arc_update(non_arc_state.get(), nullptr /* delta */,
                                 EmptyAccountId());
  EXPECT_FALSE(ShouldIncludeAppUpdate(non_arc_update));
}
