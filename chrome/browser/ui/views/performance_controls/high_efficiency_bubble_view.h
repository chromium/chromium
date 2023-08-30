// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_HIGH_EFFICIENCY_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_HIGH_EFFICIENCY_BUBBLE_VIEW_H_

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/performance_controls/high_efficiency_bubble_observer.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/view.h"

class HighEfficiencyBubbleView {
 public:
  HighEfficiencyBubbleView(const HighEfficiencyBubbleView&) = delete;
  HighEfficiencyBubbleView& operator=(const HighEfficiencyBubbleView&) = delete;

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kHighEfficiencyDialogBodyElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(
      kHighEfficiencyDialogResourceViewElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kHighEfficiencyDialogOkButton);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kHighEfficiencyDialogCancelButton);

  static views::BubbleDialogModelHost* ShowBubble(
      Browser* browser,
      views::View* anchor_view,
      HighEfficiencyBubbleObserver* observer);

 private:
  HighEfficiencyBubbleView() = default;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_HIGH_EFFICIENCY_BUBBLE_VIEW_H_
