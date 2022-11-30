// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_hover_card_controller.h"

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"

// These are regression tests for possible crashes.

class TabHoverCardControllerTest : public TestWithBrowserView {};

TEST_F(TabHoverCardControllerTest, ShowWrongTabDoesntCrash) {
  auto controller =
      std::make_unique<TabHoverCardController>(browser_view()->tabstrip());
  // Create some completely invalid pointer values (these should never be
  // dereferenced).
  Tab* const tab1 = reinterpret_cast<Tab*>(3);
  Tab* const tab2 = reinterpret_cast<Tab*>(7);
  controller->target_tab_ = tab1;
  // If the safeguard is not in place, this will crash because the target tab is
  // not a valid pointer.
  controller->ShowHoverCard(false, tab2);
}

TEST_F(TabHoverCardControllerTest, SetPreviewWithNoHoverCardDoesntCrash) {
  auto controller =
      std::make_unique<TabHoverCardController>(browser_view()->tabstrip());
  // If the safeguard is not in place, this could crash in either metrics
  // collection *or* in trying to set the actual thumbnail image on the card.
  controller->OnPreviewImageAvaialble(controller->thumbnail_observer_.get(),
                                      gfx::ImageSkia());
}
