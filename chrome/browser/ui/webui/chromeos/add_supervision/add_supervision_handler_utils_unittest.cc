// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision_handler_utils.h"
#include "chrome/services/app_service/public/cpp/app_update.h"
#include "testing/gtest/include/gtest/gtest.h"

using AddSupervisionHandlerUtilsTest = testing::Test;

// Tests that only the right apps are returned via the API.
TEST_F(AddSupervisionHandlerUtilsTest, TestShouldIncludeAppUpdate) {
  // Return ARC apps.
  apps::mojom::App arc_state;
  arc_state.app_type = apps::mojom::AppType::kArc;
  apps::AppUpdate arc_update(&arc_state, nullptr /* delta */);
  EXPECT_TRUE(ShouldIncludeAppUpdate(arc_update));

  // Don't return non-ARC apps.
  apps::mojom::App non_arc_state;
  non_arc_state.app_type = apps::mojom::AppType::kBuiltIn;
  apps::AppUpdate non_arc_update(&non_arc_state, nullptr /* delta */);
  EXPECT_FALSE(ShouldIncludeAppUpdate(non_arc_update));
}
