// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/mahi/mahi_menu_view.h"

#include <memory>
#include <string>

#include "chrome/browser/ui/views/editor_menu/utils/utils.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace chromeos::mahi {

using MahiMenuViewTest = ChromeViewsTestBase;

TEST_F(MahiMenuViewTest, Bounds) {
  const gfx::Rect anchor_view_bounds = gfx::Rect(50, 50, 25, 100);
  auto menu_widget = MahiMenuView::CreateWidget(anchor_view_bounds);

  // The bounds of the created widget should be similar to the value from the
  // utils function.
  EXPECT_EQ(editor_menu::GetEditorMenuBounds(
                anchor_view_bounds, menu_widget.get()->GetContentsView()),
            menu_widget->GetRestoredBounds());
}

}  // namespace chromeos::mahi
