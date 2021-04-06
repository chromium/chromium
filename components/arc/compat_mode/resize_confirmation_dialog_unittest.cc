// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/compat_mode/resize_confirmation_dialog.h"

#include "base/callback_helpers.h"
#include "components/exo/test/exo_test_base_views.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/layout/layout_provider.h"

namespace arc {
namespace {

using ResizeConfirmationDialogTest = exo::test::ExoTestBaseViews;

// Test that ShowResizeConfirmationDialog does not crash
TEST_F(ResizeConfirmationDialogTest, ShowAndCloseDialog) {
  // A LayoutProvider must exist in scope in order to set up views.
  views::LayoutProvider layout_provider;

  auto* widget = ShowResizeConfirmationDialog(nullptr, base::DoNothing());
  RunPendingMessages();
  widget->Close();
  RunPendingMessages();
}

}  // namespace
}  // namespace arc
