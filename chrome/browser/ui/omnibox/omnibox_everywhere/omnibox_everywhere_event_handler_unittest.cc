// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_event_handler.h"

#include <vector>

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_ui_manager.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace omnibox_everywhere {

class OmniboxEverywhereEventHandlerTest : public ChromeViewsTestBase {};

TEST_F(OmniboxEverywhereEventHandlerTest, DraggableRegionsAndHitTesting) {
  OmniboxEverywhereUIManager ui_manager;
  OmniboxEverywhereEventHandler event_handler(ui_manager);

  std::vector<blink::mojom::DraggableRegionPtr> regions;

  auto drag_region = blink::mojom::DraggableRegion::New();
  drag_region->bounds = gfx::Rect(0, 0, 800, 600);
  drag_region->draggable = true;
  regions.push_back(std::move(drag_region));

  auto no_drag_input_region = blink::mojom::DraggableRegion::New();
  no_drag_input_region->bounds = gfx::Rect(100, 30, 400, 50);
  no_drag_input_region->draggable = false;
  regions.push_back(std::move(no_drag_input_region));

  event_handler.UpdateNoDragRegions(regions);

  // Background point (draggable region).
  EXPECT_TRUE(event_handler.IsPointInDraggableRegion(gfx::Point(10, 10)));
  EXPECT_TRUE(event_handler.IsPointInDraggableRegion(gfx::Point(600, 40)));

  // Points inside search input (no-drag region).
  EXPECT_FALSE(event_handler.IsPointInDraggableRegion(gfx::Point(200, 40)));
  EXPECT_FALSE(event_handler.IsPointInDraggableRegion(gfx::Point(400, 50)));
}

}  // namespace omnibox_everywhere
