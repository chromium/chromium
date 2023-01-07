// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_BATTERY_SAVER_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_BATTERY_SAVER_BUBBLE_VIEW_H_

#include "ui/views/bubble/bubble_border.h"

class Browser;
class BatterySaverBubbleObserver;

namespace views {
class BubbleDialogModelHost;
class View;
}  // namespace views

// This class provides the view for the bubble dialog that is shown to the user
// when the battery saver toolbar button is clicked.
class BatterySaverBubbleView {
 public:
  // Creates the battery saver bubble dialog anchored to the specified view.
  static views::BubbleDialogModelHost* CreateBubble(
      Browser* browser,
      views::View* anchor_view,
      views::BubbleBorder::Arrow anchor_position,
      BatterySaverBubbleObserver* observer);

  // Hides the battery saver bubble dialog.
  static void CloseBubble(views::BubbleDialogModelHost*);

  static const char kViewClassName[];
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_BATTERY_SAVER_BUBBLE_VIEW_H_
