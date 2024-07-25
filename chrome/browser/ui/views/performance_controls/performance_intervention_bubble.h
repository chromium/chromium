// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_PERFORMANCE_INTERVENTION_BUBBLE_H_
#define CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_PERFORMANCE_INTERVENTION_BUBBLE_H_

#include "ui/base/interaction/element_identifier.h"
#include "ui/views/bubble/bubble_border.h"

class Browser;
class PerformanceInterventionButton;
class PerformanceInterventionButtonController;

namespace views {
class BubbleDialogModelHost;
}  // namespace views

struct DialogStrings {
  std::u16string title;
  std::u16string body_text;
  std::u16string deactivate_tabs_button;
};

// This class provides the view for the performance intervention bubble dialog
// that is shown automatically when a performance issue is detected.
class PerformanceInterventionBubble {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kPerformanceInterventionDialogBody);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(
      kPerformanceInterventionDialogDismissButton);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(
      kPerformanceInterventionDialogDeactivateButton);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kPerformanceInterventionTabList);

  // Creates the performance intervention bubble dialog anchored to the
  // intervention toolbar button.
  static views::BubbleDialogModelHost* CreateBubble(
      Browser* browser,
      PerformanceInterventionButton* anchor_view,
      PerformanceInterventionButtonController* button_controller);

  // Hides performance intervention bubble dialog.
  static void CloseBubble(views::BubbleDialogModelHost*);

  static void RecordCloseReason();

 private:
  static DialogStrings GetStrings(int count);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_PERFORMANCE_INTERVENTION_BUBBLE_H_
